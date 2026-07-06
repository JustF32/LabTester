#pragma once

#include <optional>
#include <vector>

#include <QString>

#include "domain/CheckRunHistoryEntry.h"
#include "domain/Submission.h"
#include "domain/TestRunResult.h"

namespace labtester::database {
class SubmissionRepository;
class ResultRepository;
}

namespace labtester::runners {
class ITestRunner;
}

namespace labtester::services {

class WorkspaceService;
class ReportService;
class ExecutionSettingsService;

class TestExecutionService {
public:
    TestExecutionService(
        database::SubmissionRepository &submissionRepository,
        database::ResultRepository &resultRepository,
        runners::ITestRunner &localTestRunner,
        runners::ITestRunner &remoteTestRunner,
        ExecutionSettingsService &executionSettingsService,
        WorkspaceService &workspaceService,
        ReportService &reportService
    );

    std::vector<domain::Submission> loadSubmissionsForExecution() const;
    domain::TestRunResult executeSubmission(
        const domain::Submission &submission,
        const QString &modeOverride = QString()
    ) const;
    void applyResult(const domain::TestRunResult &result);

    std::vector<domain::TestRunResult> runForAllSubmissions();
    std::vector<domain::TestRunResult> loadResults() const;
    std::vector<domain::CheckRunHistoryEntry> loadHistory(
        const QString &groupNameFilter,
        int studentIdFilter,
        int labWorkIdFilter
    ) const;
    std::optional<domain::TestRunResult> findResultBySubmissionId(int submissionId) const;
    std::optional<domain::TestRunResult> findResultByCheckRunId(int checkRunId) const;
    void clearResults();
    bool removeHistoryEntry(int checkRunId, QString *errorMessage = nullptr);

private:
    database::SubmissionRepository &m_submissionRepository;
    database::ResultRepository &m_resultRepository;
    runners::ITestRunner &m_localTestRunner;
    runners::ITestRunner &m_remoteTestRunner;
    ExecutionSettingsService &m_executionSettingsService;
    WorkspaceService &m_workspaceService;
    ReportService &m_reportService;
};

} // namespace labtester::services
