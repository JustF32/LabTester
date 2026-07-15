#include "test_common.h"
#include "database/ResultRepository.h"
#include "database/SubmissionRepository.h"

using namespace labtester::database;
using namespace labtester::domain;
using namespace labtester::test;

class ResultRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_testDb = std::make_unique<TestDatabase>();
        m_subRepo = std::make_unique<SubmissionRepository>(*m_testDb);
        m_resultRepo = std::make_unique<ResultRepository>(*m_testDb);
        
        // Создаем тестовые данные
        Student student = createTestStudent(100, "Alice", "Group A");
        m_subRepo->addStudent(student, nullptr);
        
        LabWork lab = createTestLabWork(100, "Test Lab");
        m_subRepo->addLabWork(lab, nullptr);
        
        Submission sub = createTestSubmission(100, 100, 100);
        m_subRepo->add(sub);
    }
    
    std::unique_ptr<TestDatabase> m_testDb;
    std::unique_ptr<SubmissionRepository> m_subRepo;
    std::unique_ptr<ResultRepository> m_resultRepo;
};

TEST_F(ResultRepositoryTest, UpsertResult) {
    TestRunResult result = createTestResult(100, true);
    m_resultRepo->upsert(result);
    
    auto results = m_resultRepo->fetchAll();
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].submissionId, 100);
    EXPECT_EQ(results[0].status, ExecutionStatus::Passed);
    EXPECT_EQ(results[0].passedTests, 3);
}

TEST_F(ResultRepositoryTest, FetchHistory) {
    // Добавляем несколько результатов
    for (int i = 1; i <= 3; ++i) {
        TestRunResult result = createTestResult(100, i % 2 == 0);
        m_resultRepo->upsert(result);
    }
    
    auto history = m_resultRepo->fetchHistory("", 0, 0);
    ASSERT_EQ(history.size(), 3);
}

TEST_F(ResultRepositoryTest, FetchHistoryWithFilters) {
    // Создаем еще студента и работу
    Student student2 = createTestStudent(200, "Bob", "Group B");
    m_subRepo->addStudent(student2, nullptr);
    
    LabWork lab2 = createTestLabWork(200, "Lab 2");
    m_subRepo->addLabWork(lab2, nullptr);
    
    Submission sub2 = createTestSubmission(200, 200, 200);
    m_subRepo->add(sub2);
    
    // Добавляем результаты для обоих студентов
    TestRunResult result1 = createTestResult(100, true);
    m_resultRepo->upsert(result1);
    
    TestRunResult result2 = createTestResult(200, false);
    m_resultRepo->upsert(result2);
    
    // Фильтр по студенту
    auto history = m_resultRepo->fetchHistory("", 100, 0);
    ASSERT_EQ(history.size(), 1);
    EXPECT_EQ(history[0].submissionId, 100);
    
    // Фильтр по группе
    history = m_resultRepo->fetchHistory("Group A", 0, 0);
    ASSERT_EQ(history.size(), 1);
}

TEST_F(ResultRepositoryTest, FindBySubmissionId) {
    TestRunResult result = createTestResult(100, true);
    m_resultRepo->upsert(result);
    
    auto found = m_resultRepo->findBySubmissionId(100);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->submissionId, 100);
}

TEST_F(ResultRepositoryTest, FindByCheckRunId) {
    TestRunResult result = createTestResult(100, true);
    m_resultRepo->upsert(result);
    
    auto found = m_resultRepo->findByCheckRunId(101);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->checkRunId, 101);
}

TEST_F(ResultRepositoryTest, ClearResults) {
    for (int i = 1; i <= 3; ++i) {
        TestRunResult result = createTestResult(100, true);
        m_resultRepo->upsert(result);
    }
    
    // Проверяем, что результаты есть
    auto results = m_resultRepo->fetchAll();
    ASSERT_EQ(results.size(), 3);
    
    m_resultRepo->clear();
    
    results = m_resultRepo->fetchAll();
    EXPECT_TRUE(results.empty());
}

TEST_F(ResultRepositoryTest, RemoveCheckRun) {
    TestRunResult result = createTestResult(100, true);
    m_resultRepo->upsert(result);
    
    bool removed = m_resultRepo->removeCheckRun(101, nullptr);
    ASSERT_TRUE(removed);
    
    auto results = m_resultRepo->fetchAll();
    EXPECT_TRUE(results.empty());
}

TEST_F(ResultRepositoryTest, RemoveNonExistentCheckRun) {
    bool removed = m_resultRepo->removeCheckRun(999, nullptr);
    EXPECT_FALSE(removed);
}