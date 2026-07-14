#include <gtest/gtest.h>
#include "services/ReportService.h"
#include "test_common.h"

using namespace labtester::services;
using namespace labtester::domain;
using namespace labtester::test;

class ReportServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_service = std::make_unique<ReportService>();
    }
    
    std::unique_ptr<ReportService> m_service;
};

TEST_F(ReportServiceTest, BuildSummary) {
    TestRunResult result = createTestResult(1, true);
    result.totalTests = 5;
    result.passedTests = 3;
    
    QString summary = m_service->buildSummary(result);
    EXPECT_TRUE(summary.contains("3"));
    EXPECT_TRUE(summary.contains("5"));
}

TEST_F(ReportServiceTest, BuildSummaryWithZeroTests) {
    TestRunResult result = createTestResult(1, false);
    result.totalTests = 0;
    result.passedTests = 0;
    
    QString summary = m_service->buildSummary(result);
    EXPECT_TRUE(summary.contains("0"));
}

TEST_F(ReportServiceTest, BuildSummaryWithAllPassed) {
    TestRunResult result = createTestResult(1, true);
    result.totalTests = 10;
    result.passedTests = 10;
    
    QString summary = m_service->buildSummary(result);
    EXPECT_TRUE(summary.contains("10"));
    EXPECT_FALSE(summary.contains("Провалено"));
}