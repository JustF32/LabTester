#include "services/CloudSyncService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSqlError>
#include <QSqlField>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStringList>
#include <QTimer>
#include <QUuid>
#include <QUrl>
#include <QtSql/QSqlDatabase>

#include <algorithm>
#include <cmath>

#include "database/DatabaseManager.h"

namespace {

const QString kSettingCloudUsername = QStringLiteral("cloud.username");
const QString kSettingCloudPassword = QStringLiteral("cloud.password");
const QString kSettingCloudAutoSync = QStringLiteral("cloud.auto_sync");
const QString kSettingCloudLocalVersion = QStringLiteral("cloud.local_snapshot_version");
const QString kSettingRemoteEndpoint = QStringLiteral("execution.remote.endpoint");
const QString kSettingRemoteToken = QStringLiteral("execution.remote.token");

const QString kDefaultRemoteEndpoint = QStringLiteral("");
const QString kDefaultRemoteToken = QStringLiteral("");

const QString kDefaultCloudHost = QStringLiteral("");
const int kDefaultCloudPort = 0;
const QString kDefaultCloudDb = QStringLiteral("");
const QString kDefaultCloudUser = QStringLiteral("");
const QString kDefaultCloudPassword = QStringLiteral("");

const QStringList kSnapshotTablesOrdered {
    QStringLiteral("student_groups"),
    QStringLiteral("students"),
    QStringLiteral("lab_works"),
    QStringLiteral("execution_targets"),
    QStringLiteral("submission_entries"),
    QStringLiteral("check_runs"),
    QStringLiteral("check_case_results"),
    QStringLiteral("sync_queue"),
};

const QStringList kSnapshotTablesClearOrder {
    QStringLiteral("check_case_results"),
    QStringLiteral("check_runs"),
    QStringLiteral("submission_entries"),
    QStringLiteral("sync_queue"),
    QStringLiteral("execution_targets"),
    QStringLiteral("lab_works"),
    QStringLiteral("students"),
    QStringLiteral("student_groups"),
};

QString repairUtf8Mojibake(const QString &value);

bool startsWithToken(const QString &text, qsizetype pos, const QString &token)
{
    if (pos < 0 || pos + token.size() > text.size()) {
        return false;
    }
    return text.mid(pos, token.size()).compare(token, Qt::CaseInsensitive) == 0;
}

bool isJsonTokenBoundary(QChar ch)
{
    return ch.isSpace() || ch == QLatin1Char(',') || ch == QLatin1Char('}')
        || ch == QLatin1Char(']') || ch == QLatin1Char(':');
}

QString replaceInvalidJsonNumbers(const QString &jsonText)
{
    QString result;
    result.reserve(jsonText.size());

    bool inString = false;
    bool escaped = false;
    for (qsizetype i = 0; i < jsonText.size();) {
        const QChar ch = jsonText.at(i);
        if (inString) {
            result.append(ch);
            if (escaped) {
                escaped = false;
            } else if (ch == QLatin1Char('\\')) {
                escaped = true;
            } else if (ch == QLatin1Char('"')) {
                inString = false;
            }
            ++i;
            continue;
        }

        if (ch == QLatin1Char('"')) {
            inString = true;
            result.append(ch);
            ++i;
            continue;
        }

        static const QStringList invalidNumberTokens {
            QStringLiteral("-Infinity"),
            QStringLiteral("+Infinity"),
            QStringLiteral("Infinity"),
            QStringLiteral("-1.#IND"),
            QStringLiteral("+1.#IND"),
            QStringLiteral("1.#IND"),
            QStringLiteral("-1.#INF"),
            QStringLiteral("+1.#INF"),
            QStringLiteral("1.#INF"),
            QStringLiteral("-NaN"),
            QStringLiteral("+NaN"),
            QStringLiteral("NaN"),
            QStringLiteral("-inf"),
            QStringLiteral("+inf"),
            QStringLiteral("inf"),
        };

        bool replaced = false;
        for (const QString &token : invalidNumberTokens) {
            if (!startsWithToken(jsonText, i, token)) {
                continue;
            }
            const qsizetype before = i - 1;
            const qsizetype after = i + token.size();
            const bool leftOk = before < 0 || isJsonTokenBoundary(jsonText.at(before));
            const bool rightOk = after >= jsonText.size() || isJsonTokenBoundary(jsonText.at(after));
            if (!leftOk || !rightOk) {
                continue;
            }
            result.append(QStringLiteral("null"));
            i += token.size();
            replaced = true;
            break;
        }
        if (replaced) {
            continue;
        }

        result.append(ch);
        ++i;
    }

    return result;
}

QJsonDocument parseJsonDocumentWithRepair(
    const QByteArray &raw,
    QJsonParseError *parseError,
    bool *repaired = nullptr
)
{
    QJsonParseError firstError;
    QJsonDocument document = QJsonDocument::fromJson(raw, &firstError);
    if (firstError.error == QJsonParseError::NoError) {
        if (parseError) {
            *parseError = firstError;
        }
        if (repaired) {
            *repaired = false;
        }
        return document;
    }

    if (firstError.error != QJsonParseError::IllegalNumber) {
        if (parseError) {
            *parseError = firstError;
        }
        if (repaired) {
            *repaired = false;
        }
        return document;
    }

    const QString repairedText = replaceInvalidJsonNumbers(QString::fromUtf8(raw));
    if (repairedText.toUtf8() == raw) {
        if (parseError) {
            *parseError = firstError;
        }
        if (repaired) {
            *repaired = false;
        }
        return document;
    }

    QJsonParseError repairedError;
    document = QJsonDocument::fromJson(repairedText.toUtf8(), &repairedError);
    if (parseError) {
        *parseError = repairedError.error == QJsonParseError::NoError ? repairedError : firstError;
    }
    if (repaired) {
        *repaired = repairedError.error == QJsonParseError::NoError;
    }
    return repairedError.error == QJsonParseError::NoError ? document : QJsonDocument();
}

QJsonValue variantToJson(const QVariant &value)
{
    if (!value.isValid() || value.isNull()) {
        return QJsonValue::Null;
    }

    switch (value.typeId()) {
    case QMetaType::Bool:
        return QJsonValue(value.toBool());
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Short:
    case QMetaType::UShort:
    case QMetaType::Float:
    case QMetaType::Double:
    {
        const double number = value.toDouble();
        if (!std::isfinite(number)) {
            return QJsonValue::Null;
        }
        return QJsonValue(number);
    }
    default:
        break;
    }

    if (value.canConvert<QDateTime>()) {
        const QDateTime dateTime = value.toDateTime();
        if (dateTime.isValid()) {
            return QJsonValue(dateTime.toString(Qt::ISODateWithMs));
        }
    }

    return QJsonValue(repairUtf8Mojibake(value.toString()));
}

QVariant jsonToVariant(const QJsonValue &value)
{
    if (value.isNull() || value.isUndefined()) {
        return {};
    }
    if (value.isBool()) {
        return value.toBool();
    }
    if (value.isDouble()) {
        const double number = value.toDouble();
        const qint64 intNumber = static_cast<qint64>(number);
        if (static_cast<double>(intNumber) == number) {
            return intNumber;
        }
        return number;
    }
    if (value.isString()) {
        return repairUtf8Mojibake(value.toString());
    }
    if (value.isArray()) {
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    }
    if (value.isObject()) {
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    }
    return {};
}

QStringList tableColumns(QSqlDatabase &db, const QString &tableName)
{
    QStringList columns;
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(tableName))) {
        return columns;
    }
    while (query.next()) {
        columns.push_back(query.value(1).toString());
    }
    return columns;
}

bool tableExists(QSqlDatabase &db, const QString &tableName)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT COUNT(1) FROM sqlite_master WHERE type='table' AND name=:name"
    ));
    query.bindValue(QStringLiteral(":name"), tableName);
    if (!query.exec() || !query.next()) {
        return false;
    }
    return query.value(0).toInt() > 0;
}

QString defaultAppVersion()
{
    const QString appVersion = QCoreApplication::applicationVersion().trimmed();
    return appVersion.isEmpty() ? QStringLiteral("0.2v") : appVersion;
}

int countCyrillicLetters(const QString &value)
{
    int total = 0;
    for (const QChar ch : value) {
        const ushort code = ch.unicode();
        if ((code >= 0x0400 && code <= 0x04FF) || (code >= 0x0500 && code <= 0x052F)) {
            ++total;
        }
    }
    return total;
}

int countMojibakeMarkers(const QString &value)
{
    int markers = 0;
    for (const QChar ch : value) {
        const ushort code = ch.unicode();
        if (code == 0x00D0 || code == 0x00D1 || code == 0x00D2 || code == 0x00D3
            || code == 0x00C2 || code == 0x00C3) {
            ++markers;
        }
    }
    return markers;
}

int textQualityScore(const QString &value)
{
    int score = 0;
    for (const QChar ch : value) {
        const ushort code = ch.unicode();
        if ((code >= 0x0400 && code <= 0x052F) || ch.isDigit() || ch.isSpace()) {
            score += 3;
        } else if (ch.isLetter() || ch.isPunct()) {
            score += 1;
        }
        if (ch == QChar::ReplacementCharacter) {
            score -= 12;
        }
    }
    score -= countMojibakeMarkers(value) * 4;
    return score;
}

QString decodeUtf8FromLatin1(const QString &value)
{
    return QString::fromUtf8(value.toLatin1());
}

QString repairUtf8Mojibake(const QString &value)
{
    if (value.trimmed().isEmpty()) {
        return value;
    }

    QString best = value;
    int bestScore = textQualityScore(best);

    QString candidate = value;
    for (int pass = 0; pass < 3; ++pass) {
        const QString decoded = decodeUtf8FromLatin1(candidate);
        if (decoded == candidate) {
            break;
        }
        const int decodedScore = textQualityScore(decoded);
        if (decodedScore > bestScore) {
            best = decoded;
            bestScore = decodedScore;
        }
        candidate = decoded;
    }

    if (countCyrillicLetters(best) >= countCyrillicLetters(value)
        && bestScore >= textQualityScore(value)) {
        return best;
    }

    return value;
}

void configureRemoteConnectionEncoding(QSqlDatabase &remoteDb)
{
    QSqlQuery query(remoteDb);
    if (!query.exec(QStringLiteral("SET client_encoding TO 'UTF8'"))) {
        qWarning() << "[CloudSync] Failed to set UTF8 client encoding:" << query.lastError().text();
    }
}

struct CloudDbConfig {
    QString host;
    int port {0};
    QString database;
    QString user;
    QString password;
};

CloudDbConfig resolveCloudDbConfig()
{
    CloudDbConfig config;

    const QString envHost = QString::fromLocal8Bit(qgetenv("LABTESTER_CLOUD_HOST")).trimmed();
    config.host = envHost.isEmpty() ? kDefaultCloudHost : envHost;

    const QString envPort = QString::fromLocal8Bit(qgetenv("LABTESTER_CLOUD_PORT")).trimmed();
    bool ok = false;
    const int parsedPort = envPort.toInt(&ok);
    config.port = ok && parsedPort > 0 ? parsedPort : kDefaultCloudPort;

    const QString envDb = QString::fromLocal8Bit(qgetenv("LABTESTER_CLOUD_DB")).trimmed();
    config.database = envDb.isEmpty() ? kDefaultCloudDb : envDb;

    const QString envUser = QString::fromLocal8Bit(qgetenv("LABTESTER_CLOUD_USER")).trimmed();
    config.user = envUser.isEmpty() ? kDefaultCloudUser : envUser;

    const QString envPassword = QString::fromLocal8Bit(qgetenv("LABTESTER_CLOUD_PASSWORD")).trimmed();
    config.password = envPassword.isEmpty() ? kDefaultCloudPassword : envPassword;

    return config;
}

bool isCloudDbConfigComplete(const CloudDbConfig &config)
{
    return !config.host.isEmpty()
        && config.port > 0
        && !config.database.isEmpty()
        && !config.user.isEmpty()
        && !config.password.isEmpty();
}

QString normalizeEndpoint(QString endpoint)
{
    endpoint = endpoint.trimmed();
    if (endpoint.isEmpty()) {
        return QString();
    }
    if (!endpoint.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
        && !endpoint.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
        endpoint.prepend(QStringLiteral("http://"));
    }
    while (endpoint.endsWith('/')) {
        endpoint.chop(1);
    }
    return endpoint;
}

bool httpJsonRequest(
    const QString &endpoint,
    const QString &token,
    const QString &path,
    const QString &method,
    const QJsonObject &payload,
    QJsonObject *response,
    QString *errorMessage
)
{
    const QString normalizedEndpoint = normalizeEndpoint(endpoint);
    if (normalizedEndpoint.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Адрес сервера синхронизации не задан.");
        }
        return false;
    }
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(normalizedEndpoint + path));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!token.trimmed().isEmpty()) {
        request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(token.trimmed()).toUtf8());
    }

    QNetworkReply *reply = nullptr;
    const QByteArray body = method == QStringLiteral("GET")
        ? QByteArray()
        : QJsonDocument(payload).toJson(QJsonDocument::Compact);
    if (method == QStringLiteral("GET")) {
        reply = manager.get(request);
    } else if (method == QStringLiteral("POST")) {
        reply = manager.post(request, body);
    } else {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Неподдерживаемый HTTP-метод: %1").arg(method);
        }
        return false;
    }

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(15000);
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        reply->deleteLater();
        if (errorMessage) {
            *errorMessage = QStringLiteral("Сервер синхронизации не ответил за 15 секунд.");
        }
        return false;
    }

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray raw = reply->readAll();
    const QString transportError = reply->error() == QNetworkReply::NoError
        ? QString()
        : reply->errorString();
    reply->deleteLater();

    QJsonParseError parseError;
    bool repairedJson = false;
    const QJsonDocument document = parseJsonDocumentWithRepair(raw, &parseError, &repairedJson);
    const QJsonObject object = document.isObject() ? document.object() : QJsonObject();
    if (response) {
        *response = object;
    }

    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        const QString rawText = QString::fromUtf8(raw).trimmed();
        const bool looksLikeExpiredTunnel = rawText.contains(QStringLiteral("No session avaliable"), Qt::CaseInsensitive)
            || rawText.contains(QStringLiteral("No session available"), Qt::CaseInsensitive);
        if (looksLikeExpiredTunnel && !kDefaultRemoteEndpoint.isEmpty()
            && normalizedEndpoint != kDefaultRemoteEndpoint) {
            qWarning().noquote() << QStringLiteral(
                "[CloudSync] Stored endpoint returned expired tunnel response. Retrying default worker endpoint: %1"
            ).arg(kDefaultRemoteEndpoint);
            return httpJsonRequest(
                kDefaultRemoteEndpoint,
                token,
                path,
                method,
                payload,
                response,
                errorMessage
            );
        }

        if (errorMessage) {
            QString preview = QString::fromUtf8(raw.left(240)).trimmed();
            preview.replace(QLatin1Char('\r'), QLatin1Char(' '));
            preview.replace(QLatin1Char('\n'), QLatin1Char(' '));
            *errorMessage = QStringLiteral("Некорректный JSON от сервера синхронизации: %1")
                .arg(parseError.errorString());
            if (!preview.isEmpty()) {
                *errorMessage += QStringLiteral(". Начало ответа: %1").arg(preview);
            }
        }
        return false;
    }

    if (repairedJson) {
        qWarning() << "[CloudSync] Repaired non-standard JSON numbers in sync server response.";
    }

    if (statusCode < 200 || statusCode >= 300 || object.value(QStringLiteral("ok")).toBool(false) == false) {
        if (errorMessage) {
            const QString serverError = object.value(QStringLiteral("error")).toString().trimmed();
            *errorMessage = !serverError.isEmpty()
                ? serverError
                : QStringLiteral("HTTP %1: %2").arg(statusCode).arg(transportError);
        }
        return false;
    }

    return true;
}

} // namespace

namespace labtester::services {

CloudSyncService::CloudSyncService(database::DatabaseManager &databaseManager)
    : m_databaseManager(databaseManager)
{
}

QString CloudSyncService::cloudUsername() const
{
    return settingValue(kSettingCloudUsername).trimmed();
}

bool CloudSyncService::setCloudUsername(const QString &username, QString *errorMessage)
{
    return setSettingValue(kSettingCloudUsername, username.trimmed(), errorMessage);
}

QString CloudSyncService::cloudPassword() const
{
    return settingValue(kSettingCloudPassword).trimmed();
}

bool CloudSyncService::setCloudPassword(const QString &password, QString *errorMessage)
{
    return setSettingValue(kSettingCloudPassword, password.trimmed(), errorMessage);
}

bool CloudSyncService::autoSyncEnabled() const
{
    const QString value = settingValue(kSettingCloudAutoSync, QStringLiteral("0")).trimmed().toLower();
    return value == QStringLiteral("1")
        || value == QStringLiteral("true")
        || value == QStringLiteral("yes");
}

bool CloudSyncService::setAutoSyncEnabled(bool enabled, QString *errorMessage)
{
    return setSettingValue(kSettingCloudAutoSync, enabled ? QStringLiteral("1") : QStringLiteral("0"), errorMessage);
}

QVariantMap CloudSyncService::versionInfo(const QString &usernameOverride, bool includeRemote) const
{
    QVariantMap map;

    const QString username = resolvedUsername(usernameOverride);
    const qint64 localVersion = localSnapshotVersion();
    const int localSchema = localSchemaVersion();

    map.insert(QStringLiteral("username"), username);
    map.insert(QStringLiteral("localAppVersion"), defaultAppVersion());
    map.insert(QStringLiteral("localVersion"), localVersion);
    map.insert(QStringLiteral("localSchemaVersion"), localSchema);
    map.insert(QStringLiteral("localSubmissionCount"), localSubmissionCount());
    map.insert(QStringLiteral("localLabWorkCount"), localLabWorkCount());
    map.insert(QStringLiteral("localHistoryCount"), localHistoryCount());
    map.insert(QStringLiteral("remoteExists"), false);
    map.insert(QStringLiteral("remoteVersion"), 0);
    map.insert(QStringLiteral("remoteSchemaVersion"), 0);
    map.insert(QStringLiteral("remoteSubmissionCount"), 0);
    map.insert(QStringLiteral("remoteLabWorkCount"), 0);
    map.insert(QStringLiteral("remoteHistoryCount"), 0);
    map.insert(QStringLiteral("remoteUpdatedAt"), QString());
    map.insert(QStringLiteral("remoteAppVersion"), QString());
    map.insert(QStringLiteral("remoteUsername"), username);
    map.insert(QStringLiteral("remoteNewer"), false);
    map.insert(QStringLiteral("remoteSchemaNewer"), false);
    map.insert(QStringLiteral("error"), QString());

    if (!includeRemote) {
        return map;
    }

    if (username.isEmpty()) {
        return map;
    }

    const QString password = resolvedPassword(QString());
    if (password.isEmpty()) {
        map.insert(QStringLiteral("error"), QStringLiteral("Введите пароль аккаунта."));
        return map;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("username"), username);
    payload.insert(QStringLiteral("password"), password);

    QJsonObject response;
    QString error;
    if (!httpJsonRequest(
            settingValue(kSettingRemoteEndpoint, kDefaultRemoteEndpoint),
            settingValue(kSettingRemoteToken, kDefaultRemoteToken),
            QStringLiteral("/sync/meta"),
            QStringLiteral("POST"),
            payload,
            &response,
            &error)) {
        map.insert(QStringLiteral("error"), error);
        return map;
    }

    map.insert(QStringLiteral("remoteExists"), response.value(QStringLiteral("exists")).toBool(false));
    map.insert(QStringLiteral("remoteVersion"), static_cast<qint64>(response.value(QStringLiteral("snapshotVersion")).toDouble(0)));
    map.insert(QStringLiteral("remoteSchemaVersion"), response.value(QStringLiteral("schemaVersion")).toInt(0));
    map.insert(QStringLiteral("remoteSubmissionCount"), response.value(QStringLiteral("submissionCount")).toInt(0));
    map.insert(QStringLiteral("remoteLabWorkCount"), response.value(QStringLiteral("labWorkCount")).toInt(0));
    map.insert(QStringLiteral("remoteHistoryCount"), response.value(QStringLiteral("historyCount")).toInt(0));
    map.insert(QStringLiteral("remoteUpdatedAt"), response.value(QStringLiteral("updatedAt")).toString());
    map.insert(QStringLiteral("remoteAppVersion"), response.value(QStringLiteral("appVersion")).toString());
    map.insert(QStringLiteral("remoteNewer"), map.value(QStringLiteral("remoteVersion")).toLongLong() > localVersion);
    map.insert(QStringLiteral("remoteSchemaNewer"), map.value(QStringLiteral("remoteSchemaVersion")).toInt() > localSchema);

    return map;
}

QVariantList CloudSyncService::remoteAccounts(QString *errorMessage) const
{
    QVariantList accounts;
    QJsonObject response;
    if (!httpJsonRequest(
            settingValue(kSettingRemoteEndpoint, kDefaultRemoteEndpoint),
            settingValue(kSettingRemoteToken, kDefaultRemoteToken),
            QStringLiteral("/sync/accounts"),
            QStringLiteral("GET"),
            {},
            &response,
            errorMessage)) {
        return accounts;
    }

    const QJsonArray items = response.value(QStringLiteral("accounts")).toArray();
    for (const QJsonValue &value : items) {
        const QJsonObject object = value.toObject();
        QVariantMap item;
        item.insert(QStringLiteral("username"), object.value(QStringLiteral("username")).toString());
        item.insert(QStringLiteral("updatedAt"), object.value(QStringLiteral("updatedAt")).toString());
        item.insert(QStringLiteral("submissionCount"), object.value(QStringLiteral("submissionCount")).toInt(0));
        item.insert(QStringLiteral("labWorkCount"), object.value(QStringLiteral("labWorkCount")).toInt(0));
        item.insert(QStringLiteral("snapshotVersion"), static_cast<qint64>(object.value(QStringLiteral("snapshotVersion")).toDouble(0)));
        accounts.push_back(item);
    }
    return accounts;
}

QVariantList CloudSyncService::remoteSaveHistory(
    const QString &usernameOverride,
    QString *errorMessage
) const
{
    QVariantList history;

    QString driverError;
    if (!ensureCloudDriverAvailable(&driverError)) {
        if (errorMessage) {
            *errorMessage = driverError;
        }
        return history;
    }

    const QString username = resolvedUsername(usernameOverride);
    if (username.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Укажите имя пользователя.");
        }
        return history;
    }

    const CloudDbConfig cloudConfig = resolveCloudDbConfig();
    if (!isCloudDbConfigComplete(cloudConfig)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                "Настройки облачной БД не заданы. Укажите LABTESTER_CLOUD_HOST, "
                "LABTESTER_CLOUD_PORT, LABTESTER_CLOUD_DB, LABTESTER_CLOUD_USER и LABTESTER_CLOUD_PASSWORD."
            );
        }
        return history;
    }

    const QString connectionName = QStringLiteral("LabTesterCloudHistory_%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase remoteDb = QSqlDatabase::addDatabase(QStringLiteral("QPSQL"), connectionName);
        remoteDb.setHostName(cloudConfig.host);
        remoteDb.setPort(cloudConfig.port);
        remoteDb.setDatabaseName(cloudConfig.database);
        remoteDb.setUserName(cloudConfig.user);
        remoteDb.setPassword(cloudConfig.password);
        remoteDb.setConnectOptions(QStringLiteral("connect_timeout=5"));

        if (!remoteDb.open()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Нет подключения к облачной БД: %1").arg(remoteDb.lastError().text());
            }
        } else {
            configureRemoteConnectionEncoding(remoteDb);
            QString schemaError;
            if (!ensureRemoteSchema(remoteDb, &schemaError)) {
                if (errorMessage) {
                    *errorMessage = schemaError;
                }
            } else {
                QSqlQuery query(remoteDb);
                query.prepare(QStringLiteral(
                    "SELECT snapshot_version, schema_version, app_version, "
                    "to_char(updated_at, 'YYYY-MM-DD\"T\"HH24:MI:SS') "
                    "FROM cloud_snapshot_history "
                    "WHERE username = :username "
                    "ORDER BY id DESC "
                    "LIMIT 200"
                ));
                query.bindValue(QStringLiteral(":username"), username);
                if (!query.exec()) {
                    if (errorMessage) {
                        *errorMessage = QStringLiteral("Не удалось загрузить историю сохранений: %1")
                            .arg(query.lastError().text());
                    }
                } else {
                    while (query.next()) {
                        QVariantMap item;
                        item.insert(QStringLiteral("username"), username);
                        item.insert(QStringLiteral("snapshotVersion"), query.value(0).toLongLong());
                        item.insert(QStringLiteral("schemaVersion"), query.value(1).toInt());
                        item.insert(QStringLiteral("appVersion"), repairUtf8Mojibake(query.value(2).toString()));
                        item.insert(QStringLiteral("updatedAt"), query.value(3).toString());
                        history.push_back(item);
                    }
                }
            }
        }

        remoteDb.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
    return history;
}

bool CloudSyncService::createAccount(const QString &username, const QString &password, QString *outMessage)
{
    const QString normalizedUsername = username.trimmed();
    const QString normalizedPassword = password.trimmed();
    if (normalizedUsername.isEmpty() || normalizedPassword.isEmpty()) {
        if (outMessage) {
            *outMessage = QStringLiteral("Введите имя аккаунта и пароль.");
        }
        return false;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("username"), normalizedUsername);
    payload.insert(QStringLiteral("password"), normalizedPassword);

    QJsonObject response;
    QString error;
    const bool ok = httpJsonRequest(
        settingValue(kSettingRemoteEndpoint, kDefaultRemoteEndpoint),
        settingValue(kSettingRemoteToken, kDefaultRemoteToken),
        QStringLiteral("/sync/account"),
        QStringLiteral("POST"),
        payload,
        &response,
        &error
    );
    if (!ok) {
        if (outMessage) {
            *outMessage = error;
        }
        return false;
    }

    QString settingsError;
    setCloudUsername(normalizedUsername, &settingsError);
    setCloudPassword(normalizedPassword, &settingsError);

    if (outMessage) {
        *outMessage = QStringLiteral("Аккаунт '%1' создан.").arg(normalizedUsername);
    }
    return true;
}

bool CloudSyncService::syncToCloud(const QString &usernameOverride, const QString &password, QString *outMessage)
{
    const QString username = resolvedUsername(usernameOverride);
    const QString accountPassword = resolvedPassword(password);
    if (username.isEmpty()) {
        if (outMessage) {
            *outMessage = QStringLiteral("Укажите имя пользователя для синхронизации.");
        }
        return false;
    }
    if (accountPassword.isEmpty()) {
        if (outMessage) {
            *outMessage = QStringLiteral("Введите пароль аккаунта.");
        }
        return false;
    }

    const int localSchema = localSchemaVersion();
    if (localSchema < 0) {
        if (outMessage) {
            *outMessage = QStringLiteral("Не удалось прочитать локальную версию схемы.");
        }
        return false;
    }

    QString snapshotError;
    const QString snapshotJson = exportLocalSnapshot(&snapshotError);
    if (!snapshotError.isEmpty() || snapshotJson.isEmpty()) {
        if (outMessage) {
            *outMessage = snapshotError.isEmpty()
                ? QStringLiteral("Не удалось собрать локальный снимок данных.")
                : snapshotError;
        }
        return false;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("username"), username);
    payload.insert(QStringLiteral("password"), accountPassword);
    payload.insert(QStringLiteral("snapshotJson"), snapshotJson);
    payload.insert(QStringLiteral("schemaVersion"), localSchema);
    payload.insert(QStringLiteral("appVersion"), defaultAppVersion());
    payload.insert(QStringLiteral("submissionCount"), localSubmissionCount());
    payload.insert(QStringLiteral("labWorkCount"), localLabWorkCount());
    payload.insert(QStringLiteral("historyCount"), localHistoryCount());

    QJsonObject response;
    QString error;
    const bool ok = httpJsonRequest(
        settingValue(kSettingRemoteEndpoint, kDefaultRemoteEndpoint),
        settingValue(kSettingRemoteToken, kDefaultRemoteToken),
        QStringLiteral("/sync/push"),
        QStringLiteral("POST"),
        payload,
        &response,
        &error
    );
    if (!ok) {
        if (outMessage) {
            *outMessage = error;
        }
        return false;
    }

    const qint64 version = static_cast<qint64>(response.value(QStringLiteral("snapshotVersion")).toDouble(0));
    QString settingsError;
    setLocalSnapshotVersion(version, &settingsError);
    setCloudUsername(username, &settingsError);
    setCloudPassword(accountPassword, &settingsError);

    if (outMessage) {
        *outMessage = QStringLiteral("Выгрузка завершена. Аккаунт: %1, работ на сервере: %2.")
            .arg(username)
            .arg(response.value(QStringLiteral("submissionCount")).toInt(localSubmissionCount()));
    }
    return true;
}

bool CloudSyncService::syncFromCloud(const QString &usernameOverride, const QString &password, QString *outMessage)
{
    const QString username = resolvedUsername(usernameOverride);
    const QString accountPassword = resolvedPassword(password);
    if (username.isEmpty()) {
        if (outMessage) {
            *outMessage = QStringLiteral("Укажите имя пользователя для синхронизации.");
        }
        return false;
    }
    if (accountPassword.isEmpty()) {
        if (outMessage) {
            *outMessage = QStringLiteral("Введите пароль аккаунта.");
        }
        return false;
    }

    const int localSchema = localSchemaVersion();
    if (localSchema < 0) {
        if (outMessage) {
            *outMessage = QStringLiteral("Не удалось прочитать локальную версию схемы.");
        }
        return false;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("username"), username);
    payload.insert(QStringLiteral("password"), accountPassword);

    QJsonObject response;
    QString error;
    const bool ok = httpJsonRequest(
        settingValue(kSettingRemoteEndpoint, kDefaultRemoteEndpoint),
        settingValue(kSettingRemoteToken, kDefaultRemoteToken),
        QStringLiteral("/sync/pull"),
        QStringLiteral("POST"),
        payload,
        &response,
        &error
    );
    if (!ok) {
        if (outMessage) {
            *outMessage = error;
        }
        return false;
    }

    if (!response.value(QStringLiteral("exists")).toBool(false)) {
        if (outMessage) {
            *outMessage = QStringLiteral("На сервере пока нет данных для аккаунта '%1'.").arg(username);
        }
        return false;
    }

    const int remoteSchema = response.value(QStringLiteral("schemaVersion")).toInt(0);
    if (remoteSchema > localSchema) {
        if (outMessage) {
            *outMessage = QStringLiteral("На сервере версия схемы новее (%1 > %2). Обновите приложение.")
                .arg(remoteSchema)
                .arg(localSchema);
        }
        return false;
    }

    QString importError;
    if (!importLocalSnapshot(response.value(QStringLiteral("snapshotJson")).toString(), &importError)) {
        if (outMessage) {
            *outMessage = importError;
        }
        return false;
    }

    const qint64 version = static_cast<qint64>(response.value(QStringLiteral("snapshotVersion")).toDouble(0));
    QString settingsError;
    setLocalSnapshotVersion(version, &settingsError);
    setCloudUsername(username, &settingsError);
    setCloudPassword(accountPassword, &settingsError);

    if (outMessage) {
        *outMessage = QStringLiteral("Загрузка завершена. Аккаунт: %1, локально работ: %2.")
            .arg(username)
            .arg(response.value(QStringLiteral("submissionCount")).toInt(0));
    }
    return true;
}

bool CloudSyncService::autoSyncOnStartup(QString *outMessage)
{
    if (!autoSyncEnabled()) {
        if (outMessage) {
            *outMessage = QStringLiteral("Автосинхронизация отключена.");
        }
        return true;
    }

    const QString username = cloudUsername();
    if (username.isEmpty()) {
        if (outMessage) {
            *outMessage = QStringLiteral("Автосинхронизация пропущена: не указан пользователь.");
        }
        return true;
    }

    return syncFromCloud(username, QString(), outMessage);
}

bool CloudSyncService::autoSyncOnShutdown(QString *outMessage)
{
    if (!autoSyncEnabled()) {
        if (outMessage) {
            *outMessage = QStringLiteral("Автосинхронизация отключена.");
        }
        return true;
    }

    const QString username = cloudUsername();
    if (username.isEmpty()) {
        if (outMessage) {
            *outMessage = QStringLiteral("Автосинхронизация пропущена: не указан пользователь.");
        }
        return true;
    }

    return syncToCloud(username, QString(), outMessage);
}

QString CloudSyncService::settingValue(const QString &key, const QString &fallback) const
{
    QSqlQuery query(m_databaseManager.database());
    query.prepare(QStringLiteral("SELECT value FROM settings WHERE key = :key LIMIT 1"));
    query.bindValue(QStringLiteral(":key"), key);
    if (!query.exec() || !query.next()) {
        return fallback;
    }
    return query.value(0).toString();
}

bool CloudSyncService::setSettingValue(const QString &key, const QString &value, QString *errorMessage) const
{
    QSqlQuery query(m_databaseManager.database());
    query.prepare(QStringLiteral(
        "INSERT INTO settings(key, value, updated_at) "
        "VALUES(:key, :value, CURRENT_TIMESTAMP) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value, updated_at = CURRENT_TIMESTAMP"
    ));
    query.bindValue(QStringLiteral(":key"), key);
    query.bindValue(QStringLiteral(":value"), value);

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось сохранить настройку '%1': %2")
                .arg(key, query.lastError().text());
        }
        return false;
    }
    return true;
}

int CloudSyncService::localSchemaVersion() const
{
    QSqlQuery query(m_databaseManager.database());
    if (!query.exec(QStringLiteral("SELECT COALESCE(MAX(version), 0) FROM schema_migrations"))
        || !query.next()) {
        return -1;
    }
    return query.value(0).toInt();
}

qint64 CloudSyncService::localSnapshotVersion() const
{
    bool ok = false;
    const qint64 parsed = settingValue(kSettingCloudLocalVersion, QStringLiteral("0")).toLongLong(&ok);
    return ok ? parsed : 0;
}

int CloudSyncService::localSubmissionCount() const
{
    QSqlQuery query(m_databaseManager.database());
    if (!query.exec(QStringLiteral(
            "SELECT COUNT(1) FROM submission_entries WHERE COALESCE(is_deleted, 0) = 0"
        )) || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

int CloudSyncService::localLabWorkCount() const
{
    QSqlQuery query(m_databaseManager.database());
    if (!query.exec(QStringLiteral(
            "SELECT COUNT(1) FROM lab_works WHERE COALESCE(is_active, 1) = 1"
        )) || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

int CloudSyncService::localHistoryCount() const
{
    QSqlQuery query(m_databaseManager.database());
    if (!query.exec(QStringLiteral("SELECT COUNT(1) FROM check_runs")) || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

bool CloudSyncService::setLocalSnapshotVersion(qint64 version, QString *errorMessage) const
{
    return setSettingValue(kSettingCloudLocalVersion, QString::number(version), errorMessage);
}

QString CloudSyncService::resolvedUsername(const QString &usernameOverride) const
{
    const QString fromArg = usernameOverride.trimmed();
    if (!fromArg.isEmpty()) {
        return fromArg;
    }
    return cloudUsername();
}

QString CloudSyncService::resolvedPassword(const QString &passwordOverride) const
{
    const QString fromArg = passwordOverride.trimmed();
    if (!fromArg.isEmpty()) {
        return fromArg;
    }
    return cloudPassword();
}

bool CloudSyncService::ensureRemoteSchema(QSqlDatabase &remoteDb, QString *errorMessage) const
{
    QSqlQuery query(remoteDb);
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS cloud_snapshots ("
            "username TEXT PRIMARY KEY, "
            "snapshot_json TEXT NOT NULL, "
            "snapshot_version BIGINT NOT NULL DEFAULT 1, "
            "schema_version INTEGER NOT NULL DEFAULT 0, "
            "app_version TEXT NOT NULL DEFAULT '', "
            "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()"
            ")"))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось создать таблицу cloud_snapshots: %1")
                .arg(query.lastError().text());
        }
        return false;
    }

    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS cloud_snapshot_history ("
            "id BIGSERIAL PRIMARY KEY, "
            "username TEXT NOT NULL, "
            "snapshot_version BIGINT NOT NULL, "
            "schema_version INTEGER NOT NULL DEFAULT 0, "
            "app_version TEXT NOT NULL DEFAULT '', "
            "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()"
            ")"))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось создать таблицу cloud_snapshot_history: %1")
                .arg(query.lastError().text());
        }
        return false;
    }

    if (!query.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_cloud_snapshot_history_username "
            "ON cloud_snapshot_history(username, id DESC)"
            ))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось создать индекс cloud_snapshot_history: %1")
                .arg(query.lastError().text());
        }
        return false;
    }
    return true;
}

bool CloudSyncService::ensureCloudDriverAvailable(QString *errorMessage) const
{
    if (QSqlDatabase::isDriverAvailable(QStringLiteral("QPSQL"))) {
        return true;
    }

    const QStringList drivers = QSqlDatabase::drivers();
    const QString driversText = drivers.isEmpty()
        ? QStringLiteral("(нет)")
        : drivers.join(QStringLiteral(", "));

    if (errorMessage) {
        *errorMessage = QStringLiteral(
            "QPSQL driver not loaded. Убедитесь, что рядом с приложением есть PostgreSQL runtime "
            "(libpq.dll и зависимости OpenSSL). Доступные драйверы: %1"
        ).arg(driversText);
    }
    return false;
}

CloudSyncService::RemoteMeta CloudSyncService::fetchRemoteMeta(
    QSqlDatabase &remoteDb,
    const QString &username,
    QString *errorMessage
) const
{
    RemoteMeta meta;

    QSqlQuery query(remoteDb);
    query.prepare(QStringLiteral(
        "SELECT snapshot_version, schema_version, app_version, "
        "to_char(updated_at, 'YYYY-MM-DD\"T\"HH24:MI:SS'), snapshot_json "
        "FROM cloud_snapshots "
        "WHERE username = :username "
        "LIMIT 1"
    ));
    query.bindValue(QStringLiteral(":username"), username);

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось прочитать метаданные облака: %1")
                .arg(query.lastError().text());
        }
        return meta;
    }
    if (!query.next()) {
        return meta;
    }

    meta.exists = true;
    meta.snapshotVersion = query.value(0).toLongLong();
    meta.schemaVersion = query.value(1).toInt();
    meta.appVersion = repairUtf8Mojibake(query.value(2).toString());
    meta.updatedAt = query.value(3).toString();
    meta.snapshotJson = repairUtf8Mojibake(query.value(4).toString());
    return meta;
}

QString CloudSyncService::exportLocalSnapshot(QString *errorMessage) const
{
    QSqlDatabase db = m_databaseManager.database();
    QJsonObject tablesObject;

    for (const QString &table : kSnapshotTablesOrdered) {
        if (!tableExists(db, table)) {
            continue;
        }

        QSqlQuery query(db);
        if (!query.exec(QStringLiteral("SELECT * FROM %1").arg(table))) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Не удалось прочитать таблицу %1: %2")
                    .arg(table, query.lastError().text());
            }
            return QString();
        }

        const QSqlRecord record = query.record();
        QJsonArray rows;
        while (query.next()) {
            QJsonObject row;
            for (int columnIndex = 0; columnIndex < record.count(); ++columnIndex) {
                const QString columnName = record.fieldName(columnIndex);
                row.insert(columnName, variantToJson(query.value(columnIndex)));
            }
            rows.push_back(row);
        }

        QJsonObject tableObject;
        tableObject.insert(QStringLiteral("rows"), rows);
        tablesObject.insert(table, tableObject);
    }

    QJsonObject root;
    root.insert(QStringLiteral("generatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    root.insert(QStringLiteral("schemaVersion"), localSchemaVersion());
    root.insert(QStringLiteral("tables"), tablesObject);

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

bool CloudSyncService::importLocalSnapshot(const QString &snapshotJson, QString *errorMessage) const
{
    QJsonParseError parseError;
    bool repairedJson = false;
    const QJsonDocument jsonDocument = parseJsonDocumentWithRepair(snapshotJson.toUtf8(), &parseError, &repairedJson);
    if (parseError.error != QJsonParseError::NoError || !jsonDocument.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Некорректный формат снимка: %1").arg(parseError.errorString());
        }
        return false;
    }
    if (repairedJson) {
        qWarning() << "[CloudSync] Repaired non-standard JSON numbers in imported sync snapshot.";
    }

    const QJsonObject root = jsonDocument.object();
    const QJsonObject tables = root.value(QStringLiteral("tables")).toObject();

    QSqlDatabase db = m_databaseManager.database();
    if (!db.transaction()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось начать транзакцию импорта: %1").arg(db.lastError().text());
        }
        return false;
    }

    auto restoreForeignKeys = [&db]() {
        QSqlQuery pragmaOn(db);
        pragmaOn.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    };
    auto failImport = [&](const QString &message) {
        db.rollback();
        restoreForeignKeys();
        if (errorMessage) {
            *errorMessage = message;
        }
        return false;
    };

    {
        QSqlQuery pragmaOff(db);
        if (!pragmaOff.exec(QStringLiteral("PRAGMA foreign_keys = OFF"))) {
            return failImport(QStringLiteral("Не удалось отключить foreign_keys перед импортом: %1")
                                  .arg(pragmaOff.lastError().text()));
        }
    }

    for (const QString &table : kSnapshotTablesClearOrder) {
        if (!tableExists(db, table)) {
            continue;
        }
        QSqlQuery clearQuery(db);
        if (!clearQuery.exec(QStringLiteral("DELETE FROM %1").arg(table))) {
            return failImport(QStringLiteral("Не удалось очистить таблицу %1: %2")
                                  .arg(table, clearQuery.lastError().text()));
        }
    }

    for (const QString &table : kSnapshotTablesOrdered) {
        if (!tableExists(db, table) || !tables.contains(table)) {
            continue;
        }

        const QJsonArray rows = tables.value(table).toObject().value(QStringLiteral("rows")).toArray();
        if (rows.isEmpty()) {
            continue;
        }

        const QStringList columns = tableColumns(db, table);
        if (columns.isEmpty()) {
            continue;
        }

        for (const QJsonValue &rowValue : rows) {
            if (!rowValue.isObject()) {
                continue;
            }
            const QJsonObject row = rowValue.toObject();

            QStringList insertColumns;
            QVariantList bindValues;
            for (const QString &column : columns) {
                if (!row.contains(column)) {
                    continue;
                }
                insertColumns.push_back(column);
                bindValues.push_back(jsonToVariant(row.value(column)));
            }

            if (insertColumns.isEmpty()) {
                continue;
            }

            QStringList placeholders;
            placeholders.reserve(insertColumns.size());
            for (int i = 0; i < insertColumns.size(); ++i) {
                placeholders.push_back(QStringLiteral("?"));
            }

            QSqlQuery insertQuery(db);
            insertQuery.prepare(QStringLiteral(
                "INSERT INTO %1(%2) VALUES(%3)"
            ).arg(table, insertColumns.join(','), placeholders.join(',')));
            for (const QVariant &bindValue : bindValues) {
                insertQuery.addBindValue(bindValue);
            }
            if (!insertQuery.exec()) {
                return failImport(QStringLiteral("Ошибка вставки в таблицу %1: %2")
                                      .arg(table, insertQuery.lastError().text()));
            }
        }
    }

    restoreForeignKeys();

    if (!db.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось завершить импорт: %1").arg(db.lastError().text());
        }
        return false;
    }

    return true;
}

} // namespace labtester::services
