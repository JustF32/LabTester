#include "database/DatabaseManager.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringList>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include <algorithm>
#include <utility>

namespace labtester::database {

DatabaseManager::DatabaseManager(QString connectionName)
    : m_connectionName(std::move(connectionName))
{
}

DatabaseManager::~DatabaseManager()
{
    if (m_database.isValid() && m_database.isOpen()) {
        m_database.close();
    }
    if (QSqlDatabase::contains(m_connectionName)) {
        m_database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool DatabaseManager::initialize()
{
    if (!open()) {
        return false;
    }
    if (!executeInitScript()) {
        return false;
    }
    return applyMigrations();
}

bool DatabaseManager::isOpen() const
{
    return m_database.isOpen();
}

QSqlDatabase DatabaseManager::database() const
{
    return m_database;
}

QString DatabaseManager::databaseFilePath() const
{
    return m_databasePath;
}

bool DatabaseManager::open()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        m_database = QSqlDatabase::database(m_connectionName);
    } else {
        m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    }

    m_databasePath = resolveDatabasePath();
    m_database.setDatabaseName(m_databasePath);

    if (!m_database.open()) {
        qWarning() << "Failed to open SQLite database at" << m_databasePath << ":" << m_database.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::executeInitScript()
{
    QFile scriptFile(QStringLiteral(":/db/init.sql"));
    if (!scriptFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open SQL init script:" << scriptFile.errorString();
        return false;
    }

    const QString script = QString::fromUtf8(scriptFile.readAll());
    return executeStatements(script);
}

bool DatabaseManager::executeStatements(const QString &script)
{
    const QStringList statements = script.split(';', Qt::SkipEmptyParts);
    QSqlQuery query(m_database);

    for (const QString &rawStatement : statements) {
        const QString statement = rawStatement.trimmed();
        if (statement.isEmpty()) {
            continue;
        }
        if (!query.exec(statement)) {
            qWarning() << "SQL statement failed:" << statement;
            qWarning() << "SQL error:" << query.lastError().text();
            return false;
        }
    }
    return true;
}

bool DatabaseManager::ensureMigrationTable()
{
    const QString createMigrationTableSql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS schema_migrations ("
        "version INTEGER PRIMARY KEY, "
        "name TEXT NOT NULL, "
        "applied_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP"
        ");"
    );

    QSqlQuery query(m_database);
    if (!query.exec(createMigrationTableSql)) {
        qWarning() << "Failed to ensure schema_migrations table:" << query.lastError().text();
        return false;
    }
    return true;
}

std::vector<DatabaseManager::MigrationScript> DatabaseManager::collectMigrations() const
{
    std::vector<MigrationScript> migrations;

    const QDir migrationsDir(QStringLiteral(":/db/migrations"));
    if (!migrationsDir.exists()) {
        return migrations;
    }

    const QFileInfoList migrationFiles = migrationsDir.entryInfoList(
        QStringList() << QStringLiteral("*.sql"),
        QDir::Files | QDir::Readable,
        QDir::Name
    );

    const QRegularExpression filePattern(QStringLiteral("^(\\d+)_([a-zA-Z0-9_\\-]+)\\.sql$"));
    for (const QFileInfo &fileInfo : migrationFiles) {
        const QRegularExpressionMatch match = filePattern.match(fileInfo.fileName());
        if (!match.hasMatch()) {
            qWarning() << "Skipping migration with invalid file name:" << fileInfo.fileName();
            continue;
        }

        bool ok = false;
        const int version = match.captured(1).toInt(&ok);
        if (!ok) {
            qWarning() << "Skipping migration with invalid numeric version:" << fileInfo.fileName();
            continue;
        }

        MigrationScript migration;
        migration.version = version;
        migration.name = match.captured(2);
        migration.resourcePath = fileInfo.filePath();
        migrations.push_back(migration);
    }

    std::sort(migrations.begin(), migrations.end(), [](const MigrationScript &lhs, const MigrationScript &rhs) {
        return lhs.version < rhs.version;
    });

    return migrations;
}

int DatabaseManager::currentSchemaVersion() const
{
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("SELECT COALESCE(MAX(version), 0) FROM schema_migrations"))) {
        qWarning() << "Failed to read schema version:" << query.lastError().text();
        return -1;
    }
    if (!query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

bool DatabaseManager::markMigrationApplied(int version, const QString &name)
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT INTO schema_migrations(version, name, applied_at) "
        "VALUES(:version, :name, CURRENT_TIMESTAMP)"
    ));
    query.bindValue(QStringLiteral(":version"), version);
    query.bindValue(QStringLiteral(":name"), name);

    if (!query.exec()) {
        qWarning() << "Failed to save applied migration version" << version << ":" << query.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::applyMigrations()
{
    if (!ensureMigrationTable()) {
        return false;
    }

    const std::vector<MigrationScript> migrations = collectMigrations();
    const int appliedVersion = currentSchemaVersion();
    if (appliedVersion < 0) {
        return false;
    }

    for (const auto &migration : migrations) {
        if (migration.version <= appliedVersion) {
            continue;
        }

        QFile file(migration.resourcePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "Failed to open migration file:" << migration.resourcePath << file.errorString();
            return false;
        }

        const QString sqlScript = QString::fromUtf8(file.readAll());

        if (!m_database.transaction()) {
            qWarning() << "Failed to start migration transaction for version" << migration.version << ":" << m_database.lastError().text();
            return false;
        }

        if (!executeStatements(sqlScript) || !markMigrationApplied(migration.version, migration.name)) {
            m_database.rollback();
            return false;
        }

        if (!m_database.commit()) {
            qWarning() << "Failed to commit migration version" << migration.version << ":" << m_database.lastError().text();
            return false;
        }

        qInfo() << "Applied migration version" << migration.version << "(" << migration.name << ")";
    }

    return true;
}

QString DatabaseManager::resolveDatabasePath() const
{
    const QString appDataRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString fallbackRoot = QDir::currentPath();
    const QString basePath = appDataRoot.isEmpty() ? fallbackRoot : appDataRoot;

    QDir dir(basePath);
    if (!dir.mkpath(QStringLiteral("LabTester"))) {
        qWarning() << "Failed to create SQLite directory:" << dir.filePath(QStringLiteral("LabTester"));
    }
    return dir.filePath(QStringLiteral("LabTester/labtester.db"));
}

} // namespace labtester::database
