#pragma once

#include "runners/ITestRunner.h"

namespace labtester::services {
class ExecutionSettingsService;
}

namespace labtester::runners {

class RemoteWorkerRunner final : public ITestRunner {
public:
    explicit RemoteWorkerRunner(const services::ExecutionSettingsService &executionSettingsService);

    domain::TestRunResult run(const domain::Submission &submission) const override;

private:
    const services::ExecutionSettingsService &m_executionSettingsService;
};

} // namespace labtester::runners

