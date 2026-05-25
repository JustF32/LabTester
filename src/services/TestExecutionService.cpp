#include "services/TestExecutionService.h"

#include <QDebug>

#include "database/ResultRepository.h"
#include "database/SubmissionRepository.h"
#include "runners/ITestRunner.h"
#include "services/ExecutionSettingsService.h"
#include "services/ReportService.h"
#include "services/WorkspaceService.h"

namespace labtester::services {

TestExecutionService::TestExecutionService(
    database::SubmissionRepository &submissionRepository,
    database::ResultRepository &resultRepository,
    runners::ITestRunner &localTestRunner,
    runners::ITestRunner &remoteTestRunner,
    ExecutionSettingsService &executionSettingsService,
    WorkspaceService &workspaceService,
    ReportService &reportService
)
    : m_submissionRepository(submissionRepository)
    , m_resultRepository(resultRepository)
    , m_localTestRunner(localTestRunner)
    , m_remoteTestRunner(remoteTestRunner)
    , m_executionSettingsService(executionSettingsService)
    , m_workspaceService(workspaceService)
    , m_reportService(reportService)
{
}

std::vector<domain::Submission> TestExecutionService::loadSubmissionsForExecution() const
{
    return m_submissionRepository.fetchAll();
}

domain::TestRunResult TestExecutionService::executeSubmission(
    const domain::Submission &submission,
    const QString &modeOverride
) const
{
    const QString workspacePath = m_workspaceService.resolveWorkspaceFor(submission);
    Q_UNUSED(workspacePath);

    const QString selectedMode = modeOverride.trimmed().isEmpty()
        ? m_executionSettingsService.executionMode()
        : modeOverride.trimmed().toLower();
    const bool useRemoteRunner =
        selectedMode == QString::fromLatin1(ExecutionSettingsService::kModeServer);
    qInfo().noquote() << QStringLiteral("[EXEC] submission=%1 student=%2 lab=%3 mode=%4")
                             .arg(submission.id)
                             .arg(submission.student.name)
                             .arg(submission.labWork.title)
                             .arg(useRemoteRunner ? QStringLiteral("server") : QStringLiteral("local"));
    runners::ITestRunner &runner = useRemoteRunner ? m_remoteTestRunner : m_localTestRunner;
    domain::TestRunResult result = runner.run(submission);
    if (result.submissionId == 0) {
        result.submissionId = submission.id;
    }
    if (result.studentName.isEmpty()) {
        result.studentName = submission.student.name;
    }
    if (result.labTitle.isEmpty()) {
        result.labTitle = submission.labWork.title;
    }
    if (result.message.isEmpty()) {
        result.message = m_reportService.buildSummary(result);
    }
    return result;
}

void TestExecutionService::applyResult(const domain::TestRunResult &result)
{
    m_resultRepository.upsert(result);
    m_submissionRepository.updateStatus(result.submissionId, result.status);
}

std::vector<domain::TestRunResult> TestExecutionService::runForAllSubmissions()
{
    const std::vector<domain::Submission> submissions = loadSubmissionsForExecution();
    std::vector<domain::TestRunResult> runResults;
    runResults.reserve(submissions.size());

    for (const auto &submission : submissions) {
        domain::TestRunResult result = executeSubmission(submission);
        applyResult(result);
        runResults.push_back(result);
    }

    return runResults;
}

std::vector<domain::TestRunResult> TestExecutionService::loadResults() const
{
    return m_resultRepository.fetchAll();
}

std::vector<domain::CheckRunHistoryEntry> TestExecutionService::loadHistory(
    const QString &groupNameFilter,
    int studentIdFilter,
    int labWorkIdFilter
) const
{
    return m_resultRepository.fetchHistory(groupNameFilter, studentIdFilter, labWorkIdFilter);
}

std::optional<domain::TestRunResult> TestExecutionService::findResultBySubmissionId(int submissionId) const
{
    return m_resultRepository.findBySubmissionId(submissionId);
}

std::optional<domain::TestRunResult> TestExecutionService::findResultByCheckRunId(int checkRunId) const
{
    return m_resultRepository.findByCheckRunId(checkRunId);
}

void TestExecutionService::clearResults()
{
    m_resultRepository.clear();
}

bool TestExecutionService::removeHistoryEntry(int checkRunId, QString *errorMessage)
{
    return m_resultRepository.removeCheckRun(checkRunId, errorMessage);
}

} // namespace labtester::services
