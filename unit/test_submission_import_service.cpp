#include "test_common.h"
#include "services/SubmissionImportService.h"
#include "services/SubmissionService.h"
#include "database/SubmissionRepository.h"

using namespace labtester::services;
using namespace labtester::database;
using namespace labtester::domain;
using namespace labtester::test;

class SubmissionImportServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_testDb = std::make_unique<TestDatabase>();
        m_repo = std::make_unique<SubmissionRepository>(*m_testDb);
        m_subService = std::make_unique<SubmissionService>(*m_repo);
        m_importService = std::make_unique<SubmissionImportService>(*m_subService);
        
        // Создаем студента и лабораторную
        m_subService->addStudent("Alice", "Group A", &m_studentId, nullptr);
        
        QTemporaryDir tempDir;
        QFile templateFile(tempDir.path() + "/template.cpp");
        templateFile.open(QIODevice::WriteOnly);
        templateFile.write("int main() {}");
        templateFile.close();
        
        QFile testFile(tempDir.path() + "/test.cpp");
        testFile.open(QIODevice::WriteOnly);
        testFile.write("#include <gtest/gtest.h>");
        testFile.close();
        
        m_subService->createLabWork("Lab 1", "Desc", templateFile.fileName(), testFile.fileName(), "cpp", &m_labId, nullptr);
        m_tempDir = std::move(tempDir);
    }
    
    std::unique_ptr<TestDatabase> m_testDb;
    std::unique_ptr<SubmissionRepository> m_repo;
    std::unique_ptr<SubmissionService> m_subService;
    std::unique_ptr<SubmissionImportService> m_importService;
    QTemporaryDir m_tempDir;
    int m_studentId = 0;
    int m_labId = 0;
};

TEST_F(SubmissionImportServiceTest, ImportSingleFile) {
    QFile sourceFile(m_tempDir.path() + "/student.cpp");
    sourceFile.open(QIODevice::WriteOnly);
    sourceFile.write("int main() { return 0; }");
    sourceFile.close();
    
    auto summary = m_importService->importSingle(sourceFile.fileName(), m_studentId, m_labId);
    
    EXPECT_EQ(summary.total, 1);
    EXPECT_EQ(summary.imported, 1);
    EXPECT_EQ(summary.skipped, 0);
    EXPECT_TRUE(summary.errors.isEmpty());
    
    auto submissions = m_subService->loadSubmissions();
    EXPECT_EQ(submissions.size(), 1);
}

TEST_F(SubmissionImportServiceTest, ImportNonExistentFile) {
    auto summary = m_importService->importSingle("/nonexistent/file.cpp", m_studentId, m_labId);
    
    EXPECT_EQ(summary.imported, 0);
    EXPECT_EQ(summary.skipped, 1);
    EXPECT_FALSE(summary.errors.isEmpty());
}

TEST_F(SubmissionImportServiceTest, ImportDroppedFiles) {
    QStringList files;
    for (int i = 1; i <= 3; ++i) {
        QFile sourceFile(m_tempDir.path() + QString("/student_%1.cpp").arg(i));
        sourceFile.open(QIODevice::WriteOnly);
        sourceFile.write("int main() { return 0; }");
        sourceFile.close();
        files << sourceFile.fileName();
    }
    
    auto summary = m_importService->importDroppedFiles(files, m_studentId, m_labId);
    
    EXPECT_EQ(summary.total, 3);
    EXPECT_EQ(summary.imported, 3);
    EXPECT_EQ(summary.skipped, 0);
    
    auto submissions = m_subService->loadSubmissions();
    EXPECT_EQ(submissions.size(), 3);
}

TEST_F(SubmissionImportServiceTest, ImportDroppedFilesWithNonCppFiles) {
    QFile nonCppFile(m_tempDir.path() + "/student.txt");
    nonCppFile.open(QIODevice::WriteOnly);
    nonCppFile.write("Hello World");
    nonCppFile.close();
    
    QStringList files;
    files << nonCppFile.fileName();
    
    auto summary = m_importService->importDroppedFiles(files, m_studentId, m_labId);
    
    EXPECT_EQ(summary.imported, 0);
    EXPECT_GT(summary.skipped, 0);
}

TEST_F(SubmissionImportServiceTest, ImportBatch) {
    std::vector<SubmissionImportItem> items;
    for (int i = 1; i <= 3; ++i) {
        QFile sourceFile(m_tempDir.path() + QString("/batch_%1.cpp").arg(i));
        sourceFile.open(QIODevice::WriteOnly);
        sourceFile.write("int main() { return 0; }");
        sourceFile.close();
        
        SubmissionImportItem item;
        item.sourcePath = sourceFile.fileName();
        item.studentId = m_studentId;
        item.labWorkId = m_labId;
        items.push_back(item);
    }
    
    auto summary = m_importService->importBatch(items, 0, 0);
    
    EXPECT_EQ(summary.imported, 3);
    
    auto submissions = m_subService->loadSubmissions();
    EXPECT_EQ(submissions.size(), 3);
}

TEST_F(SubmissionImportServiceTest, ImportBatchWithFallbackIds) {
    QFile sourceFile(m_tempDir.path() + "/fallback.cpp");
    sourceFile.open(QIODevice::WriteOnly);
    sourceFile.write("int main() { return 0; }");
    sourceFile.close();
    
    std::vector<SubmissionImportItem> items;
    SubmissionImportItem item;
    item.sourcePath = sourceFile.fileName();
    item.studentId = 0; // Будет использован fallback
    item.labWorkId = 0; // Будет использован fallback
    items.push_back(item);
    
    auto summary = m_importService->importBatch(items, m_studentId, m_labId);
    
    EXPECT_EQ(summary.imported, 1);
}

TEST_F(SubmissionImportServiceTest, ResolveImportSources) {
    // Создаем файлы в папке
    QDir dir(m_tempDir.path());
    QStringList files;
    for (int i = 1; i <= 3; ++i) {
        QFile sourceFile(dir.filePath(QString("resolve_%1.cpp").arg(i)));
        sourceFile.open(QIODevice::WriteOnly);
        sourceFile.write("int main() { return 0; }");
        sourceFile.close();
        files << sourceFile.fileName();
    }
    
    QStringList warnings;
    auto resolved = m_importService->resolveImportSources(files, &warnings);
    
    EXPECT_EQ(resolved.size(), 3);
    EXPECT_TRUE(warnings.isEmpty());
}

TEST_F(SubmissionImportServiceTest, ResolveImportSourcesWithDirectory) {
    // Создаем папку с файлами
    QDir subDir(m_tempDir.path() + "/subdir");
    subDir.mkpath(".");
    
    for (int i = 1; i <= 2; ++i) {
        QFile sourceFile(subDir.filePath(QString("dir_%1.cpp").arg(i)));
        sourceFile.open(QIODevice::WriteOnly);
        sourceFile.write("int main() { return 0; }");
        sourceFile.close();
    }
    
    QStringList warnings;
    auto resolved = m_importService->resolveImportSources({subDir.absolutePath()}, &warnings);
    
    EXPECT_EQ(resolved.size(), 2);
}

TEST_F(SubmissionImportServiceTest, ImportDroppedFilesWithDirectory) {
    QDir subDir(m_tempDir.path() + "/subdir");
    subDir.mkpath(".");
    
    for (int i = 1; i <= 2; ++i) {
        QFile sourceFile(subDir.filePath(QString("dir_%1.cpp").arg(i)));
        sourceFile.open(QIODevice::WriteOnly);
        sourceFile.write("int main() { return 0; }");
        sourceFile.close();
    }
    
    QStringList files;
    files << subDir.absolutePath();
    
    auto summary = m_importService->importDroppedFiles(files, m_studentId, m_labId);
    
    EXPECT_EQ(summary.imported, 2);
}

TEST_F(SubmissionImportServiceTest, ImportSummaryToUserMessage) {
    SubmissionImportSummary summary;
    summary.total = 5;
    summary.imported = 3;
    summary.skipped = 2;
    summary.errors << "Error 1";
    
    QString message = summary.toUserMessage();
    EXPECT_TRUE(message.contains("Импортировано: 3 из 5"));
    EXPECT_TRUE(message.contains("пропущено: 2"));
    EXPECT_TRUE(message.contains("Error 1"));
}

TEST_F(SubmissionImportServiceTest, ImportSummaryHasFailures) {
    SubmissionImportSummary summary;
    summary.total = 3;
    summary.imported = 3;
    EXPECT_FALSE(summary.hasFailures());
    
    summary.skipped = 1;
    EXPECT_TRUE(summary.hasFailures());
    
    summary.errors << "Error";
    EXPECT_TRUE(summary.hasFailures());
}