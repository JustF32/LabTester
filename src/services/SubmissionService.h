#pragma once

#include <optional>
#include <vector>

#include <QString>

#include "domain/LabWork.h"
#include "domain/Submission.h"
#include "domain/Student.h"

namespace labtester::database {
class SubmissionRepository;
}

namespace labtester::services {

class SubmissionService {
public:
    explicit SubmissionService(database::SubmissionRepository &submissionRepository);

    const std::vector<domain::Student> &loadStudents();
    const std::vector<domain::LabWork> &loadLabWorks();

    std::vector<domain::Submission> loadSubmissions() const;
    std::vector<domain::Submission> loadSubmissionsForStudent(int studentId) const;
    std::vector<domain::Submission> loadOrCreateMockSubmissions();
    bool addSubmission(
        int studentId,
        int labWorkId,
        const QString &sourcePath,
        QString *errorMessage = nullptr
    );
    bool addStudent(
        const QString &name,
        const QString &groupName,
        int *createdStudentId = nullptr,
        QString *errorMessage = nullptr
    );
    bool updateStudent(
        int studentId,
        const QString &name,
        const QString &groupName,
        QString *errorMessage = nullptr
    );
    bool removeStudent(int studentId, QString *errorMessage = nullptr);
    bool removeSubmission(int submissionId, QString *errorMessage = nullptr);
    bool createLabWork(
        const QString &title,
        const QString &description,
        const QString &templateSourcePath,
        const QString &testSourcePath,
        const QString &language,
        int *createdLabWorkId = nullptr,
        QString *errorMessage = nullptr
    );
    bool updateLabWork(
        int labWorkId,
        const QString &title,
        const QString &description,
        const QString &templateSourcePath,
        const QString &testSourcePath,
        const QString &language,
        QString *errorMessage = nullptr
    );
    bool removeLabWork(int labWorkId, QString *errorMessage = nullptr);
    void clearSubmissions();
    void resetMockSubmissions();
    void invalidateCatalogCache();

private:
    void ensureCatalogLoaded();
    QString resolveUTestRootPath() const;
    std::optional<domain::Student> ensureUnassignedStudent(QString *errorMessage);
    bool validateLabWorkFields(
        const QString &title,
        const QString &templateSourcePath,
        const QString &testSourcePath,
        bool requireFiles,
        QString *errorMessage
    ) const;
    bool copyLabAsset(
        int labWorkId,
        const QString &sourcePath,
        const QString &subDir,
        QString *targetPath,
        QString *errorMessage
    ) const;
    QString inferHeaderPathFromTestPath(int labWorkId, const QString &testFilePath) const;
    bool writeLabManifest(const domain::LabWork &labWork, QString *errorMessage) const;
    QString managedLabDirectory(int labWorkId) const;

    std::optional<domain::Student> findStudentById(int id) const;
    std::optional<domain::LabWork> findLabWorkById(int id) const;

    std::vector<domain::Student> createMockStudents() const;
    std::vector<domain::LabWork> createMockLabWorks() const;
    std::vector<domain::Submission> createMockSubmissions() const;

    database::SubmissionRepository &m_submissionRepository;
    std::vector<domain::Student> m_students;
    std::vector<domain::LabWork> m_labWorks;
    bool m_catalogLoaded {false};
};

} // namespace labtester::services
