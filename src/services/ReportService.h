#pragma once

#include <QString>

#include "domain/TestRunResult.h"

namespace labtester::services {

class ReportService {
public:
    ReportService() = default;

    QString buildSummary(const domain::TestRunResult &result) const;
};

} // namespace labtester::services
