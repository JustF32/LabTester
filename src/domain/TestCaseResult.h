#pragma once

#include <QMetaType>
#include <QString>
#include <QtGlobal>

namespace labtester::domain {

struct TestCaseResult {
    QString testName;
    bool passed {false};
    QString message;
    QString inputData;
    QString expectedOutput;
    QString actualOutput;
    QString failureDetails;
    qint64 durationMs {0};
};

} // namespace labtester::domain

Q_DECLARE_METATYPE(labtester::domain::TestCaseResult)
