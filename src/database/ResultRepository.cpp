#include "database/ResultRepository.h"

#include <QDateTime>
#include <QDebug>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include "database/DatabaseManager.h"
#include "domain/ExecutionStatus.h"

namespace {

QDateTime parseDateTime(const QString &value)
{
    if (value.trimmed().isEmpty()) {
        return QDateTime::currentDateTime();
    }

    QDateTime parsed = QDateTime::fromString(value, Qt::ISODateWithMs);
    if (!parsed.isValid()) {
        parsed = QDateTime::fromString(value, Qt::ISODate);
    }
    if (!parsed.isValid()) {
        parsed = QDateTime::fromString(value, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    }
    if (!parsed.isValid()) {
        parsed = QDateTime::currentDateTime();
    }
    return parsed;
}

int countCyrillicLetters(const QString &value)
{
    int total = 0;
    for (const QChar ch : value) {
        const ushort code = ch.unicode();
        if ((code >= 0x0400 && code <= 0x04FF) || (code >= 0x0500 && code <= 0x052F)) {
            ++total;
        }
    }
    return total;
}

bool looksLikeUtf8Mojibake(const QString &value)
{
    if (value.size() < 2) {
        return false;
    }

    int markers = 0;
    for (const QChar ch : value) {
        const ushort code = ch.unicode();
        if (code == 0x00D0 || code == 0x00D1 || code == 0x00D2 || code == 0x00D3) {
            ++markers;
        }
    }
    return markers >= 2 && (markers * 3 >= value.size());
}

QString repairUtf8Mojibake(const QString &value)
{
    if (!looksLikeUtf8Mojibake(value)) {
        return value;
    }

    const QByteArray latin1Bytes = value.toLatin1();
    const QString repaired = QString::fromUtf8(latin1Bytes);
    if (repaired.contains(QChar::ReplacementCharacter)) {
        return value;
    }

    return countCyrillicLetters(repaired) > countCyrillicLetters(value)
        ? repaired
        : value;
}

struct TierEvaluation {
    bool basicPassed {true};
    bool advancedPassed {true};
    bool performancePassed {true};
    int stars {0};
    QString statusText;
    QString statusKey;
};

enum class TestTier {
    Basic,
    Advanced,
    Performance
};

std::optional<TestTier> explicitTierForTestName(const QString &testName)
{
    const QString suiteName = testName.section('.', 0, 0);
    const QString normalized = suiteName + QStringLiteral(" ") + testName.section('.', 1);

    if (normalized.contains(QStringLiteral("_Performance"), Qt::CaseInsensitive)
        || normalized.contains(QStringLiteral("Performance"), Qt::CaseInsensitive)) {
        return TestTier::Performance;
    }
    if (normalized.contains(QStringLiteral("_Advanced"), Qt::CaseInsensitive)
        || normalized.contains(QStringLiteral("Advanced"), Qt::CaseInsensitive)) {
        return TestTier::Advanced;
    }
    if (normalized.contains(QStringLiteral("_Basic"), Qt::CaseInsensitive)
        || normalized.contains(QStringLiteral("Basic"), Qt::CaseInsensitive)) {
        return TestTier::Basic;
    }

    return std::nullopt;
}

QString tierStorageValue(const QString &testName, int oneBasedIndex)
{
    const std::optional<TestTier> explicitTier = explicitTierForTestName(testName);
    const TestTier tier = explicitTier.has_value()
        ? *explicitTier
        : (oneBasedIndex <= 10
            ? TestTier::Basic
            : (oneBasedIndex <= 15 ? TestTier::Advanced : TestTier::Performance));

    switch (tier) {
    case TestTier::Basic:
        return QStringLiteral("Basic");
    case TestTier::Advanced:
        return QStringLiteral("Advanced");
    case TestTier::Performance:
        return QStringLiteral("Performance");
    }
    return QStringLiteral("Basic");
}

TierEvaluation evaluateByPolicy(
    const labtester::domain::ExecutionStatus status,
    const std::vector<labtester::domain::TestCaseResult> &testCases
)
{
    TierEvaluation evaluation;

    if (status == labtester::domain::ExecutionStatus::BuildError) {
        evaluation.basicPassed = false;
        evaluation.advancedPassed = false;
        evaluation.performancePassed = false;
    } else {
        bool hasExplicitTier = false;
        for (const auto &testCase : testCases) {
            if (explicitTierForTestName(testCase.testName).has_value()) {
                hasExplicitTier = true;
                break;
            }
        }

        for (size_t index = 0; index < testCases.size(); ++index) {
            const bool passed = testCases[index].passed;
            if (passed) {
                continue;
            }

            const int oneBased = static_cast<int>(index) + 1;
            const TestTier tier = hasExplicitTier
                ? explicitTierForTestName(testCases[index].testName).value_or(TestTier::Basic)
                : (oneBased <= 10
                    ? TestTier::Basic
                    : (oneBased <= 15 ? TestTier::Advanced : TestTier::Performance));

            if (tier == TestTier::Basic) {
                evaluation.basicPassed = false;
            } else if (tier == TestTier::Advanced) {
                evaluation.advancedPassed = false;
            } else {
                evaluation.performancePassed = false;
            }
        }
    }

    if (!evaluation.basicPassed) {
        evaluation.stars = 0;
        evaluation.statusText = QStringLiteral("Провалено");
        evaluation.statusKey = QStringLiteral("Failed");
    } else if (!evaluation.advancedPassed) {
        evaluation.stars = 1;
        evaluation.statusText = QStringLiteral("Пройдены базовые тесты");
        evaluation.statusKey = QStringLiteral("BasicOnly");
    } else if (!evaluation.performancePassed) {
        evaluation.stars = 2;
        evaluation.statusText = QStringLiteral("Не оптимальное решение");
        evaluation.statusKey = QStringLiteral("NeedsOptimization");
    } else {
        evaluation.stars = 3;
        evaluation.statusText = QStringLiteral("Полностью решено");
        evaluation.statusKey = QStringLiteral("FullySolved");
    }

    return evaluation;
}

TierEvaluation evaluateByPolicy(
    const labtester::domain::ExecutionStatus status,
    bool basicPassed,
    bool advancedPassed,
    bool performancePassed
)
{
    TierEvaluation evaluation;
    evaluation.basicPassed = basicPassed;
    evaluation.advancedPassed = advancedPassed;
    evaluation.performancePassed = performancePassed;

    if (status == labtester::domain::ExecutionStatus::BuildError) {
        evaluation.basicPassed = false;
        evaluation.advancedPassed = false;
        evaluation.performancePassed = false;
    }

    if (!evaluation.basicPassed) {
        evaluation.stars = 0;
        evaluation.statusText = QStringLiteral("Провалено");
        evaluation.statusKey = QStringLiteral("Failed");
    } else if (!evaluation.advancedPassed) {
        evaluation.stars = 1;
        evaluation.statusText = QStringLiteral("Пройдены базовые тесты");
        evaluation.statusKey = QStringLiteral("BasicOnly");
    } else if (!evaluation.performancePassed) {
        evaluation.stars = 2;
        evaluation.statusText = QStringLiteral("Не оптимальное решение");
        evaluation.statusKey = QStringLiteral("NeedsOptimization");
    } else {
        evaluation.stars = 3;
        evaluation.statusText = QStringLiteral("Полностью решено");
        evaluation.statusKey = QStringLiteral("FullySolved");
    }

    return evaluation;
}

} // namespace

namespace labtester::database {

ResultRepository::ResultRepository(DatabaseManager &databaseManager)
    : m_databaseManager(databaseManager)
{
}

std::vector<domain::TestRunResult> ResultRepository::fetchAll() const
{
    std::vector<domain::TestRunResult> results;

    QSqlQuery query(m_databaseManager.database());
    query.prepare(QStringLiteral(
        "SELECT MAX(cr.id) "
        "FROM check_runs cr "
        "JOIN submission_entries se ON se.id = cr.submission_id "
        "WHERE se.is_deleted = 0 "
        "  AND COALESCE(cr.is_visible_in_results, 1) = 1 "
        "GROUP BY cr.submission_id "
        "ORDER BY cr.submission_id"
    ));

    if (!query.exec()) {
        qWarning() << "Failed to fetch visible result check runs:" << query.lastError().text();
        return results;
    }

    while (query.next()) {
        const int checkRunId = query.value(0).toInt();
        const std::optional<domain::TestRunResult> result = fetchByCheckRunId(checkRunId);
        if (result.has_value()) {
            results.push_back(*result);
        }
    }

    return results;
}

std::vector<domain::CheckRunHistoryEntry> ResultRepository::fetchHistory(
    const QString &groupNameFilter,
    int studentIdFilter,
    int labWorkIdFilter
) const
{
    std::vector<domain::CheckRunHistoryEntry> history;

    QSqlQuery query(m_databaseManager.database());
    query.prepare(QStringLiteral(
        "SELECT "
        "cr.id, cr.submission_id, COALESCE(s.full_name, ''), COALESCE(g.name, ''), "
        "COALESCE(l.title, ''), cr.total_tests, cr.passed_tests, cr.failed_tests, "
        "cr.status, COALESCE(cr.runner_message, ''), COALESCE(cr.created_at, ''), COALESCE(cr.stars, 0), "
        "COALESCE(cr.tier_basic_passed, 0), COALESCE(cr.tier_advanced_passed, 0), COALESCE(cr.tier_performance_passed, 0) "
        "FROM check_runs cr "
        "LEFT JOIN students s ON s.id = cr.student_id "
        "LEFT JOIN student_groups g ON g.id = s.group_id "
        "LEFT JOIN lab_works l ON l.id = cr.lab_work_id "
        "WHERE (:student_id1 = 0 OR cr.student_id = :student_id2) "
        "  AND (:lab_work_id1 = 0 OR cr.lab_work_id = :lab_work_id2) "
        "  AND (:group_name1 = '' OR lower(COALESCE(g.name, '')) = lower(:group_name2)) "
        "ORDER BY cr.id DESC"
    ));
    
    const int actualStudentId = studentIdFilter > 0 ? studentIdFilter : 0;
    query.bindValue(QStringLiteral(":student_id1"), actualStudentId);
    query.bindValue(QStringLiteral(":student_id2"), actualStudentId);
    
    const int actualLabWorkId = labWorkIdFilter > 0 ? labWorkIdFilter : 0;
    query.bindValue(QStringLiteral(":lab_work_id1"), actualLabWorkId);
    query.bindValue(QStringLiteral(":lab_work_id2"), actualLabWorkId);
    
    const QString actualGroupName = groupNameFilter.trimmed();
    query.bindValue(QStringLiteral(":group_name1"), actualGroupName);
    query.bindValue(QStringLiteral(":group_name2"), actualGroupName);

    if (!query.exec()) {
        qWarning() << "Failed to fetch history:" << query.lastError().text();
        return history;
    }

    while (query.next()) {
        domain::CheckRunHistoryEntry entry;
        entry.checkRunId = query.value(0).toInt();
        entry.submissionId = query.value(1).toInt();
        entry.studentName = repairUtf8Mojibake(query.value(2).toString());
        entry.groupName = repairUtf8Mojibake(query.value(3).toString());
        entry.labTitle = repairUtf8Mojibake(query.value(4).toString());
        entry.totalTests = query.value(5).toInt();
        entry.passedTests = query.value(6).toInt();
        entry.failedTests = query.value(7).toInt();
        entry.status = domain::executionStatusFromString(query.value(8).toString());
        entry.message = query.value(9).toString();
        entry.executedAt = parseDateTime(query.value(10).toString());
        const bool basicPassed = query.value(12).toInt() != 0;
        const bool advancedPassed = query.value(13).toInt() != 0;
        const bool performancePassed = query.value(14).toInt() != 0;
        const TierEvaluation evaluation = evaluateByPolicy(
            entry.status,
            basicPassed,
            advancedPassed,
            performancePassed
        );
        entry.stars = evaluation.stars;
        entry.statusText = evaluation.statusText;
        entry.statusKey = evaluation.statusKey;
        history.push_back(entry);
    }

    return history;
}

void ResultRepository::upsert(const domain::TestRunResult &result)
{
    if (result.submissionId <= 0) {
        qWarning() << "ResultRepository::upsert ignored result with empty submissionId";
        return;
    }

    QSqlDatabase db = m_databaseManager.database();
    if (!db.transaction()) {
        qWarning() << "Failed to start transaction for result upsert:" << db.lastError().text();
        return;
    }

    QSqlQuery lookupQuery(db);
    lookupQuery.prepare(QStringLiteral(
        "SELECT student_id, lab_work_id "
        "FROM submission_entries "
        "WHERE id = :submission_id "
        "LIMIT 1"
    ));
    lookupQuery.bindValue(QStringLiteral(":submission_id"), result.submissionId);
    if (!lookupQuery.exec() || !lookupQuery.next()) {
        qWarning() << "Failed to resolve submission for result:" << result.submissionId << lookupQuery.lastError().text();
        db.rollback();
        return;
    }

    const int studentId = lookupQuery.value(0).toInt();
    const int labWorkId = lookupQuery.value(1).toInt();
    const TierEvaluation evaluation = evaluateByPolicy(result.status, result.testCases);

    QSqlQuery insertRunQuery(db);
    insertRunQuery.prepare(QStringLiteral(
        "INSERT INTO check_runs("
        "submission_id, student_id, lab_work_id, execution_target_id, status, "
        "tier_basic_passed, tier_advanced_passed, tier_performance_passed, stars, "
        "total_tests, passed_tests, failed_tests, duration_ms, runner_message, report_rel_path, is_visible_in_results, "
        "sync_state, remote_id, created_at"
        ") VALUES ("
        ":submission_id, :student_id, :lab_work_id, NULL, :status, "
        ":tier_basic_passed, :tier_advanced_passed, :tier_performance_passed, :stars, "
        ":total_tests, :passed_tests, :failed_tests, :duration_ms, :runner_message, NULL, 1, "
        "'LocalOnly', NULL, :created_at"
        ")"
    ));
    insertRunQuery.bindValue(QStringLiteral(":submission_id"), result.submissionId);
    insertRunQuery.bindValue(QStringLiteral(":student_id"), studentId);
    insertRunQuery.bindValue(QStringLiteral(":lab_work_id"), labWorkId);
    insertRunQuery.bindValue(QStringLiteral(":status"), domain::toStorageString(result.status));
    insertRunQuery.bindValue(QStringLiteral(":tier_basic_passed"), evaluation.basicPassed ? 1 : 0);
    insertRunQuery.bindValue(QStringLiteral(":tier_advanced_passed"), evaluation.advancedPassed ? 1 : 0);
    insertRunQuery.bindValue(QStringLiteral(":tier_performance_passed"), evaluation.performancePassed ? 1 : 0);
    insertRunQuery.bindValue(QStringLiteral(":stars"), evaluation.stars);
    insertRunQuery.bindValue(QStringLiteral(":total_tests"), result.totalTests);
    insertRunQuery.bindValue(QStringLiteral(":passed_tests"), result.passedTests);
    insertRunQuery.bindValue(QStringLiteral(":failed_tests"), result.failedTests);
    insertRunQuery.bindValue(QStringLiteral(":duration_ms"), 0);
    insertRunQuery.bindValue(QStringLiteral(":runner_message"), result.message);
    insertRunQuery.bindValue(
        QStringLiteral(":created_at"),
        (result.executedAt.isValid() ? result.executedAt : QDateTime::currentDateTime()).toString(Qt::ISODateWithMs)
    );

    if (!insertRunQuery.exec()) {
        qWarning() << "Failed to insert check run:" << insertRunQuery.lastError().text();
        db.rollback();
        return;
    }

    const int checkRunId = insertRunQuery.lastInsertId().toInt();
    for (const auto &testCase : result.testCases) {
        QSqlQuery insertCaseQuery(db);
        insertCaseQuery.prepare(QStringLiteral(
            "INSERT INTO check_case_results("
            "check_run_id, test_name, tier, passed, input_data, expected_output, actual_output, "
            "message, failure_details, duration_ms"
            ") VALUES ("
            ":check_run_id, :test_name, :tier, :passed, :input_data, :expected_output, :actual_output, "
            ":message, :failure_details, :duration_ms"
            ")"
        ));
        insertCaseQuery.bindValue(QStringLiteral(":check_run_id"), checkRunId);
        insertCaseQuery.bindValue(QStringLiteral(":test_name"), testCase.testName);
        insertCaseQuery.bindValue(
            QStringLiteral(":tier"),
            tierStorageValue(testCase.testName, static_cast<int>(&testCase - result.testCases.data()) + 1)
        );
        insertCaseQuery.bindValue(QStringLiteral(":passed"), testCase.passed ? 1 : 0);
        insertCaseQuery.bindValue(QStringLiteral(":input_data"), testCase.inputData);
        insertCaseQuery.bindValue(QStringLiteral(":expected_output"), testCase.expectedOutput);
        insertCaseQuery.bindValue(QStringLiteral(":actual_output"), testCase.actualOutput);
        insertCaseQuery.bindValue(QStringLiteral(":message"), testCase.message);
        insertCaseQuery.bindValue(QStringLiteral(":failure_details"), testCase.failureDetails);
        insertCaseQuery.bindValue(QStringLiteral(":duration_ms"), static_cast<qlonglong>(testCase.durationMs));

        if (!insertCaseQuery.exec()) {
            qWarning() << "Failed to insert check case result:" << insertCaseQuery.lastError().text();
            db.rollback();
            return;
        }
    }

    if (!db.commit()) {
        qWarning() << "Failed to commit result upsert:" << db.lastError().text();
        return;
    }

    QSqlQuery statusUpdateQuery(db);
    statusUpdateQuery.prepare(QStringLiteral(
        "UPDATE submission_entries "
        "SET status = :status, updated_at = CURRENT_TIMESTAMP "
        "WHERE id = :id"
    ));
    statusUpdateQuery.bindValue(QStringLiteral(":status"), domain::toStorageString(result.status));
    statusUpdateQuery.bindValue(QStringLiteral(":id"), result.submissionId);
    if (!statusUpdateQuery.exec()) {
        qWarning() << "Failed to update submission status after result insert:" << statusUpdateQuery.lastError().text();
    }
}

void ResultRepository::clear()
{
    QSqlQuery hideQuery(m_databaseManager.database());
    if (!hideQuery.exec(QStringLiteral(
            "UPDATE check_runs "
            "SET is_visible_in_results = 0 "
            "WHERE COALESCE(is_visible_in_results, 1) = 1"
        ))) {
        qWarning() << "Failed to hide results from checks list:" << hideQuery.lastError().text();
    }
}

bool ResultRepository::removeCheckRun(int checkRunId, QString *errorMessage)
{
    if (checkRunId <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Некорректный идентификатор запуска.");
        }
        return false;
    }

    QSqlDatabase db = m_databaseManager.database();
    if (!db.transaction()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось начать транзакцию удаления: %1").arg(db.lastError().text());
        }
        return false;
    }

    QSqlQuery deleteCases(db);
    deleteCases.prepare(QStringLiteral("DELETE FROM check_case_results WHERE check_run_id = :check_run_id"));
    deleteCases.bindValue(QStringLiteral(":check_run_id"), checkRunId);
    if (!deleteCases.exec()) {
        db.rollback();
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось удалить тесты запуска: %1").arg(deleteCases.lastError().text());
        }
        return false;
    }

    QSqlQuery deleteRun(db);
    deleteRun.prepare(QStringLiteral("DELETE FROM check_runs WHERE id = :check_run_id"));
    deleteRun.bindValue(QStringLiteral(":check_run_id"), checkRunId);
    if (!deleteRun.exec()) {
        db.rollback();
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось удалить запуск: %1").arg(deleteRun.lastError().text());
        }
        return false;
    }

    if (deleteRun.numRowsAffected() <= 0) {
        db.rollback();
        if (errorMessage) {
            *errorMessage = QStringLiteral("Запуск не найден.");
        }
        return false;
    }

    if (!db.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось завершить удаление: %1").arg(db.lastError().text());
        }
        return false;
    }

    return true;
}

std::optional<domain::TestRunResult> ResultRepository::findBySubmissionId(int submissionId) const
{
    return fetchLatestBySubmissionId(submissionId);
}

std::optional<domain::TestRunResult> ResultRepository::findByCheckRunId(int checkRunId) const
{
    return fetchByCheckRunId(checkRunId);
}

std::optional<domain::TestRunResult> ResultRepository::fetchLatestBySubmissionId(int submissionId) const
{
    QSqlQuery runQuery(m_databaseManager.database());
    runQuery.prepare(QStringLiteral(
        "SELECT cr.id "
        "FROM check_runs cr "
        "WHERE cr.submission_id = :submission_id "
        "ORDER BY cr.id DESC "
        "LIMIT 1"
    ));
    runQuery.bindValue(QStringLiteral(":submission_id"), submissionId);

    if (!runQuery.exec()) {
        qWarning() << "Failed to fetch latest check run for submission:" << submissionId << runQuery.lastError().text();
        return std::nullopt;
    }
    if (!runQuery.next()) {
        return std::nullopt;
    }

    const int checkRunId = runQuery.value(0).toInt();
    return fetchByCheckRunId(checkRunId);
}

std::optional<domain::TestRunResult> ResultRepository::fetchByCheckRunId(int checkRunId) const
{
    QSqlQuery runQuery(m_databaseManager.database());
    runQuery.prepare(QStringLiteral(
        "SELECT "
        "cr.id, cr.submission_id, COALESCE(s.full_name, ''), COALESCE(l.title, ''), "
        "cr.total_tests, cr.passed_tests, cr.failed_tests, cr.status, COALESCE(cr.runner_message, ''), "
        "COALESCE(cr.created_at, ''), COALESCE(cr.tier_basic_passed, 0), "
        "COALESCE(cr.tier_advanced_passed, 0), COALESCE(cr.tier_performance_passed, 0) "
        "FROM check_runs cr "
        "LEFT JOIN students s ON s.id = cr.student_id "
        "LEFT JOIN lab_works l ON l.id = cr.lab_work_id "
        "WHERE cr.id = :check_run_id "
        "LIMIT 1"
    ));
    runQuery.bindValue(QStringLiteral(":check_run_id"), checkRunId);

    if (!runQuery.exec()) {
        qWarning() << "Failed to fetch check run by id:" << checkRunId << runQuery.lastError().text();
        return std::nullopt;
    }
    if (!runQuery.next()) {
        return std::nullopt;
    }

    domain::TestRunResult result;
    result.checkRunId = runQuery.value(0).toInt();
    result.submissionId = runQuery.value(1).toInt();
    result.studentName = repairUtf8Mojibake(runQuery.value(2).toString());
    result.labTitle = repairUtf8Mojibake(runQuery.value(3).toString());
    result.totalTests = runQuery.value(4).toInt();
    result.passedTests = runQuery.value(5).toInt();
    result.failedTests = runQuery.value(6).toInt();
    result.status = domain::executionStatusFromString(runQuery.value(7).toString());
    result.message = runQuery.value(8).toString();
    result.executedAt = parseDateTime(runQuery.value(9).toString());
    const bool basicPassed = runQuery.value(10).toInt() != 0;
    const bool advancedPassed = runQuery.value(11).toInt() != 0;
    const bool performancePassed = runQuery.value(12).toInt() != 0;
    const TierEvaluation evaluation = evaluateByPolicy(
        result.status,
        basicPassed,
        advancedPassed,
        performancePassed
    );
    result.stars = evaluation.stars;
    result.displayStatus = evaluation.statusText;
    result.displayStatusKey = evaluation.statusKey;

    QSqlQuery caseQuery(m_databaseManager.database());
    caseQuery.prepare(QStringLiteral(
        "SELECT test_name, passed, COALESCE(message, ''), COALESCE(input_data, ''), "
        "COALESCE(expected_output, ''), COALESCE(actual_output, ''), COALESCE(failure_details, ''), "
        "COALESCE(duration_ms, 0) "
        "FROM check_case_results "
        "WHERE check_run_id = :check_run_id "
        "ORDER BY id"
    ));
    caseQuery.bindValue(QStringLiteral(":check_run_id"), checkRunId);

    if (!caseQuery.exec()) {
        qWarning() << "Failed to fetch check case results for run:" << checkRunId << caseQuery.lastError().text();
        return result;
    }

    while (caseQuery.next()) {
        domain::TestCaseResult testCase;
        testCase.testName = caseQuery.value(0).toString();
        testCase.passed = caseQuery.value(1).toInt() != 0;
        testCase.message = caseQuery.value(2).toString();
        testCase.inputData = caseQuery.value(3).toString();
        testCase.expectedOutput = caseQuery.value(4).toString();
        testCase.actualOutput = caseQuery.value(5).toString();
        testCase.failureDetails = caseQuery.value(6).toString();
        testCase.durationMs = caseQuery.value(7).toLongLong();
        result.testCases.push_back(testCase);
    }

    return result;
}

} // namespace labtester::database
