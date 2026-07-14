#include "test_common.h"
#include "services/SubmissionService.h"
#include "database/SubmissionRepository.h"
#include "database/DatabaseManager.h"

using namespace labtester::services;
using namespace labtester::database;
using namespace labtester::domain;
using namespace labtester::test;

class SubmissionServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_testDb = std::make_unique<TestDatabase>();
        m_repo = std::make_unique<SubmissionRepository>(*m_testDb);
        m_service = std::make_unique<SubmissionService>(*m_repo);
    }
    
    std::unique_ptr<TestDatabase> m_testDb;
    std::unique_ptr<SubmissionRepository> m_repo;
    std::unique_ptr<SubmissionService> m_service;
};

TEST_F(SubmissionServiceTest, LoadStudents) {
    // Добавляем студента напрямую в репозиторий
    Student student = createTestStudent(100, "Alice", "Group A");
    m_repo->addStudent(student, nullptr);
    
    // Инвалидируем кэш, чтобы загрузить свежие данные
    m_service->invalidateCatalogCache();
    auto& students = m_service->loadStudents();
    
    ASSERT_EQ(students.size(), 1);
    EXPECT_EQ(students[0].name, "Alice");
}

TEST_F(SubmissionServiceTest, AddStudent) {
    int createdId = 0;
    bool result = m_service->addStudent("Bob", "Group B", &createdId, nullptr);
    
    ASSERT_TRUE(result);
    EXPECT_GT(createdId, 0);
    
    auto& students = m_service->loadStudents();
    bool found = false;
    for (const auto& s : students) {
        if (s.name == "Bob") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(SubmissionServiceTest, AddDuplicateStudent) {
    m_service->addStudent("Alice", "Group A", nullptr, nullptr);
    
    int createdId = 0;
    bool result = m_service->addStudent("Alice", "Group A", &createdId, nullptr);
    
    EXPECT_FALSE(result);
    EXPECT_GT(createdId, 0); // Возвращает ID существующего студента
}

TEST_F(SubmissionServiceTest, UpdateStudent) {
    int createdId = 0;
    m_service->addStudent("Alice", "Group A", &createdId, nullptr);
    
    bool result = m_service->updateStudent(createdId, "Alice Updated", "Group B", nullptr);
    ASSERT_TRUE(result);
    
    auto& students = m_service->loadStudents();
    for (const auto& s : students) {
        if (s.id == createdId) {
            EXPECT_EQ(s.name, "Alice Updated");
            EXPECT_EQ(s.groupName, "Group B");
            return;
        }
    }
    FAIL() << "Student not found";
}

TEST_F(SubmissionServiceTest, RemoveStudent) {
    int createdId = 0;
    m_service->addStudent("Alice", "Group A", &createdId, nullptr);
    
    bool result = m_service->removeStudent(createdId, nullptr);
    ASSERT_TRUE(result);
    
    auto& students = m_service->loadStudents();
    for (const auto& s : students) {
        EXPECT_NE(s.id, createdId);
    }
}

TEST_F(SubmissionServiceTest, CannotRemoveUnassignedStudent) {
    // Загружаем каталог, чтобы появился "Студент не выбран"
    m_service->loadStudents();
    
    // Находим ID служебного студента
    auto& students = m_service->loadStudents();
    int unassignedId = 0;
    for (const auto& s : students) {
        if (s.name == "Студент не выбран") {
            unassignedId = s.id;
            break;
        }
    }
    
    EXPECT_GT(unassignedId, 0);
    bool result = m_service->removeStudent(unassignedId, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(SubmissionServiceTest, LoadLabWorks) {
    // Добавляем лабораторную
    LabWork lab = createTestLabWork(100, "Test Lab");
    m_repo->addLabWork(lab, nullptr);
    
    m_service->invalidateCatalogCache();
    auto& labs = m_service->loadLabWorks();
    
    ASSERT_GE(labs.size(), 1);
}

TEST_F(SubmissionServiceTest, CreateLabWork) {
    // Создаем временные файлы
    QTemporaryDir tempDir;
    QFile templateFile(tempDir.path() + "/template.cpp");
    templateFile.open(QIODevice::WriteOnly);
    templateFile.write("int main() {}");
    templateFile.close();
    
    QFile testFile(tempDir.path() + "/test.cpp");
    testFile.open(QIODevice::WriteOnly);
    testFile.write("#include <gtest/gtest.h>");
    testFile.close();
    
    int createdId = 0;
    bool result = m_service->createLabWork(
        "New Lab",
        "Description",
        templateFile.fileName(),
        testFile.fileName(),
        "cpp",
        &createdId,
        nullptr
    );
    
    ASSERT_TRUE(result);
    EXPECT_GT(createdId, 0);
}

TEST_F(SubmissionServiceTest, UpdateLabWork) {
    // Сначала создаем лабораторную
    QTemporaryDir tempDir;
    QFile templateFile(tempDir.path() + "/template.cpp");
    templateFile.open(QIODevice::WriteOnly);
    templateFile.write("int main() {}");
    templateFile.close();
    
    QFile testFile(tempDir.path() + "/test.cpp");
    testFile.open(QIODevice::WriteOnly);
    testFile.write("#include <gtest/gtest.h>");
    testFile.close();
    
    int createdId = 0;
    m_service->createLabWork("Lab 1", "Desc", templateFile.fileName(), testFile.fileName(), "cpp", &createdId, nullptr);
    
    bool result = m_service->updateLabWork(createdId, "Updated Lab", "New Desc", "", "", "cpp", nullptr);
    ASSERT_TRUE(result);
    
    auto& labs = m_service->loadLabWorks();
    for (const auto& l : labs) {
        if (l.id == createdId) {
            EXPECT_EQ(l.title, "Updated Lab");
            return;
        }
    }
    FAIL() << "Lab not found";
}

TEST_F(SubmissionServiceTest, RemoveLabWork) {
    QTemporaryDir tempDir;
    QFile templateFile(tempDir.path() + "/template.cpp");
    templateFile.open(QIODevice::WriteOnly);
    templateFile.write("int main() {}");
    templateFile.close();
    
    QFile testFile(tempDir.path() + "/test.cpp");
    testFile.open(QIODevice::WriteOnly);
    testFile.write("#include <gtest/gtest.h>");
    testFile.close();
    
    int createdId = 0;
    m_service->createLabWork("Lab 1", "Desc", templateFile.fileName(), testFile.fileName(), "cpp", &createdId, nullptr);
    
    bool result = m_service->removeLabWork(createdId, nullptr);
    ASSERT_TRUE(result);
    
    auto& labs = m_service->loadLabWorks();
    for (const auto& l : labs) {
        EXPECT_NE(l.id, createdId);
    }
}

TEST_F(SubmissionServiceTest, AddSubmission) {
    // Создаем студента и лабораторную
    int studentId = 0;
    m_service->addStudent("Alice", "Group A", &studentId, nullptr);
    
    QTemporaryDir tempDir;
    QFile templateFile(tempDir.path() + "/template.cpp");
    templateFile.open(QIODevice::WriteOnly);
    templateFile.write("int main() {}");
    templateFile.close();
    
    QFile testFile(tempDir.path() + "/test.cpp");
    testFile.open(QIODevice::WriteOnly);
    testFile.write("#include <gtest/gtest.h>");
    testFile.close();
    
    int labId = 0;
    m_service->createLabWork("Lab 1", "Desc", templateFile.fileName(), testFile.fileName(), "cpp", &labId, nullptr);
    
    // Создаем файл работы
    QFile sourceFile(tempDir.path() + "/student.cpp");
    sourceFile.open(QIODevice::WriteOnly);
    sourceFile.write("int main() { return 0; }");
    sourceFile.close();
    
    bool result = m_service->addSubmission(studentId, labId, sourceFile.fileName(), nullptr);
    ASSERT_TRUE(result);
    
    auto submissions = m_service->loadSubmissions();
    ASSERT_GE(submissions.size(), 1);
}

TEST_F(SubmissionServiceTest, AddSubmissionWithNonExistentStudent) {
    QTemporaryDir tempDir;
    QFile sourceFile(tempDir.path() + "/student.cpp");
    sourceFile.open(QIODevice::WriteOnly);
    sourceFile.write("int main() { return 0; }");
    sourceFile.close();
    
    // Создаем лабораторную
    QFile templateFile(tempDir.path() + "/template.cpp");
    templateFile.open(QIODevice::WriteOnly);
    templateFile.write("int main() {}");
    templateFile.close();
    
    QFile testFile(tempDir.path() + "/test.cpp");
    testFile.open(QIODevice::WriteOnly);
    testFile.write("#include <gtest/gtest.h>");
    testFile.close();
    
    int labId = 0;
    m_service->createLabWork("Lab 1", "Desc", templateFile.fileName(), testFile.fileName(), "cpp", &labId, nullptr);
    
    // Студент с несуществующим ID должен создать служебного
    bool result = m_service->addSubmission(999, labId, sourceFile.fileName(), nullptr);
    ASSERT_TRUE(result);
    
    auto submissions = m_service->loadSubmissions();
    ASSERT_GE(submissions.size(), 1);
    EXPECT_EQ(submissions[0].student.name, "Студент не выбран");
}

TEST_F(SubmissionServiceTest, RemoveSubmission) {
    // Создаем все необходимое
    int studentId = 0;
    m_service->addStudent("Alice", "Group A", &studentId, nullptr);
    
    QTemporaryDir tempDir;
    QFile templateFile(tempDir.path() + "/template.cpp");
    templateFile.open(QIODevice::WriteOnly);
    templateFile.write("int main() {}");
    templateFile.close();
    
    QFile testFile(tempDir.path() + "/test.cpp");
    testFile.open(QIODevice::WriteOnly);
    testFile.write("#include <gtest/gtest.h>");
    testFile.close();
    
    int labId = 0;
    m_service->createLabWork("Lab 1", "Desc", templateFile.fileName(), testFile.fileName(), "cpp", &labId, nullptr);
    
    QFile sourceFile(tempDir.path() + "/student.cpp");
    sourceFile.open(QIODevice::WriteOnly);
    sourceFile.write("int main() { return 0; }");
    sourceFile.close();
    
    m_service->addSubmission(studentId, labId, sourceFile.fileName(), nullptr);
    
    auto submissions = m_service->loadSubmissions();
    ASSERT_FALSE(submissions.empty());
    int submissionId = submissions[0].id;
    
    bool result = m_service->removeSubmission(submissionId, nullptr);
    ASSERT_TRUE(result);
    
    submissions = m_service->loadSubmissions();
    EXPECT_TRUE(submissions.empty());
}

TEST_F(SubmissionServiceTest, LoadSubmissionsForStudent) {
    int studentId = 0;
    m_service->addStudent("Alice", "Group A", &studentId, nullptr);
    
    QTemporaryDir tempDir;
    QFile templateFile(tempDir.path() + "/template.cpp");
    templateFile.open(QIODevice::WriteOnly);
    templateFile.write("int main() {}");
    templateFile.close();
    
    QFile testFile(tempDir.path() + "/test.cpp");
    testFile.open(QIODevice::WriteOnly);
    testFile.write("#include <gtest/gtest.h>");
    testFile.close();
    
    int labId = 0;
    m_service->createLabWork("Lab 1", "Desc", templateFile.fileName(), testFile.fileName(), "cpp", &labId, nullptr);
    
    QFile sourceFile(tempDir.path() + "/student.cpp");
    sourceFile.open(QIODevice::WriteOnly);
    sourceFile.write("int main() { return 0; }");
    sourceFile.close();
    
    m_service->addSubmission(studentId, labId, sourceFile.fileName(), nullptr);
    
    auto submissions = m_service->loadSubmissionsForStudent(studentId);
    ASSERT_GE(submissions.size(), 1);
}