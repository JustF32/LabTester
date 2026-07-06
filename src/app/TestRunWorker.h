#pragma once

#include <QObject>
#include <QString>

#include <vector>

#include "domain/Submission.h"
#include "domain/TestRunResult.h"

namespace labtester::services {
class TestExecutionService;
}

namespace labtester::app {

class TestRunWorker : public QObject {
    Q_OBJECT

public:
    TestRunWorker(
        services::TestExecutionService &testExecutionService,
        std::vector<domain::Submission> submissions,
        QString runnerModeOverride = QString()
    );

public slots:
    void process();

signals:
    void progressChanged(int current, int total, const QString &label);
    void submissionFinished(int current, int total, const labtester::domain::TestRunResult &result);
    void finished();

private:
    services::TestExecutionService &m_testExecutionService;
    std::vector<domain::Submission> m_submissions;
    QString m_runnerModeOverride;
};

} // namespace labtester::app
