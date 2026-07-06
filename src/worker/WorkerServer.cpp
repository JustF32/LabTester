#include "worker/WorkerServer.h"

#include <algorithm>
#include <utility>

#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMetaObject>
#include <QPointer>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThreadPool>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QRandomGenerator>

#include "domain/ExecutionStatus.h"
#include "domain/LabWork.h"
#include "domain/Student.h"
#include "domain/Submission.h"
#include "domain/TestRunResult.h"
#include "runners/CppGTestRunner.h"

namespace {

QString normalizePathPart(const QString &pathPart)
{
    const QString trimmed = pathPart.trimmed();
    if (trimmed.isEmpty()) {
        return QStringLiteral("/");
    }
    const QUrl url(trimmed);
    QString path = url.path().trimmed();
    if (url.hasQuery()) {
        path += QStringLiteral("?") + url.query();
    }
    return path.isEmpty() ? QStringLiteral("/") : path;
}

QString workerLabsRootPath()
{
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString basePath = appDataPath.isEmpty() ? QDir::currentPath() : appDataPath;
    QDir dir(basePath);
    dir.mkpath(QStringLiteral("LabTesterWorker/labs"));
    return dir.filePath(QStringLiteral("LabTesterWorker/labs"));
}

bool isValidLabSignature(const QString &signature)
{
    static const QRegularExpression re(QStringLiteral("^[0-9a-fA-F]{64}$"));
    return re.match(signature.trimmed()).hasMatch();
}

QString workerSignatureLabDirPath(const QString &signature)
{
    return QDir(workerLabsRootPath()).filePath(QStringLiteral("by_signature/%1").arg(signature.trimmed().toLower()));
}

QString workerSignatureLabSyncMetadataPath(const QString &signature)
{
    return QDir(workerSignatureLabDirPath(signature)).filePath(QStringLiteral(".labtester_sync.json"));
}

QString workerSubmissionsRootPath()
{
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString basePath = appDataPath.isEmpty() ? QDir::currentPath() : appDataPath;
    QDir dir(basePath);
    dir.mkpath(QStringLiteral("LabTesterWorker/submissions"));
    return dir.filePath(QStringLiteral("LabTesterWorker/submissions"));
}

QString workerBackupsRootPath()
{
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString basePath = appDataPath.isEmpty() ? QDir::currentPath() : appDataPath;
    QDir dir(basePath);
    dir.mkpath(QStringLiteral("LabTesterWorker/backups"));
    return dir.filePath(QStringLiteral("LabTesterWorker/backups"));
}

QString workerBackupDatabasePath()
{
    return QDir(workerBackupsRootPath()).filePath(QStringLiteral("backup.sqlite"));
}

QString queryParam(const QString &path, const QString &name)
{
    const QUrl url(QStringLiteral("http://worker.local") + path);
    return QUrlQuery(url).queryItemValue(name).trimmed();
}

bool openBackupDatabase(QSqlDatabase *database, QString *errorMessage)
{
    if (!database) {
        return false;
    }

    const QString connectionName = QStringLiteral("LabTesterWorkerBackup_%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(workerBackupDatabasePath());
    if (!db.open()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось открыть SQLite backup DB: %1").arg(db.lastError().text());
        }
        QSqlDatabase::removeDatabase(connectionName);
        return false;
    }

    QSqlQuery pragma(db);
    pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    pragma.exec(QStringLiteral("PRAGMA busy_timeout=5000"));

    QSqlQuery create(db);
    if (!create.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS backup_snapshots ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "username TEXT NOT NULL,"
            "label TEXT,"
            "snapshot_json TEXT NOT NULL,"
            "schema_version INTEGER DEFAULT 0,"
            "app_version TEXT,"
            "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP"
            ")"
        ))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось создать таблицу backup_snapshots: %1")
                .arg(create.lastError().text());
        }
        db.close();
        const QString name = db.connectionName();
        *database = QSqlDatabase();
        QSqlDatabase::removeDatabase(name);
        return false;
    }

    if (!create.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_backup_snapshots_username_created "
            "ON backup_snapshots(username, created_at DESC, id DESC)"
        ))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось создать индекс backup_snapshots: %1")
                .arg(create.lastError().text());
        }
        db.close();
        const QString name = db.connectionName();
        *database = QSqlDatabase();
        QSqlDatabase::removeDatabase(name);
        return false;
    }

    if (!create.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS sync_accounts ("
            "username TEXT PRIMARY KEY,"
            "password_salt TEXT NOT NULL,"
            "password_hash TEXT NOT NULL,"
            "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
            "updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP"
            ")"
        ))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось создать таблицу sync_accounts: %1")
                .arg(create.lastError().text());
        }
        db.close();
        const QString name = db.connectionName();
        *database = QSqlDatabase();
        QSqlDatabase::removeDatabase(name);
        return false;
    }

    if (!create.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS sync_snapshots ("
            "username TEXT PRIMARY KEY,"
            "snapshot_json TEXT NOT NULL,"
            "snapshot_version INTEGER NOT NULL DEFAULT 1,"
            "schema_version INTEGER NOT NULL DEFAULT 0,"
            "app_version TEXT NOT NULL DEFAULT '',"
            "submission_count INTEGER NOT NULL DEFAULT 0,"
            "updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
            "FOREIGN KEY(username) REFERENCES sync_accounts(username) ON DELETE CASCADE"
            ")"
        ))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось создать таблицу sync_snapshots: %1")
                .arg(create.lastError().text());
        }
        db.close();
        const QString name = db.connectionName();
        *database = QSqlDatabase();
        QSqlDatabase::removeDatabase(name);
        return false;
    }

    *database = db;
    return true;
}

void closeBackupDatabase(QSqlDatabase &database)
{
    const QString connectionName = database.connectionName();
    database.close();
    database = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}

QString normalizedSourceFileName(const QString &sourcePath, const QString &sourceOriginalName)
{
    QString fileName = sourceOriginalName.trimmed();
    if (fileName.isEmpty()) {
        fileName = QFileInfo(sourcePath).fileName();
    }

    fileName = QFileInfo(fileName).fileName();
    if (fileName.isEmpty()) {
        return QStringLiteral("student.cpp");
    }

    if (QFileInfo(fileName).suffix().trimmed().isEmpty()) {
        fileName += QStringLiteral(".cpp");
    }
    return fileName;
}

QString materializeSubmissionSourceFile(
    const QString &jobId,
    const QString &sourcePath,
    const QString &sourceOriginalName,
    const QString &sourceContentBase64
)
{
    const QString encodedContent = sourceContentBase64.trimmed();
    if (encodedContent.isEmpty()) {
        return sourcePath;
    }

    const QByteArray decodedContent = QByteArray::fromBase64(encodedContent.toUtf8());
    if (decodedContent.isEmpty()) {
        qWarning().noquote() << QStringLiteral(
            "[Worker] failed to decode source content for job=%1, fallback to sourcePath"
        ).arg(jobId);
        return sourcePath;
    }

    const QString submissionsRoot = workerSubmissionsRootPath();
    const QString jobDirPath = QDir(submissionsRoot).filePath(jobId);
    if (!QDir().mkpath(jobDirPath)) {
        qWarning().noquote() << QStringLiteral(
            "[Worker] failed to create source directory for job=%1 path=%2"
        ).arg(jobId, QDir::toNativeSeparators(jobDirPath));
        return sourcePath;
    }

    const QString fileName = normalizedSourceFileName(sourcePath, sourceOriginalName);
    const QString targetPath = QDir(jobDirPath).filePath(fileName);

    QFile targetFile(targetPath);
    if (!targetFile.open(QIODevice::WriteOnly)) {
        qWarning().noquote() << QStringLiteral(
            "[Worker] failed to open source file for write job=%1 path=%2 err=%3"
        ).arg(jobId, QDir::toNativeSeparators(targetPath), targetFile.errorString());
        return sourcePath;
    }

    if (targetFile.write(decodedContent) < 0) {
        qWarning().noquote() << QStringLiteral(
            "[Worker] failed to write source content for job=%1 path=%2 err=%3"
        ).arg(jobId, QDir::toNativeSeparators(targetPath), targetFile.errorString());
        targetFile.close();
        return sourcePath;
    }
    targetFile.close();

    const QString resolvedPath = QDir::toNativeSeparators(QFileInfo(targetPath).absoluteFilePath());
    qInfo().noquote() << QStringLiteral(
        "[Worker] materialized source for job=%1 path=%2"
    ).arg(jobId, resolvedPath);
    return resolvedPath;
}

bool isSafeRelativePath(const QString &relativePath)
{
    const QString normalized = QDir::fromNativeSeparators(relativePath.trimmed());
    if (normalized.isEmpty()) {
        return false;
    }
    if (normalized.startsWith(QLatin1Char('/')) || normalized.startsWith(QStringLiteral("../"))) {
        return false;
    }
    return !normalized.contains(QStringLiteral("/../"))
        && !normalized.contains(QStringLiteral("..\\"));
}

bool writeJsonFile(const QString &path, const QJsonObject &object, QString *errorMessage)
{
    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось подготовить папку для файла: %1").arg(info.absolutePath());
        }
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось записать файл: %1").arg(file.errorString());
        }
        return false;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

QJsonObject readJsonFileObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }
    return document.object();
}

QString validateSyncedLabForRun(const QString &expectedLabSignature, const QString &providedTestSuiteFile)
{
    if (!isValidLabSignature(expectedLabSignature)) {
        return QStringLiteral("Запрос запуска не содержит корректную контрольную сумму тестового набора.");
    }

    const QString labDirPath = workerSignatureLabDirPath(expectedLabSignature);
    const QDir labDir(labDirPath);
    if (!labDir.exists()) {
        return QStringLiteral("Тестовый набор %1 еще не синхронизирован с воркером.")
            .arg(expectedLabSignature.left(12));
    }

    const QJsonObject metadata = readJsonFileObject(workerSignatureLabSyncMetadataPath(expectedLabSignature));
    if (metadata.isEmpty()) {
        return QStringLiteral("Для тестового набора %1 нет метаданных синхронизации. Запустите проверку еще раз.")
            .arg(expectedLabSignature.left(12));
    }

    QString testSuiteFile = providedTestSuiteFile;
    if (testSuiteFile.isEmpty()) {
        testSuiteFile = metadata.value(QStringLiteral("testSuiteFile")).toString();
        if (testSuiteFile.isEmpty()) {
            testSuiteFile = QStringLiteral("sort_test.cpp");
        }
    }

    if (!labDir.exists(QStringLiteral("tests/") + testSuiteFile) || !labDir.exists(QStringLiteral("tests/cc.h"))) {
        return QStringLiteral(
            "На воркере для тестового набора %1 лежит неполный набор файлов: нужны tests/%2 и tests/cc.h."
        ).arg(expectedLabSignature.left(12), testSuiteFile);
    }

    const QString actualSignature = metadata.value(QStringLiteral("labSignature")).toString().trimmed();
    if (actualSignature.compare(expectedLabSignature.trimmed(), Qt::CaseInsensitive) != 0) {
        return QStringLiteral("Метаданные тестового набора %1 повреждены или не совпадают.")
            .arg(expectedLabSignature.left(12));
    }

    return QString();
}

QString normalizedUsername(const QString &username)
{
    return username.trimmed().toLower();
}

QString randomSalt()
{
    QByteArray bytes;
    bytes.resize(16);
    for (int i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    return QString::fromLatin1(bytes.toHex());
}

QString passwordHash(const QString &password, const QString &salt)
{
    return QString::fromLatin1(QCryptographicHash::hash(
        (salt + QStringLiteral(":") + password).toUtf8(),
        QCryptographicHash::Sha256
    ).toHex());
}

int snapshotTableRowCount(const QString &snapshotJson, const QString &tableName)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(snapshotJson.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return 0;
    }

    const QJsonObject tables = document.object().value(QStringLiteral("tables")).toObject();
    const QJsonArray rows = tables.value(tableName).toObject().value(QStringLiteral("rows")).toArray();
    return rows.size();
}

bool verifyAccountPassword(QSqlDatabase &db, const QString &username, const QString &password, QString *errorMessage)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT password_salt, password_hash FROM sync_accounts WHERE username = :username"));
    query.bindValue(QStringLiteral(":username"), normalizedUsername(username));
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось проверить аккаунт: %1").arg(query.lastError().text());
        }
        return false;
    }
    if (!query.next()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Аккаунт не найден.");
        }
        return false;
    }
    const QString salt = query.value(0).toString();
    const QString expectedHash = query.value(1).toString();
    if (passwordHash(password.trimmed(), salt) != expectedHash) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Неверный пароль аккаунта.");
        }
        return false;
    }
    return true;
}

} // namespace

namespace labtester::worker {

WorkerServer::WorkerServer(WorkerConfig config, QObject *parent)
    : QObject(parent)
    , m_config(std::move(config))
{
    if (m_config.maxParallelJobs <= 0) {
        m_config.maxParallelJobs = 1;
    }
    if (m_config.maxStoredJobs < 50) {
        m_config.maxStoredJobs = 50;
    }

    connect(&m_server, &QTcpServer::newConnection, this, &WorkerServer::onNewConnection);
    QThreadPool::globalInstance()->setMaxThreadCount(std::max(1, m_config.maxParallelJobs));
}

bool WorkerServer::start(QString *errorMessage)
{
    if (!m_server.listen(m_config.bindAddress, m_config.port)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                "Не удалось запустить сервер (%1:%2): %3"
            ).arg(m_config.bindAddress.toString()).arg(m_config.port).arg(m_server.errorString());
        }
        return false;
    }
    return true;
}

void WorkerServer::onNewConnection()
{
    while (m_server.hasPendingConnections()) {
        QTcpSocket *socket = m_server.nextPendingConnection();
        if (!socket) {
            continue;
        }
        m_socketStates.insert(socket, SocketState{});

        connect(socket, &QTcpSocket::readyRead, this, &WorkerServer::onSocketReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &WorkerServer::onSocketDisconnected);
    }
}

void WorkerServer::onSocketReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket) {
        return;
    }

    auto it = m_socketStates.find(socket);
    if (it == m_socketStates.end()) {
        return;
    }

    it->buffer += socket->readAll();

    QString parseError;
    if (!parseRequestFromSocket(socket, *it, &parseError)) {
        if (parseError.isEmpty()) {
            return;
        }
        sendError(socket, 400, parseError);
        socket->disconnectFromHost();
        return;
    }

    const RequestData request = it->request;
    m_socketStates.erase(it);
    handleRequest(socket, request);
    socket->disconnectFromHost();
}

void WorkerServer::onSocketDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket) {
        return;
    }
    m_socketStates.remove(socket);
    socket->deleteLater();
}

bool WorkerServer::parseRequestFromSocket(QTcpSocket *socket, SocketState &state, QString *errorMessage)
{
    Q_UNUSED(socket);

    if (!state.headerParsed) {
        const int headerEnd = state.buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0) {
            return false;
        }

        const QByteArray headerBytes = state.buffer.left(headerEnd);
        state.buffer.remove(0, headerEnd + 4);

        const QList<QByteArray> lines = headerBytes.split('\n');
        if (lines.isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Пустой HTTP-запрос.");
            }
            return false;
        }

        const QList<QByteArray> requestLineParts = lines.first().trimmed().split(' ');
        if (requestLineParts.size() < 2) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Некорректная строка запроса.");
            }
            return false;
        }

        state.request.method = QString::fromUtf8(requestLineParts.at(0)).trimmed().toUpper();
        state.request.path = normalizePathPart(QString::fromUtf8(requestLineParts.at(1)));

        for (int i = 1; i < lines.size(); ++i) {
            const QByteArray line = lines.at(i).trimmed();
            if (line.isEmpty()) {
                continue;
            }

            const int separator = line.indexOf(':');
            if (separator <= 0) {
                continue;
            }

            const QString key = QString::fromUtf8(line.left(separator)).trimmed().toLower();
            const QString value = QString::fromUtf8(line.mid(separator + 1)).trimmed();
            state.request.headers.insert(key, value);
        }

        bool ok = false;
        const int contentLength = state.request.headers.value(QStringLiteral("content-length")).toInt(&ok);
        state.contentLength = ok ? std::max(0, contentLength) : 0;
        state.headerParsed = true;
    }

    if (state.buffer.size() < state.contentLength) {
        return false;
    }

    state.request.body = state.buffer.left(state.contentLength);
    state.buffer.remove(0, state.contentLength);
    return true;
}

void WorkerServer::handleRequest(QTcpSocket *socket, const RequestData &request)
{
    if (request.method == QStringLiteral("GET") && request.path == QStringLiteral("/health")) {
        handleHealth(socket);
        return;
    }

    if (!isAuthorized(request)) {
        sendError(socket, 401, QStringLiteral("Unauthorized"));
        return;
    }

    if (request.method == QStringLiteral("POST") && request.path == QStringLiteral("/run")) {
        handleRun(socket, request);
        return;
    }

    if (request.method == QStringLiteral("POST") && request.path == QStringLiteral("/sync/lab")) {
        handleSyncLab(socket, request);
        return;
    }

    if (request.method == QStringLiteral("POST") && request.path == QStringLiteral("/backup/snapshots")) {
        handleBackupSnapshotUpload(socket, request);
        return;
    }

    if (request.method == QStringLiteral("GET") && request.path.startsWith(QStringLiteral("/backup/snapshots/latest"))) {
        handleLatestBackupSnapshot(socket, request);
        return;
    }

    if (request.method == QStringLiteral("GET") && request.path == QStringLiteral("/sync/accounts")) {
        handleSyncAccounts(socket);
        return;
    }

    if (request.method == QStringLiteral("POST") && request.path == QStringLiteral("/sync/account")) {
        handleCreateSyncAccount(socket, request);
        return;
    }

    if (request.method == QStringLiteral("POST") && request.path == QStringLiteral("/sync/meta")) {
        handleSyncMeta(socket, request);
        return;
    }

    if (request.method == QStringLiteral("POST") && request.path == QStringLiteral("/sync/push")) {
        handleSyncPush(socket, request);
        return;
    }

    if (request.method == QStringLiteral("POST") && request.path == QStringLiteral("/sync/pull")) {
        handleSyncPull(socket, request);
        return;
    }

    if (request.method == QStringLiteral("GET") && request.path == QStringLiteral("/jobs")) {
        handleJobs(socket, request);
        return;
    }

    if (request.method == QStringLiteral("GET") && request.path.startsWith(QStringLiteral("/jobs/"))) {
        const QString jobId = request.path.section('/', 2, 2).trimmed();
        if (jobId.isEmpty()) {
            sendError(socket, 400, QStringLiteral("Некорректный id задачи."));
            return;
        }
        handleSingleJob(socket, jobId);
        return;
    }

    sendError(socket, 404, QStringLiteral("Маршрут не найден."));
}

bool WorkerServer::isAuthorized(const RequestData &request) const
{
    if (m_config.authToken.trimmed().isEmpty()) {
        return true;
    }
    return bearerTokenFromRequest(request) == m_config.authToken.trimmed();
}

QString WorkerServer::bearerTokenFromRequest(const RequestData &request) const
{
    const QString header = request.headers.value(QStringLiteral("authorization")).trimmed();
    if (!header.startsWith(QStringLiteral("Bearer "), Qt::CaseInsensitive)) {
        return QString();
    }
    return header.mid(7).trimmed();
}

void WorkerServer::handleHealth(QTcpSocket *socket)
{
    QJsonObject response;
    response.insert(QStringLiteral("ok"), true);
    response.insert(QStringLiteral("service"), QStringLiteral("LabTesterWorker"));
    response.insert(QStringLiteral("version"), m_config.appVersion);
    response.insert(QStringLiteral("time"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    response.insert(QStringLiteral("activeJobs"), m_activeJobs);
    response.insert(QStringLiteral("queuedJobs"), m_pendingJobs.size());
    response.insert(QStringLiteral("maxParallelJobs"), m_config.maxParallelJobs);

    sendJson(socket, 200, response);
}

void WorkerServer::handleSyncLab(QTcpSocket *socket, const RequestData &request)
{
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(request.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
        sendError(socket, 400, QStringLiteral("Некорректный JSON: %1").arg(parseError.errorString()));
        return;
    }

    const QJsonObject object = json.object();
    const int labId = object.value(QStringLiteral("labId")).toInt(0);
    if (labId <= 0) {
        sendError(socket, 400, QStringLiteral("Поле labId обязательно и должно быть > 0."));
        return;
    }

    const QJsonArray files = object.value(QStringLiteral("files")).toArray();
    if (files.isEmpty()) {
        sendError(socket, 400, QStringLiteral("Список files пуст."));
        return;
    }

    const QString labSignature = object.value(QStringLiteral("labSignature")).toString().trimmed().toLower();
    if (!isValidLabSignature(labSignature)) {
        sendError(socket, 400, QStringLiteral("Поле labSignature обязательно и должно быть SHA-256 в hex."));
        return;
    }

    QString testSuiteFile = object.value(QStringLiteral("testSuiteFile")).toString();
    if (testSuiteFile.isEmpty()) {
        testSuiteFile = QStringLiteral("sort_test.cpp");
    }

    const QString labDirPath = workerSignatureLabDirPath(labSignature);
    QDir labDir(labDirPath);
    const QJsonObject existingMetadata = readJsonFileObject(workerSignatureLabSyncMetadataPath(labSignature));
    if (labDir.exists()
        && labDir.exists(QStringLiteral("tests/") + testSuiteFile)
        && labDir.exists(QStringLiteral("tests/cc.h"))
        && existingMetadata.value(QStringLiteral("labSignature")).toString().compare(labSignature, Qt::CaseInsensitive) == 0) {
        QJsonObject response;
        response.insert(QStringLiteral("ok"), true);
        response.insert(QStringLiteral("labId"), labId);
        response.insert(QStringLiteral("labSignature"), labSignature);
        response.insert(QStringLiteral("filesWritten"), 0);
        response.insert(QStringLiteral("reused"), true);
        response.insert(QStringLiteral("path"), QDir::toNativeSeparators(labDirPath));
        sendJson(socket, 200, response);
        return;
    }

    if (labDir.exists() && !labDir.removeRecursively()) {
        sendError(socket, 500, QStringLiteral("Не удалось очистить папку лабораторной на воркере."));
        return;
    }
    if (!QDir().mkpath(labDirPath)) {
        sendError(socket, 500, QStringLiteral("Не удалось создать папку лабораторной на воркере."));
        return;
    }

    int writtenFiles = 0;
    for (const QJsonValue &fileValue : files) {
        const QJsonObject fileObject = fileValue.toObject();
        const QString relativePath = QDir::fromNativeSeparators(
            fileObject.value(QStringLiteral("relativePath")).toString().trimmed()
        );
        if (!isSafeRelativePath(relativePath)) {
            sendError(socket, 400, QStringLiteral("Небезопасный relativePath: %1").arg(relativePath));
            return;
        }

        const QString base64 = fileObject.value(QStringLiteral("contentBase64")).toString();
        const QByteArray content = QByteArray::fromBase64(base64.toUtf8());

        const QString absolutePath = QDir(labDirPath).filePath(relativePath);
        const QFileInfo targetInfo(absolutePath);
        if (!QDir().mkpath(targetInfo.absolutePath())) {
            sendError(socket, 500, QStringLiteral("Не удалось создать подпапку: %1").arg(targetInfo.absolutePath()));
            return;
        }

        QFile targetFile(absolutePath);
        if (!targetFile.open(QIODevice::WriteOnly)) {
            sendError(socket, 500, QStringLiteral("Не удалось записать файл: %1").arg(relativePath));
            return;
        }
        targetFile.write(content);
        targetFile.close();
        ++writtenFiles;
    }

    qInfo().noquote() << QStringLiteral("[Worker] synced lab id=%1 signature=%2 files=%3 path=%4")
                             .arg(labId)
                             .arg(labSignature.left(12))
                             .arg(writtenFiles)
                             .arg(QDir::toNativeSeparators(labDirPath));

    QJsonObject metadata;
    metadata.insert(QStringLiteral("labId"), labId);
    metadata.insert(QStringLiteral("labTitle"), object.value(QStringLiteral("labTitle")).toString().trimmed());
    metadata.insert(QStringLiteral("labRootName"), object.value(QStringLiteral("labRootName")).toString().trimmed());
    metadata.insert(QStringLiteral("labSignature"), labSignature);
    metadata.insert(QStringLiteral("testSuiteFile"), object.value(QStringLiteral("testSuiteFile")).toString().trimmed());
    metadata.insert(QStringLiteral("syncedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    metadata.insert(QStringLiteral("filesWritten"), writtenFiles);

    QString metadataError;
    if (!writeJsonFile(workerSignatureLabSyncMetadataPath(labSignature), metadata, &metadataError)) {
        sendError(socket, 500, metadataError);
        return;
    }

    QJsonObject response;
    response.insert(QStringLiteral("ok"), true);
    response.insert(QStringLiteral("labId"), labId);
    response.insert(QStringLiteral("labSignature"), labSignature);
    response.insert(QStringLiteral("filesWritten"), writtenFiles);
    response.insert(QStringLiteral("reused"), false);
    response.insert(QStringLiteral("path"), QDir::toNativeSeparators(labDirPath));
    sendJson(socket, 200, response);
}

void WorkerServer::handleRun(QTcpSocket *socket, const RequestData &request)
{
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(request.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
        sendError(socket, 400, QStringLiteral("Некорректный JSON: %1").arg(parseError.errorString()));
        return;
    }

    const QJsonObject object = json.object();
    const QString sourcePath = object.value(QStringLiteral("sourcePath")).toString().trimmed();
    const QString sourceContentBase64 = object.value(QStringLiteral("sourceContentBase64")).toString().trimmed();
    if (sourcePath.isEmpty() && sourceContentBase64.isEmpty()) {
        sendError(socket, 400, QStringLiteral("Нужно передать sourcePath или sourceContentBase64."));
        return;
    }

    JobInput input;
    input.sourcePath = sourcePath;
    input.sourceOriginalName = object.value(QStringLiteral("sourceOriginalName")).toString().trimmed();
    input.sourceContentBase64 = sourceContentBase64;
    if (input.sourceOriginalName.isEmpty()) {
        input.sourceOriginalName = QFileInfo(sourcePath).fileName();
    }
    input.labId = std::max(1, object.value(QStringLiteral("labId")).toInt(1));
    input.labRootName = object.value(QStringLiteral("labRootName")).toString().trimmed();
    input.labSignature = object.value(QStringLiteral("labSignature")).toString().trimmed().toLower();
    input.studentName = object.value(QStringLiteral("studentName")).toString().trimmed();
    if (input.studentName.isEmpty()) {
        input.studentName = QStringLiteral("Remote Student");
    }
    input.testSuiteFile = object.value(QStringLiteral("testSuiteFile")).toString().trimmed();
    input.labTitle = object.value(QStringLiteral("labTitle")).toString().trimmed();
    if (input.labTitle.isEmpty()) {
        input.labTitle = QStringLiteral("Лабораторная %1").arg(input.labId);
    }

    const QString labValidationError = validateSyncedLabForRun(input.labSignature, input.testSuiteFile);
    if (!labValidationError.isEmpty()) {
        sendError(socket, 409, labValidationError);
        return;
    }

    const int requestedSubmissionId = object.value(QStringLiteral("submissionId")).toInt(0);
    if (requestedSubmissionId > 0) {
        input.submissionId = requestedSubmissionId;
    } else {
        input.submissionId = m_submissionSequence++;
    }

    const QString jobId = enqueueJob(input);

    QJsonObject response;
    response.insert(QStringLiteral("ok"), true);
    response.insert(QStringLiteral("jobId"), jobId);
    response.insert(QStringLiteral("state"), QStringLiteral("queued"));
    response.insert(QStringLiteral("message"), QStringLiteral("Задача добавлена в очередь."));
    sendJson(socket, 202, response);
}

void WorkerServer::handleBackupSnapshotUpload(QTcpSocket *socket, const RequestData &request)
{
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(request.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
        sendError(socket, 400, QStringLiteral("Некорректный JSON: %1").arg(parseError.errorString()));
        return;
    }

    const QJsonObject object = json.object();
    QString username = object.value(QStringLiteral("username")).toString().trimmed();
    if (username.isEmpty()) {
        username = QStringLiteral("default");
    }

    QString snapshotJson = object.value(QStringLiteral("snapshotJson")).toString().trimmed();
    if (snapshotJson.isEmpty() && object.value(QStringLiteral("snapshot")).isObject()) {
        snapshotJson = QString::fromUtf8(
            QJsonDocument(object.value(QStringLiteral("snapshot")).toObject()).toJson(QJsonDocument::Compact)
        );
    }
    if (snapshotJson.isEmpty()) {
        sendError(socket, 400, QStringLiteral("Поле snapshotJson обязательно."));
        return;
    }

    QJsonParseError snapshotParseError;
    const QJsonDocument snapshotDocument = QJsonDocument::fromJson(snapshotJson.toUtf8(), &snapshotParseError);
    if (snapshotParseError.error != QJsonParseError::NoError || !snapshotDocument.isObject()) {
        sendError(socket, 400, QStringLiteral("snapshotJson должен быть JSON-объектом: %1")
                                  .arg(snapshotParseError.errorString()));
        return;
    }

    const QJsonObject snapshotObject = snapshotDocument.object();
    const int schemaVersion = snapshotObject.value(QStringLiteral("schemaVersion")).toInt(
        object.value(QStringLiteral("schemaVersion")).toInt(0)
    );
    const QString appVersion = object.value(QStringLiteral("appVersion")).toString(m_config.appVersion).trimmed();
    const QString label = object.value(QStringLiteral("label")).toString().trimmed();

    QString dbError;
    QSqlDatabase backupDb;
    if (!openBackupDatabase(&backupDb, &dbError)) {
        sendError(socket, 500, dbError);
        return;
    }

    QSqlQuery insert(backupDb);
    insert.prepare(QStringLiteral(
        "INSERT INTO backup_snapshots(username, label, snapshot_json, schema_version, app_version, created_at) "
        "VALUES(:username, :label, :snapshot_json, :schema_version, :app_version, CURRENT_TIMESTAMP)"
    ));
    insert.bindValue(QStringLiteral(":username"), username);
    insert.bindValue(QStringLiteral(":label"), label);
    insert.bindValue(QStringLiteral(":snapshot_json"), snapshotJson);
    insert.bindValue(QStringLiteral(":schema_version"), schemaVersion);
    insert.bindValue(QStringLiteral(":app_version"), appVersion);
    if (!insert.exec()) {
        const QString error = QStringLiteral("Не удалось сохранить backup snapshot: %1")
            .arg(insert.lastError().text());
        closeBackupDatabase(backupDb);
        sendError(socket, 500, error);
        return;
    }

    const qint64 snapshotId = insert.lastInsertId().toLongLong();
    closeBackupDatabase(backupDb);

    QJsonObject response;
    response.insert(QStringLiteral("ok"), true);
    response.insert(QStringLiteral("id"), static_cast<double>(snapshotId));
    response.insert(QStringLiteral("username"), username);
    response.insert(QStringLiteral("schemaVersion"), schemaVersion);
    response.insert(QStringLiteral("appVersion"), appVersion);
    response.insert(QStringLiteral("databasePath"), QDir::toNativeSeparators(workerBackupDatabasePath()));
    sendJson(socket, 200, response);
}

void WorkerServer::handleLatestBackupSnapshot(QTcpSocket *socket, const RequestData &request)
{
    QString username = queryParam(request.path, QStringLiteral("username"));
    if (username.isEmpty()) {
        username = QStringLiteral("default");
    }

    QString dbError;
    QSqlDatabase backupDb;
    if (!openBackupDatabase(&backupDb, &dbError)) {
        sendError(socket, 500, dbError);
        return;
    }

    QSqlQuery query(backupDb);
    query.prepare(QStringLiteral(
        "SELECT id, username, label, snapshot_json, schema_version, app_version, created_at "
        "FROM backup_snapshots "
        "WHERE username = :username "
        "ORDER BY created_at DESC, id DESC "
        "LIMIT 1"
    ));
    query.bindValue(QStringLiteral(":username"), username);
    if (!query.exec()) {
        const QString error = QStringLiteral("Не удалось прочитать backup snapshot: %1")
            .arg(query.lastError().text());
        closeBackupDatabase(backupDb);
        sendError(socket, 500, error);
        return;
    }

    if (!query.next()) {
        closeBackupDatabase(backupDb);
        QJsonObject response;
        response.insert(QStringLiteral("ok"), true);
        response.insert(QStringLiteral("exists"), false);
        response.insert(QStringLiteral("username"), username);
        sendJson(socket, 200, response);
        return;
    }

    QJsonObject response;
    response.insert(QStringLiteral("ok"), true);
    response.insert(QStringLiteral("exists"), true);
    response.insert(QStringLiteral("id"), query.value(0).toLongLong());
    response.insert(QStringLiteral("username"), query.value(1).toString());
    response.insert(QStringLiteral("label"), query.value(2).toString());
    response.insert(QStringLiteral("snapshotJson"), query.value(3).toString());
    response.insert(QStringLiteral("schemaVersion"), query.value(4).toInt());
    response.insert(QStringLiteral("appVersion"), query.value(5).toString());
    response.insert(QStringLiteral("createdAt"), query.value(6).toString());
    closeBackupDatabase(backupDb);
    sendJson(socket, 200, response);
}

void WorkerServer::handleSyncAccounts(QTcpSocket *socket)
{
    QString dbError;
    QSqlDatabase db;
    if (!openBackupDatabase(&db, &dbError)) {
        sendError(socket, 500, dbError);
        return;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT a.username, a.updated_at, "
        "COALESCE(s.submission_count, 0), COALESCE(s.snapshot_version, 0), COALESCE(s.snapshot_json, '') "
        "FROM sync_accounts a "
        "LEFT JOIN sync_snapshots s ON s.username = a.username "
        "ORDER BY a.username"
    ));
    if (!query.exec()) {
        const QString error = QStringLiteral("Не удалось прочитать аккаунты: %1").arg(query.lastError().text());
        closeBackupDatabase(db);
        sendError(socket, 500, error);
        return;
    }

    QJsonArray accounts;
    while (query.next()) {
        QJsonObject item;
        item.insert(QStringLiteral("username"), query.value(0).toString());
        item.insert(QStringLiteral("updatedAt"), query.value(1).toString());
        item.insert(QStringLiteral("submissionCount"), query.value(2).toInt());
        item.insert(QStringLiteral("snapshotVersion"), query.value(3).toLongLong());
        item.insert(QStringLiteral("labWorkCount"), snapshotTableRowCount(query.value(4).toString(), QStringLiteral("lab_works")));
        accounts.push_back(item);
    }
    closeBackupDatabase(db);

    QJsonObject response;
    response.insert(QStringLiteral("ok"), true);
    response.insert(QStringLiteral("accounts"), accounts);
    sendJson(socket, 200, response);
}

void WorkerServer::handleCreateSyncAccount(QTcpSocket *socket, const RequestData &request)
{
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(request.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
        sendError(socket, 400, QStringLiteral("Некорректный JSON: %1").arg(parseError.errorString()));
        return;
    }

    const QJsonObject object = json.object();
    const QString username = normalizedUsername(object.value(QStringLiteral("username")).toString());
    const QString password = object.value(QStringLiteral("password")).toString().trimmed();
    if (username.isEmpty() || password.isEmpty()) {
        sendError(socket, 400, QStringLiteral("Введите имя аккаунта и пароль."));
        return;
    }

    QString dbError;
    QSqlDatabase db;
    if (!openBackupDatabase(&db, &dbError)) {
        sendError(socket, 500, dbError);
        return;
    }

    const QString salt = randomSalt();
    QSqlQuery insert(db);
    insert.prepare(QStringLiteral(
        "INSERT INTO sync_accounts(username, password_salt, password_hash, created_at, updated_at) "
        "VALUES(:username, :salt, :hash, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)"
    ));
    insert.bindValue(QStringLiteral(":username"), username);
    insert.bindValue(QStringLiteral(":salt"), salt);
    insert.bindValue(QStringLiteral(":hash"), passwordHash(password, salt));
    if (!insert.exec()) {
        const QString error = insert.lastError().nativeErrorCode() == QStringLiteral("19")
            ? QStringLiteral("Аккаунт уже существует.")
            : QStringLiteral("Не удалось создать аккаунт: %1").arg(insert.lastError().text());
        closeBackupDatabase(db);
        sendError(socket, 400, error);
        return;
    }
    closeBackupDatabase(db);

    QJsonObject response;
    response.insert(QStringLiteral("ok"), true);
    response.insert(QStringLiteral("username"), username);
    sendJson(socket, 200, response);
}

void WorkerServer::handleSyncMeta(QTcpSocket *socket, const RequestData &request)
{
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(request.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
        sendError(socket, 400, QStringLiteral("Некорректный JSON: %1").arg(parseError.errorString()));
        return;
    }

    const QJsonObject object = json.object();
    const QString username = normalizedUsername(object.value(QStringLiteral("username")).toString());
    const QString password = object.value(QStringLiteral("password")).toString().trimmed();
    if (username.isEmpty() || password.isEmpty()) {
        sendError(socket, 400, QStringLiteral("Введите имя аккаунта и пароль."));
        return;
    }

    QString dbError;
    QSqlDatabase db;
    if (!openBackupDatabase(&db, &dbError)) {
        sendError(socket, 500, dbError);
        return;
    }
    QString authError;
    if (!verifyAccountPassword(db, username, password, &authError)) {
        closeBackupDatabase(db);
        sendError(socket, 401, authError);
        return;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT snapshot_version, schema_version, app_version, submission_count, updated_at, snapshot_json "
        "FROM sync_snapshots WHERE username = :username LIMIT 1"
    ));
    query.bindValue(QStringLiteral(":username"), username);
    if (!query.exec()) {
        const QString error = QStringLiteral("Не удалось прочитать данные аккаунта: %1").arg(query.lastError().text());
        closeBackupDatabase(db);
        sendError(socket, 500, error);
        return;
    }

    QJsonObject response;
    response.insert(QStringLiteral("ok"), true);
    response.insert(QStringLiteral("username"), username);
    if (!query.next()) {
        response.insert(QStringLiteral("exists"), false);
        response.insert(QStringLiteral("snapshotVersion"), 0);
        response.insert(QStringLiteral("schemaVersion"), 0);
        response.insert(QStringLiteral("appVersion"), QString());
        response.insert(QStringLiteral("submissionCount"), 0);
        response.insert(QStringLiteral("labWorkCount"), 0);
        response.insert(QStringLiteral("historyCount"), 0);
        response.insert(QStringLiteral("updatedAt"), QString());
    } else {
        const QString snapshotJson = query.value(5).toString();
        response.insert(QStringLiteral("exists"), true);
        response.insert(QStringLiteral("snapshotVersion"), query.value(0).toLongLong());
        response.insert(QStringLiteral("schemaVersion"), query.value(1).toInt());
        response.insert(QStringLiteral("appVersion"), query.value(2).toString());
        response.insert(QStringLiteral("submissionCount"), query.value(3).toInt());
        response.insert(QStringLiteral("labWorkCount"), snapshotTableRowCount(snapshotJson, QStringLiteral("lab_works")));
        response.insert(QStringLiteral("historyCount"), snapshotTableRowCount(snapshotJson, QStringLiteral("check_runs")));
        response.insert(QStringLiteral("updatedAt"), query.value(4).toString());
    }
    closeBackupDatabase(db);
    sendJson(socket, 200, response);
}

void WorkerServer::handleSyncPush(QTcpSocket *socket, const RequestData &request)
{
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(request.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
        sendError(socket, 400, QStringLiteral("Некорректный JSON: %1").arg(parseError.errorString()));
        return;
    }

    const QJsonObject object = json.object();
    const QString username = normalizedUsername(object.value(QStringLiteral("username")).toString());
    const QString password = object.value(QStringLiteral("password")).toString().trimmed();
    const QString snapshotJson = object.value(QStringLiteral("snapshotJson")).toString().trimmed();
    if (username.isEmpty() || password.isEmpty() || snapshotJson.isEmpty()) {
        sendError(socket, 400, QStringLiteral("Нужны username, password и snapshotJson."));
        return;
    }

    QJsonParseError snapshotParseError;
    const QJsonDocument snapshotDocument = QJsonDocument::fromJson(snapshotJson.toUtf8(), &snapshotParseError);
    if (snapshotParseError.error != QJsonParseError::NoError || !snapshotDocument.isObject()) {
        sendError(socket, 400, QStringLiteral("snapshotJson должен быть JSON-объектом: %1")
                                  .arg(snapshotParseError.errorString()));
        return;
    }
    const int historyCount = snapshotTableRowCount(snapshotJson, QStringLiteral("check_runs"));

    QString dbError;
    QSqlDatabase db;
    if (!openBackupDatabase(&db, &dbError)) {
        sendError(socket, 500, dbError);
        return;
    }
    QString authError;
    if (!verifyAccountPassword(db, username, password, &authError)) {
        closeBackupDatabase(db);
        sendError(socket, 401, authError);
        return;
    }

    qint64 previousVersion = 0;
    QSqlQuery versionQuery(db);
    versionQuery.prepare(QStringLiteral("SELECT snapshot_version FROM sync_snapshots WHERE username = :username"));
    versionQuery.bindValue(QStringLiteral(":username"), username);
    if (versionQuery.exec() && versionQuery.next()) {
        previousVersion = versionQuery.value(0).toLongLong();
    }
    const qint64 nextVersion = previousVersion + 1;

    QSqlQuery upsert(db);
    upsert.prepare(QStringLiteral(
        "INSERT INTO sync_snapshots(username, snapshot_json, snapshot_version, schema_version, app_version, submission_count, updated_at) "
        "VALUES(:username, :snapshot_json, :snapshot_version, :schema_version, :app_version, :submission_count, CURRENT_TIMESTAMP) "
        "ON CONFLICT(username) DO UPDATE SET "
        "snapshot_json = excluded.snapshot_json, snapshot_version = excluded.snapshot_version, "
        "schema_version = excluded.schema_version, app_version = excluded.app_version, "
        "submission_count = excluded.submission_count, updated_at = CURRENT_TIMESTAMP"
    ));
    upsert.bindValue(QStringLiteral(":username"), username);
    upsert.bindValue(QStringLiteral(":snapshot_json"), snapshotJson);
    upsert.bindValue(QStringLiteral(":snapshot_version"), nextVersion);
    upsert.bindValue(QStringLiteral(":schema_version"), object.value(QStringLiteral("schemaVersion")).toInt(0));
    upsert.bindValue(QStringLiteral(":app_version"), object.value(QStringLiteral("appVersion")).toString(m_config.appVersion));
    upsert.bindValue(QStringLiteral(":submission_count"), object.value(QStringLiteral("submissionCount")).toInt(0));
    if (!upsert.exec()) {
        const QString error = QStringLiteral("Не удалось сохранить снимок: %1").arg(upsert.lastError().text());
        closeBackupDatabase(db);
        sendError(socket, 500, error);
        return;
    }

    QJsonObject response;
    response.insert(QStringLiteral("ok"), true);
    response.insert(QStringLiteral("username"), username);
    response.insert(QStringLiteral("snapshotVersion"), nextVersion);
    response.insert(QStringLiteral("submissionCount"), object.value(QStringLiteral("submissionCount")).toInt(0));
    response.insert(QStringLiteral("labWorkCount"), snapshotTableRowCount(snapshotJson, QStringLiteral("lab_works")));
    response.insert(QStringLiteral("historyCount"), historyCount);
    closeBackupDatabase(db);
    sendJson(socket, 200, response);
}

void WorkerServer::handleSyncPull(QTcpSocket *socket, const RequestData &request)
{
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(request.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
        sendError(socket, 400, QStringLiteral("Некорректный JSON: %1").arg(parseError.errorString()));
        return;
    }

    const QJsonObject object = json.object();
    const QString username = normalizedUsername(object.value(QStringLiteral("username")).toString());
    const QString password = object.value(QStringLiteral("password")).toString().trimmed();
    if (username.isEmpty() || password.isEmpty()) {
        sendError(socket, 400, QStringLiteral("Введите имя аккаунта и пароль."));
        return;
    }

    QString dbError;
    QSqlDatabase db;
    if (!openBackupDatabase(&db, &dbError)) {
        sendError(socket, 500, dbError);
        return;
    }
    QString authError;
    if (!verifyAccountPassword(db, username, password, &authError)) {
        closeBackupDatabase(db);
        sendError(socket, 401, authError);
        return;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT snapshot_json, snapshot_version, schema_version, app_version, submission_count, updated_at "
        "FROM sync_snapshots WHERE username = :username LIMIT 1"
    ));
    query.bindValue(QStringLiteral(":username"), username);
    if (!query.exec()) {
        const QString error = QStringLiteral("Не удалось прочитать снимок: %1").arg(query.lastError().text());
        closeBackupDatabase(db);
        sendError(socket, 500, error);
        return;
    }

    QJsonObject response;
    response.insert(QStringLiteral("ok"), true);
    response.insert(QStringLiteral("username"), username);
    if (!query.next()) {
        response.insert(QStringLiteral("exists"), false);
    } else {
        response.insert(QStringLiteral("exists"), true);
        response.insert(QStringLiteral("snapshotJson"), query.value(0).toString());
        response.insert(QStringLiteral("snapshotVersion"), query.value(1).toLongLong());
        response.insert(QStringLiteral("schemaVersion"), query.value(2).toInt());
        response.insert(QStringLiteral("appVersion"), query.value(3).toString());
        response.insert(QStringLiteral("submissionCount"), query.value(4).toInt());
        response.insert(QStringLiteral("labWorkCount"), snapshotTableRowCount(query.value(0).toString(), QStringLiteral("lab_works")));
        response.insert(QStringLiteral("historyCount"), snapshotTableRowCount(query.value(0).toString(), QStringLiteral("check_runs")));
        response.insert(QStringLiteral("updatedAt"), query.value(5).toString());
    }
    closeBackupDatabase(db);
    sendJson(socket, 200, response);
}

void WorkerServer::handleJobs(QTcpSocket *socket, const RequestData &request)
{
    Q_UNUSED(request);

    QJsonArray items;
    for (auto it = m_jobs.cbegin(); it != m_jobs.cend(); ++it) {
        items.push_back(jobToJson(it.value()));
    }

    QJsonObject response;
    response.insert(QStringLiteral("ok"), true);
    response.insert(QStringLiteral("jobs"), items);
    sendJson(socket, 200, response);
}

void WorkerServer::handleSingleJob(QTcpSocket *socket, const QString &jobId)
{
    const auto it = m_jobs.find(jobId);
    if (it == m_jobs.end()) {
        sendError(socket, 404, QStringLiteral("Задача не найдена."));
        return;
    }

    QJsonObject response;
    response.insert(QStringLiteral("ok"), true);
    response.insert(QStringLiteral("job"), jobToJson(it.value()));
    sendJson(socket, 200, response);
}

QString WorkerServer::enqueueJob(const JobInput &input)
{
    JobRecord record;
    record.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    record.input = input;
    record.state = JobState::Queued;
    record.createdAt = QDateTime::currentDateTimeUtc();

    m_jobs.insert(record.id, record);
    m_pendingJobs.enqueue(record.id);
    scheduleJobs();
    trimJobs();
    return record.id;
}

void WorkerServer::scheduleJobs()
{
    while (m_activeJobs < m_config.maxParallelJobs && !m_pendingJobs.isEmpty()) {
        const QString nextJob = m_pendingJobs.dequeue();
        startJob(nextJob);
    }
}

void WorkerServer::startJob(const QString &jobId)
{
    auto it = m_jobs.find(jobId);
    if (it == m_jobs.end()) {
        return;
    }

    it->state = JobState::Running;
    it->startedAt = QDateTime::currentDateTimeUtc();
    const JobInput input = it->input;
    ++m_activeJobs;

    QPointer<WorkerServer> safeThis(this);
    QThreadPool::globalInstance()->start([safeThis, jobId, input]() {
        domain::Submission submission;
        submission.id = input.submissionId;
        submission.sourcePath = materializeSubmissionSourceFile(
            jobId,
            input.sourcePath,
            input.sourceOriginalName,
            input.sourceContentBase64
        );
        submission.student = domain::Student{0, input.studentName, QString()};
        const QString syncedLabDir = workerSignatureLabDirPath(input.labSignature);
        QString suiteFileName = input.testSuiteFile;
        if (suiteFileName.isEmpty()) {
            suiteFileName = QStringLiteral("sort_test.cpp");
        }
        submission.labWork = domain::LabWork{
            input.labId,
            input.labTitle,
            QStringLiteral("C++"),
            QString(),
            QString(),
            QString(),
            QString(),
            QString(),
            QDir(syncedLabDir).filePath(QStringLiteral("tests/") + suiteFileName),
            QDir(syncedLabDir).filePath(QStringLiteral("tests/cc.h"))
        };
        submission.status = domain::ExecutionStatus::Pending;
        submission.createdAt = QDateTime::currentDateTime();

        runners::CppGTestRunner runner;
        const domain::TestRunResult result = runner.run(submission);
        const QJsonObject resultJson = WorkerServer::resultToJson(result);

        if (!safeThis) {
            return;
        }
        QMetaObject::invokeMethod(
            safeThis,
            [safeThis, jobId, resultJson]() {
                if (!safeThis) {
                    return;
                }
                safeThis->completeJob(jobId, true, QString(), resultJson);
            },
            Qt::QueuedConnection
        );
    });
}

void WorkerServer::completeJob(
    const QString &jobId,
    bool success,
    const QString &errorMessage,
    const QJsonObject &result
)
{
    auto it = m_jobs.find(jobId);
    if (it == m_jobs.end()) {
        return;
    }

    it->finishedAt = QDateTime::currentDateTimeUtc();
    it->result = result;
    it->errorMessage = errorMessage;
    it->state = success ? JobState::Finished : JobState::Failed;

    m_activeJobs = std::max(0, m_activeJobs - 1);
    scheduleJobs();
    trimJobs();
}

void WorkerServer::trimJobs()
{
    if (m_jobs.size() <= m_config.maxStoredJobs) {
        return;
    }

    QList<JobRecord> completed;
    completed.reserve(m_jobs.size());
    for (const JobRecord &job : std::as_const(m_jobs)) {
        if (job.state == JobState::Finished || job.state == JobState::Failed) {
            completed.push_back(job);
        }
    }

    std::sort(completed.begin(), completed.end(), [](const JobRecord &a, const JobRecord &b) {
        return a.finishedAt < b.finishedAt;
    });

    int removeCount = m_jobs.size() - m_config.maxStoredJobs;
    for (int i = 0; i < completed.size() && removeCount > 0; ++i) {
        m_jobs.remove(completed.at(i).id);
        --removeCount;
    }
}

QJsonObject WorkerServer::jobToJson(const JobRecord &job) const
{
    QJsonObject json;
    json.insert(QStringLiteral("jobId"), job.id);
    json.insert(QStringLiteral("state"), jobStateToString(job.state));
    json.insert(QStringLiteral("createdAt"), job.createdAt.toString(Qt::ISODateWithMs));
    json.insert(QStringLiteral("startedAt"), job.startedAt.toString(Qt::ISODateWithMs));
    json.insert(QStringLiteral("finishedAt"), job.finishedAt.toString(Qt::ISODateWithMs));
    json.insert(QStringLiteral("error"), job.errorMessage);

    QJsonObject request;
    request.insert(QStringLiteral("submissionId"), job.input.submissionId);
    request.insert(QStringLiteral("labId"), job.input.labId);
    request.insert(QStringLiteral("sourcePath"), job.input.sourcePath);
    request.insert(QStringLiteral("sourceOriginalName"), job.input.sourceOriginalName);
    request.insert(
        QStringLiteral("hasEmbeddedSource"),
        !job.input.sourceContentBase64.trimmed().isEmpty()
    );
    request.insert(QStringLiteral("studentName"), job.input.studentName);
    request.insert(QStringLiteral("labTitle"), job.input.labTitle);
    json.insert(QStringLiteral("request"), request);
    json.insert(QStringLiteral("result"), job.result);
    return json;
}

QJsonObject WorkerServer::resultToJson(const domain::TestRunResult &result)
{
    QJsonObject json;
    json.insert(QStringLiteral("submissionId"), result.submissionId);
    json.insert(QStringLiteral("studentName"), result.studentName);
    json.insert(QStringLiteral("labTitle"), result.labTitle);
    json.insert(QStringLiteral("totalTests"), result.totalTests);
    json.insert(QStringLiteral("passedTests"), result.passedTests);
    json.insert(QStringLiteral("failedTests"), result.failedTests);
    json.insert(QStringLiteral("status"), domain::toStorageString(result.status));
    json.insert(QStringLiteral("message"), result.message);
    json.insert(QStringLiteral("executedAt"), result.executedAt.toString(Qt::ISODateWithMs));

    QJsonArray testCases;
    for (const auto &testCase : result.testCases) {
        QJsonObject caseObject;
        caseObject.insert(QStringLiteral("testName"), testCase.testName);
        caseObject.insert(QStringLiteral("passed"), testCase.passed);
        caseObject.insert(QStringLiteral("message"), testCase.message);
        caseObject.insert(QStringLiteral("inputData"), testCase.inputData);
        caseObject.insert(QStringLiteral("expectedOutput"), testCase.expectedOutput);
        caseObject.insert(QStringLiteral("actualOutput"), testCase.actualOutput);
        caseObject.insert(QStringLiteral("failureDetails"), testCase.failureDetails);
        caseObject.insert(QStringLiteral("durationMs"), static_cast<qint64>(testCase.durationMs));
        testCases.push_back(caseObject);
    }
    json.insert(QStringLiteral("testCases"), testCases);
    return json;
}

QString WorkerServer::jobStateToString(JobState state)
{
    switch (state) {
    case JobState::Queued:
        return QStringLiteral("queued");
    case JobState::Running:
        return QStringLiteral("running");
    case JobState::Finished:
        return QStringLiteral("finished");
    case JobState::Failed:
        return QStringLiteral("failed");
    }
    return QStringLiteral("queued");
}

QString WorkerServer::statusTextForHttpCode(int statusCode)
{
    switch (statusCode) {
    case 200:
        return QStringLiteral("OK");
    case 202:
        return QStringLiteral("Accepted");
    case 400:
        return QStringLiteral("Bad Request");
    case 401:
        return QStringLiteral("Unauthorized");
    case 404:
        return QStringLiteral("Not Found");
    default:
        return QStringLiteral("Internal Server Error");
    }
}

void WorkerServer::sendJson(QTcpSocket *socket, int statusCode, const QJsonObject &json)
{
    if (!socket) {
        return;
    }
    const QByteArray body = QJsonDocument(json).toJson(QJsonDocument::Compact);
    QByteArray response;
    response += "HTTP/1.1 " + QByteArray::number(statusCode) + ' '
        + statusTextForHttpCode(statusCode).toUtf8() + "\r\n";
    response += "Content-Type: application/json; charset=utf-8\r\n";
    response += "Connection: close\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n";
    response += body;
    socket->write(response);
    socket->flush();
}

void WorkerServer::sendError(QTcpSocket *socket, int statusCode, const QString &message)
{
    QJsonObject payload;
    payload.insert(QStringLiteral("ok"), false);
    payload.insert(QStringLiteral("error"), message);
    sendJson(socket, statusCode, payload);
}

} // namespace labtester::worker
