#include "test_common.h"
#include "database/DatabaseManager.h"

using namespace labtester::database;
using namespace labtester::test;

class DatabaseManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Используем in-memory БД
        m_dbManager = std::make_unique<DatabaseManager>(
            QStringLiteral("TestDB_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces))
        );
    }
    
    std::unique_ptr<DatabaseManager> m_dbManager;
};

TEST_F(DatabaseManagerTest, InitializeCreatesTables) {
    ASSERT_TRUE(m_dbManager->initialize());
    ASSERT_TRUE(m_dbManager->isOpen());
    
    QSqlQuery query(m_dbManager->database());
    query.exec("SELECT name FROM sqlite_master WHERE type='table'");
    
    QSet<QString> tables;
    while (query.next()) {
        tables.insert(query.value(0).toString());
    }
    
    EXPECT_TRUE(tables.contains("student_groups"));
    EXPECT_TRUE(tables.contains("students"));
    EXPECT_TRUE(tables.contains("lab_works"));
    EXPECT_TRUE(tables.contains("submission_entries"));
    EXPECT_TRUE(tables.contains("check_runs"));
    EXPECT_TRUE(tables.contains("check_case_results"));
    EXPECT_TRUE(tables.contains("settings"));
    EXPECT_TRUE(tables.contains("schema_migrations"));
}

TEST_F(DatabaseManagerTest, DatabasePathIsSet) {
    ASSERT_TRUE(m_dbManager->initialize());
    EXPECT_FALSE(m_dbManager->databaseFilePath().isEmpty());
}

TEST_F(DatabaseManagerTest, MultipleInitializeCallsAreSafe) {
    ASSERT_TRUE(m_dbManager->initialize());
    ASSERT_TRUE(m_dbManager->isOpen());
    
    // Второй вызов не должен вызвать проблем
    EXPECT_TRUE(m_dbManager->initialize());
}

TEST_F(DatabaseManagerTest, ApplyMigrations) {
    ASSERT_TRUE(m_dbManager->initialize());
    
    QSqlQuery query(m_dbManager->database());
    query.exec("SELECT version FROM schema_migrations");
    int maxVersion = 0;
    while (query.next()) {
        maxVersion = std::max(maxVersion, query.value(0).toInt());
    }
    
    // Должны быть применены все миграции
    EXPECT_GT(maxVersion, 0);
}