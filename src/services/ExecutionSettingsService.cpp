#include "services/ExecutionSettingsService.h"

#include <QMutexLocker>
#include <QSqlError>
#include <QSqlQuery>

#include "database/DatabaseManager.h"

namespace {

const QString kSettingExecutionMode = QStringLiteral("execution.mode");
const QString kSettingRemoteEndpoint = QStringLiteral("execution.remote.endpoint");
const QString kSettingRemoteToken = QStringLiteral("execution.remote.token");
const QString kSettingRemoteNgrokMode = QStringLiteral("execution.remote.ngrok_mode");

const QString kDefaultRemoteEndpoint = QStringLiteral("");
const QString kDefaultRemoteToken = QStringLiteral("");

} // namespace

namespace labtester::services {

ExecutionSettingsService::ExecutionSettingsService(database::DatabaseManager &databaseManager)
    : m_databaseManager(databaseManager)
{
    ensureCacheLoaded();
}

QString ExecutionSettingsService::executionMode() const
{
    ensureCacheLoaded();
    QMutexLocker locker(&m_cacheMutex);
    return m_cachedExecutionMode;
}

bool ExecutionSettingsService::setExecutionMode(const QString &mode, QString *errorMessage) const
{
    const QString normalizedMode = normalizeMode(mode);
    if (!setSettingValue(kSettingExecutionMode, normalizedMode, errorMessage)) {
        return false;
    }

    QMutexLocker locker(&m_cacheMutex);
    m_cacheLoaded = true;
    m_cachedExecutionMode = normalizedMode;
    return true;
}

QString ExecutionSettingsService::remoteEndpoint() const
{
    ensureCacheLoaded();
    QMutexLocker locker(&m_cacheMutex);
    return m_cachedRemoteConfig.endpoint;
}

bool ExecutionSettingsService::setRemoteEndpoint(const QString &endpoint, QString *errorMessage) const
{
    const QString normalizedEndpoint = normalizeEndpoint(endpoint);
    if (!setSettingValue(kSettingRemoteEndpoint, normalizedEndpoint, errorMessage)) {
        return false;
    }

    QMutexLocker locker(&m_cacheMutex);
    m_cacheLoaded = true;
    m_cachedRemoteConfig.endpoint = normalizedEndpoint;
    return true;
}

QString ExecutionSettingsService::remoteToken() const
{
    ensureCacheLoaded();
    QMutexLocker locker(&m_cacheMutex);
    return m_cachedRemoteConfig.token;
}

bool ExecutionSettingsService::setRemoteToken(const QString &token, QString *errorMessage) const
{
    const QString normalizedToken = token.trimmed();
    if (!setSettingValue(kSettingRemoteToken, normalizedToken, errorMessage)) {
        return false;
    }

    QMutexLocker locker(&m_cacheMutex);
    m_cacheLoaded = true;
    m_cachedRemoteConfig.token = normalizedToken;
    return true;
}

bool ExecutionSettingsService::remoteNgrokMode() const
{
    ensureCacheLoaded();
    QMutexLocker locker(&m_cacheMutex);
    return m_cachedRemoteConfig.ngrokMode;
}

bool ExecutionSettingsService::setRemoteNgrokMode(bool enabled, QString *errorMessage) const
{
    if (!setSettingValue(kSettingRemoteNgrokMode, enabled ? QStringLiteral("1") : QStringLiteral("0"), errorMessage)) {
        return false;
    }

    QMutexLocker locker(&m_cacheMutex);
    m_cacheLoaded = true;
    m_cachedRemoteConfig.ngrokMode = enabled;
    return true;
}

ExecutionSettingsService::RemoteConfig ExecutionSettingsService::remoteConfig() const
{
    ensureCacheLoaded();
    QMutexLocker locker(&m_cacheMutex);
    return m_cachedRemoteConfig;
}

QString ExecutionSettingsService::settingValueFromDatabase(const QString &key, const QString &fallback) const
{
    QSqlQuery query(m_databaseManager.database());
    query.prepare(QStringLiteral("SELECT value FROM settings WHERE key = :key LIMIT 1"));
    query.bindValue(QStringLiteral(":key"), key);
    if (!query.exec() || !query.next()) {
        return fallback;
    }
    return query.value(0).toString();
}

void ExecutionSettingsService::ensureCacheLoaded() const
{
    QMutexLocker locker(&m_cacheMutex);
    if (m_cacheLoaded) {
        return;
    }

    m_cachedExecutionMode = QString::fromLatin1(kModeServer);
    m_cachedRemoteConfig.endpoint = kDefaultRemoteEndpoint;
    m_cachedRemoteConfig.token = kDefaultRemoteToken;
    m_cachedRemoteConfig.ngrokMode = false;

    const QString mode = settingValueFromDatabase(
        kSettingExecutionMode,
        QString::fromLatin1(kModeServer)
    );
    m_cachedExecutionMode = normalizeMode(mode);

    m_cachedRemoteConfig.endpoint = normalizeEndpoint(
        settingValueFromDatabase(kSettingRemoteEndpoint, kDefaultRemoteEndpoint)
    );
    m_cachedRemoteConfig.token = settingValueFromDatabase(kSettingRemoteToken, kDefaultRemoteToken).trimmed();
    const QString ngrokValue = settingValueFromDatabase(kSettingRemoteNgrokMode, QStringLiteral("0"))
        .trimmed()
        .toLower();
    m_cachedRemoteConfig.ngrokMode = ngrokValue == QStringLiteral("1")
        || ngrokValue == QStringLiteral("true")
        || ngrokValue == QStringLiteral("yes");

    m_cacheLoaded = true;
}

bool ExecutionSettingsService::setSettingValue(const QString &key, const QString &value, QString *errorMessage) const
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
            *errorMessage = QStringLiteral("Не удалось сохранить настройку %1: %2")
                .arg(key, query.lastError().text());
        }
        return false;
    }
    return true;
}

QString ExecutionSettingsService::normalizeMode(const QString &mode)
{
    const QString normalized = mode.trimmed().toLower();
    if (normalized == QString::fromLatin1(kModeServer)) {
        return QString::fromLatin1(kModeServer);
    }
    if (normalized == QString::fromLatin1(kModeHybrid)) {
        return QString::fromLatin1(kModeHybrid);
    }
    return QString::fromLatin1(kModeLocal);
}

QString ExecutionSettingsService::normalizeEndpoint(const QString &endpoint)
{
    QString normalized = endpoint.trimmed();
    if (normalized.isEmpty()) {
        return QString();
    }
    if (!normalized.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
        && !normalized.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
        normalized.prepend(QStringLiteral("http://"));
    }
    while (normalized.endsWith('/')) {
        normalized.chop(1);
    }
    return normalized;
}

} // namespace labtester::services
