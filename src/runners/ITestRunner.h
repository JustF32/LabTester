#pragma once

#include "domain/Submission.h"
#include "domain/TestRunResult.h"

namespace labtester::runners {

class ITestRunner {
public:
    virtual ~ITestRunner() = default;
    virtual domain::TestRunResult run(const domain::Submission &submission) const = 0;
};

} // namespace labtester::runners
