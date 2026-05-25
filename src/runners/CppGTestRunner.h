#pragma once

#include "runners/ITestRunner.h"

namespace labtester::runners {

class CppGTestRunner final : public ITestRunner {
public:
    domain::TestRunResult run(const domain::Submission &submission) const override;
};

} // namespace labtester::runners
