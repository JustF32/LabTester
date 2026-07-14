#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QCoreApplication>
#include <QThread>
#include <QTimer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QFile>
#include <QTextStream>
#include <memory>

namespace labtester::test {

// Helper: Создать временную БД SQLite
class TestDatabase {
public:
    TestDatabase() {
        m_connectionName = QStringLiteral("TestDB_%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
        m_db.setDatabaseName(QStringLiteral(":memory:"));
        m_db.open();
        
        // Создаем схему
        QSqlQuery query(m_db);
        query.exec("CREATE TABLE student_groups (id INTEGER PRIMARY KEY, name TEXT, created_at TEXT, updated_at TEXT)");
        query.exec("CREATE TABLE students (id INTEGER PRIMARY KEY, group_id INTEGER, full_name TEXT, is_active INTEGER DEFAULT 1, created_at TEXT, updated_at TEXT)");
        query.exec("CREATE TABLE lab_works (id INTEGER PRIMARY KEY, title TEXT, description TEXT, language TEXT, manifest_rel_path TEXT, starter_code_rel_path TEXT, test_suite_rel_path TEXT, header_rel_path TEXT, is_active INTEGER DEFAULT 1, created_at TEXT, updated_at TEXT)");
        query.exec("CREATE TABLE submission_entries (id INTEGER PRIMARY KEY, student_id INTEGER, lab_work_id INTEGER, source_rel_path TEXT, source_original_name TEXT, status TEXT, is_deleted INTEGER DEFAULT 0, created_at TEXT, updated_at TEXT)");
        query.exec("CREATE TABLE check_runs (id INTEGER PRIMARY KEY, submission_id INTEGER, student_id INTEGER, lab_work_id INTEGER, execution_target_id INTEGER, status TEXT, tier_basic_passed INTEGER DEFAULT 0, tier_advanced_passed INTEGER DEFAULT 0, tier_performance_passed INTEGER DEFAULT 0, stars INTEGER DEFAULT 0, total_tests INTEGER DEFAULT 0, passed_tests INTEGER DEFAULT 0, failed_tests INTEGER DEFAULT 0, duration_ms INTEGER DEFAULT 0, runner_message TEXT, report_rel_path TEXT, is_visible_in_results INTEGER DEFAULT 1, sync_state TEXT, remote_id TEXT, created_at TEXT)");
        query.exec("CREATE TABLE check_case_results (id INTEGER PRIMARY KEY, check_run_id INTEGER, test_name TEXT, tier TEXT, passed INTEGER DEFAULT 0, input_data TEXT, expected_output TEXT, actual_output TEXT, message TEXT, failure_details TEXT, duration_ms INTEGER DEFAULT 0)");
        query.exec("CREATE TABLE settings (key TEXT PRIMARY KEY, value TEXT, updated_at TEXT)");
        query.exec("CREATE TABLE schema_migrations (version INTEGER PRIMARY KEY, name TEXT, applied_at TEXT)");
        query.exec("CREATE TABLE sync_queue (id INTEGER PRIMARY KEY, submission_id INTEGER, operation TEXT, status TEXT, retry_count INTEGER DEFAULT 0, created_at TEXT)");
    }
    
    ~TestDatabase() {
        m_db.close();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
    
    QSqlDatabase& db() { return m_db; }
    const QString& connectionName() const { return m_connectionName; }
    
private:
    QSqlDatabase m_db;
    QString m_connectionName;
};

// Helper: Создать тестового студента
domain::Student createTestStudent(int id = 1, const QString& name = "Test Student", const QString& group = "Group A") {
    domain::Student student;
    student.id = id;
    student.name = name;
    student.groupName = group;
    return student;
}

// Helper: Создать тестовую лабораторную
domain::LabWork createTestLabWork(int id = 1, const QString& title = "Lab 1") {
    domain::LabWork lab;
    lab.id = id;
    lab.title = title;
    lab.language = "cpp";
    lab.description = "Test lab";
    lab.testSuitePath = "/path/to/test.cpp";
    lab.referenceHeaderPath = "/path/to/cc.h";
    return lab;
}

// Helper: Создать тестовую работу
domain::Submission createTestSubmission(int id = 1, int studentId = 1, int labId = 1) {
    domain::Submission sub;
    sub.id = id;
    sub.student = createTestStudent(studentId);
    sub.labWork = createTestLabWork(labId);
    sub.sourcePath = "/path/to/student.cpp";
    sub.status = domain::ExecutionStatus::Pending;
    sub.createdAt = QDateTime::currentDateTime();
    return sub;
}

// Helper: Создать тестовый результат
domain::TestRunResult createTestResult(int submissionId = 1, bool passed = true) {
    domain::TestRunResult result;
    result.submissionId = submissionId;
    result.checkRunId = 100 + submissionId;
    result.studentName = "Test Student";
    result.labTitle = "Lab 1";
    result.status = passed ? domain::ExecutionStatus::Passed : domain::ExecutionStatus::Failed;
    result.totalTests = 3;
    result.passedTests = passed ? 3 : 1;
    result.failedTests = passed ? 0 : 2;
    result.message = "Test completed";
    result.executedAt = QDateTime::currentDateTime();
    result.stars = passed ? 3 : 0;
    result.displayStatus = passed ? "Passed" : "Failed";
    result.displayStatusKey = passed ? "passed" : "failed";
    
    domain::TestCaseResult tc1;
    tc1.testName = "Stack_PushPop";
    tc1.passed = true;
    tc1.message = "OK";
    tc1.inputData = "push(1), pop()";
    tc1.expectedOutput = "1";
    tc1.actualOutput = "1";
    tc1.durationMs = 10;
    result.testCases.push_back(tc1);
    
    return result;
}

} // namespace labtester::test