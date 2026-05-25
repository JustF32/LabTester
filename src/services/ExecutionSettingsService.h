#pragma once

#include <QMutex>
#include <QString>

namespace labtester::database {
class DatabaseManager;
}

namespace labtester::services {

class ExecutionSettingsService {
public:
    struct RemoteConfig {
        QString endpoint;
        QString token;
        bool ngrokMode {false};
    };

    static constexpr const char *kModeLocal = "local";
    static constexpr const char *kModeServer = "server";
    static constexpr const char *kModeHybrid = "hybrid";

    explicit ExecutionSettingsService(database::DatabaseManager &databaseManager);

    QString executionMode() const;
    bool setExecutionMode(const QString &mode, QString *errorMessage = nullptr) const;

    QString remoteEndpoint() const;
    bool setRemoteEndpoint(const QString &endpoint, QString *errorMessage = nullptr) const;

    QString remoteToken() const;
    bool setRemoteToken(const QString &token, QString *errorMessage = nullptr) const;

    bool remoteNgrokMode() const;
    bool setRemoteNgrokMode(bool enabled, QString *errorMessage = nullptr) const;

    RemoteConfig remoteConfig() const;

private:
    QString settingValueFromDatabase(const QString &key, const QString &fallback = QString()) const;
    bool setSettingValue(const QString &key, const QString &value, QString *errorMessage = nullptr) const;
    void ensureCacheLoaded() const;
    static QString normalizeMode(const QString &mode);
    static QString normalizeEndpoint(const QString &endpoint);

    database::DatabaseManager &m_databaseManager;
    mutable QMutex m_cacheMutex;
    mutable bool m_cacheLoaded {false};
    mutable QString m_cachedExecutionMode;
    mutable RemoteConfig m_cachedRemoteConfig;
};

} // namespace labtester::services
