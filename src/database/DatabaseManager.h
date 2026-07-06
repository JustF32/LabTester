#pragma once

#include <QString>
#include <QtSql/QSqlDatabase>
#include <vector>

namespace labtester::database {

class DatabaseManager {
public:
    explicit DatabaseManager(QString connectionName = QStringLiteral("LabTesterConnection"));
    ~DatabaseManager();

    bool initialize();
    bool isOpen() const;
    QSqlDatabase database() const;
    QString databaseFilePath() const;

private:
    struct MigrationScript {
        int version {0};
        QString name;
        QString resourcePath;
    };

    bool open();
    bool executeInitScript();
    bool executeStatements(const QString &script);
    bool ensureMigrationTable();
    bool applyMigrations();
    std::vector<MigrationScript> collectMigrations() const;
    int currentSchemaVersion() const;
    bool markMigrationApplied(int version, const QString &name);
    QString resolveDatabasePath() const;

    QString m_connectionName;
    QSqlDatabase m_database;
    QString m_databasePath;
};

} // namespace labtester::database
