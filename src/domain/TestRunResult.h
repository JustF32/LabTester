#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>

#include <vector>

#include "domain/ExecutionStatus.h"
#include "domain/TestCaseResult.h"

namespace labtester::domain {

struct TestRunResult {
    int checkRunId {0};
    int submissionId {0};
    QString studentName;
    QString labTitle;
    int totalTests {0};
    int passedTests {0};
    int failedTests {0};
    int stars {0};
    ExecutionStatus status {ExecutionStatus::Pending};
    QString displayStatus;
    QString displayStatusKey;
    QString message;
    QDateTime executedAt {QDateTime::currentDateTime()};
    std::vector<TestCaseResult> testCases;
};

} // namespace labtester::domain

Q_DECLARE_METATYPE(labtester::domain::TestRunResult)
