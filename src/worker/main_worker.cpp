#include <algorithm>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include "worker/WorkerServer.h"

namespace {

QString generateToken()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-');
}

bool parseHostAddress(const QString &text, QHostAddress *outAddress)
{
    if (!outAddress) {
        return false;
    }

    QHostAddress parsed;
    if (!parsed.setAddress(text.trimmed())) {
        return false;
    }
    *outAddress = parsed;
    return true;
}

bool loadConfigFromFile(
    const QString &filePath,
    labtester::worker::WorkerConfig *config,
    QString *errorMessage
)
{
    if (!config) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось открыть конфиг: %1").arg(file.errorString());
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Некорректный JSON конфига: %1").arg(parseError.errorString());
        }
        return false;
    }

    const QJsonObject object = document.object();
    const QString bindText = object.value(QStringLiteral("bind")).toString().trimmed();
    if (!bindText.isEmpty()) {
        QHostAddress address;
        if (!parseHostAddress(bindText, &address)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Некорректный bind адрес в конфиге.");
            }
            return false;
        }
        config->bindAddress = address;
    }

    const int port = object.value(QStringLiteral("port")).toInt(static_cast<int>(config->port));
    if (port <= 0 || port > 65535) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Некорректный порт в конфиге.");
        }
        return false;
    }
    config->port = static_cast<quint16>(port);

    const QString token = object.value(QStringLiteral("authToken")).toString().trimmed();
    if (!token.isEmpty()) {
        config->authToken = token;
    }

    config->maxParallelJobs = std::max(1, object.value(QStringLiteral("maxParallelJobs")).toInt(config->maxParallelJobs));
    config->maxStoredJobs = std::max(50, object.value(QStringLiteral("maxStoredJobs")).toInt(config->maxStoredJobs));

    const QString appVersion = object.value(QStringLiteral("appVersion")).toString().trimmed();
    if (!appVersion.isEmpty()) {
        config->appVersion = appVersion;
    }

    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("LabTesterWorker"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.2v"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("LabTester Worker Service"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption configOption(
        QStringList{QStringLiteral("c"), QStringLiteral("config")},
        QStringLiteral("Путь к JSON конфигу."),
        QStringLiteral("file")
    );
    QCommandLineOption bindOption(
        QStringList{QStringLiteral("bind")},
        QStringLiteral("Bind адрес, например 127.0.0.1 или 0.0.0.0."),
        QStringLiteral("address")
    );
    QCommandLineOption portOption(
        QStringList{QStringLiteral("p"), QStringLiteral("port")},
        QStringLiteral("Порт для прослушивания."),
        QStringLiteral("port")
    );
    QCommandLineOption tokenOption(
        QStringList{QStringLiteral("token")},
        QStringLiteral("Bearer токен для API."),
        QStringLiteral("token")
    );
    QCommandLineOption maxParallelOption(
        QStringList{QStringLiteral("max-parallel")},
        QStringLiteral("Максимум параллельных задач."),
        QStringLiteral("count")
    );

    parser.addOption(configOption);
    parser.addOption(bindOption);
    parser.addOption(portOption);
    parser.addOption(tokenOption);
    parser.addOption(maxParallelOption);

    parser.process(app);

    labtester::worker::WorkerConfig config;

    if (parser.isSet(configOption)) {
        QString errorMessage;
        if (!loadConfigFromFile(parser.value(configOption), &config, &errorMessage)) {
            qCritical().noquote() << errorMessage;
            return 1;
        }
    }

    if (parser.isSet(bindOption)) {
        QHostAddress address;
        if (!parseHostAddress(parser.value(bindOption), &address)) {
            qCritical().noquote() << "Некорректный bind адрес.";
            return 1;
        }
        config.bindAddress = address;
    }

    if (parser.isSet(portOption)) {
        bool ok = false;
        const int port = parser.value(portOption).toInt(&ok);
        if (!ok || port <= 0 || port > 65535) {
            qCritical().noquote() << "Некорректный порт.";
            return 1;
        }
        config.port = static_cast<quint16>(port);
    }

    if (parser.isSet(tokenOption)) {
        config.authToken = parser.value(tokenOption).trimmed();
    }

    if (parser.isSet(maxParallelOption)) {
        bool ok = false;
        const int maxParallel = parser.value(maxParallelOption).toInt(&ok);
        if (!ok || maxParallel <= 0) {
            qCritical().noquote() << "Некорректное значение --max-parallel.";
            return 1;
        }
        config.maxParallelJobs = maxParallel;
    }

    if (config.authToken.trimmed().isEmpty()) {
        config.authToken = generateToken();
        qWarning().noquote() << "Auth token не задан. Сгенерирован временный токен:" << config.authToken;
    }

    qInfo().noquote() << "LabTesterWorker starting...";
    qInfo().noquote() << "  bind:" << config.bindAddress.toString();
    qInfo().noquote() << "  port:" << config.port;
    qInfo().noquote() << "  maxParallelJobs:" << config.maxParallelJobs;
    qInfo().noquote() << "  appVersion:" << config.appVersion;
    qInfo().noquote() << "  bearer token:" << config.authToken;

    labtester::worker::WorkerServer server(config);
    QString startError;
    if (!server.start(&startError)) {
        qCritical().noquote() << startError;
        return 1;
    }

    qInfo().noquote() << "LabTesterWorker started successfully.";
    return app.exec();
}
