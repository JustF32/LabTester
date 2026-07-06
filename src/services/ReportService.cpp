#include "services/ReportService.h"

namespace labtester::services {

QString ReportService::buildSummary(const domain::TestRunResult &result) const
{
    return QStringLiteral("Пройдено тестов: %1 из %2")
        .arg(result.passedTests)
        .arg(result.totalTests);
}

} // namespace labtester::services
