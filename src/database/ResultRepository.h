#pragma once

#include <optional>
#include <vector>

#include <QString>

#include "domain/CheckRunHistoryEntry.h"
#include "domain/TestRunResult.h"

namespace labtester::database {

class DatabaseManager;

class ResultRepository {
public:
    explicit ResultRepository(DatabaseManager &databaseManager);

    std::vector<domain::TestRunResult> fetchAll() const;
    std::vector<domain::CheckRunHistoryEntry> fetchHistory(
        const QString &groupNameFilter,
        int studentIdFilter,
        int labWorkIdFilter
    ) const;
    void upsert(const domain::TestRunResult &result);
    void clear();
    bool removeCheckRun(int checkRunId, QString *errorMessage = nullptr);
    std::optional<domain::TestRunResult> findBySubmissionId(int submissionId) const;
    std::optional<domain::TestRunResult> findByCheckRunId(int checkRunId) const;

private:
    std::optional<domain::TestRunResult> fetchLatestBySubmissionId(int submissionId) const;
    std::optional<domain::TestRunResult> fetchByCheckRunId(int checkRunId) const;

    DatabaseManager &m_databaseManager;
};

} // namespace labtester::database
