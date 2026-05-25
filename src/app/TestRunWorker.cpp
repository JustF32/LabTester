#include "app/TestRunWorker.h"

#include <QThread>

#include <utility>

#include "services/TestExecutionService.h"

namespace labtester::app {

TestRunWorker::TestRunWorker(
    services::TestExecutionService &testExecutionService,
    std::vector<domain::Submission> submissions,
    QString runnerModeOverride
)
    : m_testExecutionService(testExecutionService)
    , m_submissions(std::move(submissions))
    , m_runnerModeOverride(std::move(runnerModeOverride))
{
}

void TestRunWorker::process()
{
    const int total = static_cast<int>(m_submissions.size());

    for (int index = 0; index < total; ++index) {
        if (QThread::currentThread()->isInterruptionRequested()) {
            break;
        }

        const auto &submission = m_submissions[static_cast<size_t>(index)];
        const QString label = QStringLiteral("%1 - %2")
            .arg(submission.student.name)
            .arg(submission.labWork.title);

        emit progressChanged(index, total, label);

        if (QThread::currentThread()->isInterruptionRequested()) {
            break;
        }

        const domain::TestRunResult result =
            m_testExecutionService.executeSubmission(submission, m_runnerModeOverride);
        if (QThread::currentThread()->isInterruptionRequested()) {
            break;
        }
        emit submissionFinished(index + 1, total, result);
    }

    emit finished();
}

} // namespace labtester::app
