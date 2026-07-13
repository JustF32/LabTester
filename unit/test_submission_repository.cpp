#include "test_common.h"
#include "database/SubmissionRepository.h"

using namespace labtester::database;
using namespace labtester::domain;
using namespace labtester::test;

class SubmissionRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_testDb = std::make_unique<TestDatabase>();
        m_repo = std::make_unique<SubmissionRepository>(*m_testDb);
    }
    
    std::unique_ptr<TestDatabase> m_testDb;
    std::unique_ptr<SubmissionRepository> m_repo;
};

TEST_F(SubmissionRepositoryTest, AddAndFetchStudent) {
    Student student = createTestStudent(100, "Alice", "Group A");
    
    bool result = m_repo->addStudent(student, nullptr);
    ASSERT_TRUE(result);
    
    auto students = m_repo->fetchStudents();
    ASSERT_EQ(students.size(), 1);
    EXPECT_EQ(students[0].id, 100);
    EXPECT_EQ(students[0].name, "Alice");
    EXPECT_EQ(students[0].groupName, "Group A");
}

TEST_F(SubmissionRepositoryTest, UpdateStudent) {
    Student student = createTestStudent(100, "Alice", "Group A");
    m_repo->addStudent(student, nullptr);
    
    Student updated;
    updated.id = 100;
    updated.name = "Alice Updated";
    updated.groupName = "Group B";
    
    bool result = m_repo->updateStudent(updated, nullptr);
    ASSERT_TRUE(result);
    
    auto students = m_repo->fetchStudents();
    ASSERT_EQ(students.size(), 1);
    EXPECT_EQ(students[0].name, "Alice Updated");
    EXPECT_EQ(students[0].groupName, "Group B");
}

TEST_F(SubmissionRepositoryTest, DeactivateStudent) {
    Student student = createTestStudent(100, "Alice", "Group A");
    m_repo->addStudent(student, nullptr);
    
    bool result = m_repo->deactivateStudent(100, nullptr);
    ASSERT_TRUE(result);
    
    auto students = m_repo->fetchStudents();
    EXPECT_TRUE(students.empty());
}

TEST_F(SubmissionRepositoryTest, AddAndFetchLabWork) {
    LabWork lab = createTestLabWork(100, "Test Lab");
    
    bool result = m_repo->addLabWork(lab, nullptr);
    ASSERT_TRUE(result);
    
    auto labs = m_repo->fetchLabWorks();
    ASSERT_EQ(labs.size(), 1);
    EXPECT_EQ(labs[0].id, 100);
    EXPECT_EQ(labs[0].title, "Test Lab");
}

TEST_F(SubmissionRepositoryTest, UpdateLabWork) {
    LabWork lab = createTestLabWork(100, "Test Lab");
    m_repo->addLabWork(lab, nullptr);
    
    LabWork updated;
    updated.id = 100;
    updated.title = "Updated Lab";
    updated.language = "cpp";
    
    bool result = m_repo->updateLabWork(updated, nullptr);
    ASSERT_TRUE(result);
    
    auto labs = m_repo->fetchLabWorks();
    ASSERT_EQ(labs.size(), 1);
    EXPECT_EQ(labs[0].title, "Updated Lab");
}

TEST_F(SubmissionRepositoryTest, DeactivateLabWork) {
    LabWork lab = createTestLabWork(100, "Test Lab");
    m_repo->addLabWork(lab, nullptr);
    
    bool result = m_repo->deactivateLabWork(100, nullptr);
    ASSERT_TRUE(result);
    
    auto labs = m_repo->fetchLabWorks();
    EXPECT_TRUE(labs.empty());
}

TEST_F(SubmissionRepositoryTest, AddAndFetchSubmission) {
    // Сначала создаем студента и лабораторную
    Student student = createTestStudent(100, "Alice", "Group A");
    m_repo->addStudent(student, nullptr);
    
    LabWork lab = createTestLabWork(100, "Test Lab");
    m_repo->addLabWork(lab, nullptr);
    
    Submission sub = createTestSubmission(100, 100, 100);
    
    m_repo->add(sub);
    
    auto submissions = m_repo->fetchAll();
    ASSERT_EQ(submissions.size(), 1);
    EXPECT_EQ(submissions[0].id, 100);
    EXPECT_EQ(submissions[0].student.name, "Alice");
    EXPECT_EQ(submissions[0].labWork.title, "Test Lab");
}

TEST_F(SubmissionRepositoryTest, FetchSubmissionsByStudentId) {
    Student student = createTestStudent(100, "Alice", "Group A");
    m_repo->addStudent(student, nullptr);
    
    LabWork lab = createTestLabWork(100, "Test Lab");
    m_repo->addLabWork(lab, nullptr);
    
    for (int i = 1; i <= 3; ++i) {
        Submission sub = createTestSubmission(i, 100, 100);
        m_repo->add(sub);
    }
    
    auto submissions = m_repo->fetchByStudentId(100);
    ASSERT_EQ(submissions.size(), 3);
}

TEST_F(SubmissionRepositoryTest, SoftDeleteSubmission) {
    Student student = createTestStudent(100, "Alice", "Group A");
    m_repo->addStudent(student, nullptr);
    
    LabWork lab = createTestLabWork(100, "Test Lab");
    m_repo->addLabWork(lab, nullptr);
    
    Submission sub = createTestSubmission(100, 100, 100);
    m_repo->add(sub);
    
    bool result = m_repo->softDelete(100, nullptr);
    ASSERT_TRUE(result);
    
    auto submissions = m_repo->fetchAll();
    EXPECT_TRUE(submissions.empty());
}

TEST_F(SubmissionRepositoryTest, SoftDeleteAllSubmissions) {
    Student student = createTestStudent(100, "Alice", "Group A");
    m_repo->addStudent(student, nullptr);
    
    LabWork lab = createTestLabWork(100, "Test Lab");
    m_repo->addLabWork(lab, nullptr);
    
    for (int i = 1; i <= 3; ++i) {
        Submission sub = createTestSubmission(i, 100, 100);
        m_repo->add(sub);
    }
    
    bool result = m_repo->softDeleteAll(nullptr);
    ASSERT_TRUE(result);
    
    auto submissions = m_repo->fetchAll();
    EXPECT_TRUE(submissions.empty());
}

TEST_F(SubmissionRepositoryTest, UpdateSubmissionStatus) {
    Student student = createTestStudent(100, "Alice", "Group A");
    m_repo->addStudent(student, nullptr);
    
    LabWork lab = createTestLabWork(100, "Test Lab");
    m_repo->addLabWork(lab, nullptr);
    
    Submission sub = createTestSubmission(100, 100, 100);
    m_repo->add(sub);
    
    m_repo->updateStatus(100, ExecutionStatus::Passed);
    
    auto submissions = m_repo->fetchAll();
    ASSERT_EQ(submissions.size(), 1);
    EXPECT_EQ(submissions[0].status, ExecutionStatus::Passed);
}

TEST_F(SubmissionRepositoryTest, FindSubmissionById) {
    Student student = createTestStudent(100, "Alice", "Group A");
    m_repo->addStudent(student, nullptr);
    
    LabWork lab = createTestLabWork(100, "Test Lab");
    m_repo->addLabWork(lab, nullptr);
    
    Submission sub = createTestSubmission(100, 100, 100);
    m_repo->add(sub);
    
    auto found = m_repo->findById(100);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->id, 100);
}

TEST_F(SubmissionRepositoryTest, NextIds) {
    EXPECT_EQ(m_repo->nextStudentId(), 1);
    EXPECT_EQ(m_repo->nextLabWorkId(), 1);
    EXPECT_EQ(m_repo->nextSubmissionId(), 1);
    
    // Добавляем что-то и проверяем, что ID увеличиваются
    Student student = createTestStudent(100, "Alice", "Group A");
    m_repo->addStudent(student, nullptr);
    
    EXPECT_EQ(m_repo->nextStudentId(), 101);
}