#pragma once

#include <QString>
#include <QVariantList>
#include <QVariantMap>

class QSqlDatabase;

namespace labtester::database {
class DatabaseManager;
}

namespace labtester::services {

class CloudSyncService {
public:
    explicit CloudSyncService(database::DatabaseManager &databaseManager);

    QString cloudUsername() const;
    bool setCloudUsername(const QString &username, QString *errorMessage = nullptr);
    QString cloudPassword() const;
    bool setCloudPassword(const QString &password, QString *errorMessage = nullptr);

    bool autoSyncEnabled() const;
    bool setAutoSyncEnabled(bool enabled, QString *errorMessage = nullptr);

    QVariantMap versionInfo(const QString &usernameOverride = QString(), bool includeRemote = true) const;
    QVariantList remoteAccounts(QString *errorMessage = nullptr) const;
    QVariantList remoteSaveHistory(
        const QString &usernameOverride = QString(),
        QString *errorMessage = nullptr
    ) const;

    bool createAccount(const QString &username, const QString &password, QString *outMessage = nullptr);
    bool syncToCloud(const QString &usernameOverride, const QString &password = QString(), QString *outMessage = nullptr);
    bool syncFromCloud(const QString &usernameOverride, const QString &password = QString(), QString *outMessage = nullptr);

    bool autoSyncOnStartup(QString *outMessage = nullptr);
    bool autoSyncOnShutdown(QString *outMessage = nullptr);

private:
    struct RemoteMeta {
        bool exists {false};
        qint64 snapshotVersion {0};
        int schemaVersion {0};
        QString appVersion;
        QString updatedAt;
        QString snapshotJson;
    };

    QString settingValue(const QString &key, const QString &fallback = QString()) const;
    bool setSettingValue(const QString &key, const QString &value, QString *errorMessage = nullptr) const;

    int localSchemaVersion() const;
    qint64 localSnapshotVersion() const;
    int localSubmissionCount() const;
    int localLabWorkCount() const;
    int localHistoryCount() const;
    bool setLocalSnapshotVersion(qint64 version, QString *errorMessage = nullptr) const;

    QString resolvedUsername(const QString &usernameOverride) const;
    QString resolvedPassword(const QString &passwordOverride) const;

    bool ensureRemoteSchema(QSqlDatabase &remoteDb, QString *errorMessage = nullptr) const;
    bool ensureCloudDriverAvailable(QString *errorMessage = nullptr) const;
    RemoteMeta fetchRemoteMeta(QSqlDatabase &remoteDb, const QString &username, QString *errorMessage = nullptr) const;

    QString exportLocalSnapshot(QString *errorMessage = nullptr) const;
    bool importLocalSnapshot(const QString &snapshotJson, QString *errorMessage = nullptr) const;

    database::DatabaseManager &m_databaseManager;
};

} // namespace labtester::services
