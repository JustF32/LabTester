#pragma once

#include <QDateTime>
#include <QHash>
#include <QHostAddress>
#include <QJsonObject>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QTcpServer>

#include "domain/TestRunResult.h"

class QTcpSocket;

namespace labtester::worker {

struct WorkerConfig {
    QHostAddress bindAddress {QHostAddress::LocalHost};
    quint16 port {18080};
    QString authToken;
    int maxParallelJobs {2};
    int maxStoredJobs {250};
    QString appVersion {QStringLiteral("0.2v")};
};

class WorkerServer : public QObject {
    Q_OBJECT

public:
    explicit WorkerServer(WorkerConfig config, QObject *parent = nullptr);

    bool start(QString *errorMessage = nullptr);

private slots:
    void onNewConnection();
    void onSocketReadyRead();
    void onSocketDisconnected();

private:
    struct RequestData {
        QString method;
        QString path;
        QHash<QString, QString> headers;
        QByteArray body;
    };

    struct SocketState {
        QByteArray buffer;
        bool headerParsed {false};
        int contentLength {0};
        RequestData request;
    };

    struct JobInput {
        int submissionId {0};
        int labId {1};
        QString sourcePath;
        QString sourceOriginalName;
        QString sourceContentBase64;
        QString studentName;
        QString labTitle;
        QString labRootName;
        QString labSignature;
        QString testSuiteFile;
    };

    enum class JobState {
        Queued,
        Running,
        Finished,
        Failed
    };

    struct JobRecord {
        QString id;
        JobInput input;
        JobState state {JobState::Queued};
        QDateTime createdAt {QDateTime::currentDateTimeUtc()};
        QDateTime startedAt;
        QDateTime finishedAt;
        QJsonObject result;
        QString errorMessage;
    };

    bool parseRequestFromSocket(QTcpSocket *socket, SocketState &state, QString *errorMessage);
    void handleRequest(QTcpSocket *socket, const RequestData &request);

    bool isAuthorized(const RequestData &request) const;
    QString bearerTokenFromRequest(const RequestData &request) const;

    void handleHealth(QTcpSocket *socket);
    void handleSyncLab(QTcpSocket *socket, const RequestData &request);
    void handleRun(QTcpSocket *socket, const RequestData &request);
    void handleBackupSnapshotUpload(QTcpSocket *socket, const RequestData &request);
    void handleLatestBackupSnapshot(QTcpSocket *socket, const RequestData &request);
    void handleSyncAccounts(QTcpSocket *socket);
    void handleCreateSyncAccount(QTcpSocket *socket, const RequestData &request);
    void handleSyncMeta(QTcpSocket *socket, const RequestData &request);
    void handleSyncPush(QTcpSocket *socket, const RequestData &request);
    void handleSyncPull(QTcpSocket *socket, const RequestData &request);
    void handleJobs(QTcpSocket *socket, const RequestData &request);
    void handleSingleJob(QTcpSocket *socket, const QString &jobId);

    QString enqueueJob(const JobInput &input);
    void scheduleJobs();
    void startJob(const QString &jobId);
    void completeJob(
        const QString &jobId,
        bool success,
        const QString &errorMessage,
        const QJsonObject &result
    );
    void trimJobs();

    QJsonObject jobToJson(const JobRecord &job) const;
    static QJsonObject resultToJson(const domain::TestRunResult &result);
    static QString jobStateToString(JobState state);
    static QString statusTextForHttpCode(int statusCode);

    void sendJson(QTcpSocket *socket, int statusCode, const QJsonObject &json);
    void sendError(QTcpSocket *socket, int statusCode, const QString &message);

    WorkerConfig m_config;
    QTcpServer m_server;
    QHash<QTcpSocket *, SocketState> m_socketStates;
    QHash<QString, JobRecord> m_jobs;
    QQueue<QString> m_pendingJobs;
    int m_activeJobs {0};
    int m_submissionSequence {1};
};

} // namespace labtester::worker
