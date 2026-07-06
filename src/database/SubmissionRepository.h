#pragma once

#include <optional>
#include <vector>

#include <QString>

#include "domain/LabWork.h"
#include "domain/Submission.h"
#include "domain/Student.h"

namespace labtester::database {

class DatabaseManager;

class SubmissionRepository {
public:
    explicit SubmissionRepository(DatabaseManager &databaseManager);

    std::vector<domain::Student> fetchStudents() const;
    std::vector<domain::LabWork> fetchLabWorks() const;
    void replaceStudents(const std::vector<domain::Student> &students);
    void replaceLabWorks(const std::vector<domain::LabWork> &labWorks);
    bool addLabWork(const domain::LabWork &labWork, QString *errorMessage = nullptr);
    bool updateLabWork(const domain::LabWork &labWork, QString *errorMessage = nullptr);
    bool deactivateLabWork(int labWorkId, QString *errorMessage = nullptr);
    bool addStudent(const domain::Student &student, QString *errorMessage = nullptr);
    bool updateStudent(const domain::Student &student, QString *errorMessage = nullptr);
    bool deactivateStudent(int studentId, QString *errorMessage = nullptr);
    bool softDeleteByStudentId(int studentId, QString *errorMessage = nullptr);
    bool softDeleteByLabWorkId(int labWorkId, QString *errorMessage = nullptr);

    std::vector<domain::Submission> fetchAll() const;
    std::vector<domain::Submission> fetchByStudentId(int studentId) const;
    void replaceAll(const std::vector<domain::Submission> &submissions);
    void add(const domain::Submission &submission);
    bool softDelete(int submissionId, QString *errorMessage = nullptr);
    bool softDeleteAll(QString *errorMessage = nullptr);
    void updateStatus(int submissionId, domain::ExecutionStatus status);
    std::optional<domain::Submission> findById(int submissionId) const;
    int nextSubmissionId() const;
    int nextStudentId() const;
    int nextLabWorkId() const;

private:
    bool upsertSubmission(const domain::Submission &submission);
    int ensureGroup(const QString &groupName) const;
    bool ensureStudent(const domain::Student &student) const;
    bool ensureLabWork(const domain::LabWork &labWork) const;

    DatabaseManager &m_databaseManager;
};

} // namespace labtester::database
