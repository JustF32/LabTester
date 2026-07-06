#pragma once

#include <QDateTime>
#include <QString>

#include "domain/ExecutionStatus.h"

namespace labtester::domain {

struct CheckRunHistoryEntry {
    int checkRunId {0};
    int submissionId {0};
    QString studentName;
    QString groupName;
    QString labTitle;
    int totalTests {0};
    int passedTests {0};
    int failedTests {0};
    int stars {0};
    ExecutionStatus status {ExecutionStatus::Pending};
    QString statusText;
    QString statusKey;
    QString message;
    QDateTime executedAt {QDateTime::currentDateTime()};
};

} // namespace labtester::domain
