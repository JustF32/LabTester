#include <gtest/gtest.h>
#include "services/WorkspaceService.h"
#include "test_common.h"

using namespace labtester::services;
using namespace labtester::domain;
using namespace labtester::test;

class WorkspaceServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_service = std::make_unique<WorkspaceService>();
    }
    
    std::unique_ptr<WorkspaceService> m_service;
};

TEST_F(WorkspaceServiceTest, ResolveWorkspaceForSubmissionWithSourcePath) {
    Submission sub = createTestSubmission(1);
    sub.sourcePath = "/path/to/source.cpp";
    
    QString workspace = m_service->resolveWorkspaceFor(sub);
    EXPECT_EQ(workspace, sub.sourcePath);
}

TEST_F(WorkspaceServiceTest, ResolveWorkspaceForSubmissionWithoutSourcePath) {
    Submission sub = createTestSubmission(1);
    sub.sourcePath = "";
    
    QString workspace = m_service->resolveWorkspaceFor(sub);
    EXPECT_TRUE(workspace.contains("workspace/submission_1"));
}

TEST_F(WorkspaceServiceTest, ResolveWorkspaceWithDifferentSubmissionId) {
    Submission sub = createTestSubmission(42);
    sub.sourcePath = "";
    
    QString workspace = m_service->resolveWorkspaceFor(sub);
    EXPECT_TRUE(workspace.contains("workspace/submission_42"));
}