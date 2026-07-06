#include "database/SubmissionRepository.h"

#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QVariant>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include "database/DatabaseManager.h"
#include "domain/ExecutionStatus.h"

namespace {

QDateTime parseDateTime(const QVariant &value)
{
    const QString text = value.toString().trimmed();
    if (text.isEmpty()) {
        return QDateTime::currentDateTime();
    }

    QDateTime parsed = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!parsed.isValid()) {
        parsed = QDateTime::fromString(text, Qt::ISODate);
    }
    if (!parsed.isValid()) {
        parsed = QDateTime::fromString(text, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    }
    if (!parsed.isValid()) {
        parsed = QDateTime::currentDateTime();
    }
    return parsed;
}

int countCyrillicLetters(const QString &value)
{
    int total = 0;
    for (const QChar ch : value) {
        const ushort code = ch.unicode();
        if ((code >= 0x0400 && code <= 0x04FF) || (code >= 0x0500 && code <= 0x052F)) {
            ++total;
        }
    }
    return total;
}

bool looksLikeUtf8Mojibake(const QString &value)
{
    if (value.size() < 2) {
        return false;
    }

    int markers = 0;
    for (const QChar ch : value) {
        const ushort code = ch.unicode();
        if (code == 0x00D0 || code == 0x00D1 || code == 0x00D2 || code == 0x00D3) {
            ++markers;
        }
    }
    return markers >= 2 && (markers * 3 >= value.size());
}

QString repairUtf8Mojibake(const QString &value)
{
    if (!looksLikeUtf8Mojibake(value)) {
        return value;
    }

    const QByteArray latin1Bytes = value.toLatin1();
    const QString repaired = QString::fromUtf8(latin1Bytes);
    if (repaired.contains(QChar::ReplacementCharacter)) {
        return value;
    }

    return countCyrillicLetters(repaired) > countCyrillicLetters(value)
        ? repaired
        : value;
}

} // namespace

namespace labtester::database {

SubmissionRepository::SubmissionRepository(DatabaseManager &databaseManager)
    : m_databaseManager(databaseManager)
{
}

std::vector<domain::Student> SubmissionRepository::fetchStudents() const
{
    std::vector<domain::Student> students;
    QSqlQuery query(m_databaseManager.database());
    query.prepare(QStringLiteral(
        "SELECT s.id, s.full_name, COALESCE(g.name, '') "
        "FROM students s "
        "LEFT JOIN student_groups g ON g.id = s.group_id "
        "WHERE s.is_active = 1 "
        "ORDER BY s.id"
    ));

    if (!query.exec()) {
        qWarning() << "Failed to fetch students:" << query.lastError().text();
        return students;
    }

    while (query.next()) {
        domain::Student student;
        student.id = query.value(0).toInt();
        student.name = repairUtf8Mojibake(query.value(1).toString());
        student.groupName = repairUtf8Mojibake(query.value(2).toString());
        students.push_back(student);
    }
    return students;
}

std::vector<domain::LabWork> SubmissionRepository::fetchLabWorks() const
{
    std::vector<domain::LabWork> labWorks;
    QSqlQuery query(m_databaseManager.database());
    query.prepare(QStringLiteral(
        "SELECT id, title, language, COALESCE(description, ''), "
        "COALESCE(manifest_rel_path, ''), "
        "COALESCE(starter_code_rel_path, ''), COALESCE(test_suite_rel_path, ''), "
        "COALESCE(header_rel_path, '') "
        "FROM lab_works "
        "WHERE is_active = 1 "
        "ORDER BY id"
    ));

    if (!query.exec()) {
        qWarning() << "Failed to fetch lab works:" << query.lastError().text();
        return labWorks;
    }

    while (query.next()) {
        domain::LabWork labWork;
        labWork.id = query.value(0).toInt();
        labWork.title = repairUtf8Mojibake(query.value(1).toString());
        labWork.language = query.value(2).toString();
        labWork.description = repairUtf8Mojibake(query.value(3).toString());
        labWork.manifestPath = QDir::toNativeSeparators(query.value(4).toString());
        labWork.templateFile = QDir::toNativeSeparators(query.value(5).toString());
        labWork.expectedInput = QString();
        labWork.expectedOutput = QString();
        labWork.testSuitePath = QDir::toNativeSeparators(query.value(6).toString());
        labWork.referenceHeaderPath = QDir::toNativeSeparators(query.value(7).toString());
        labWorks.push_back(labWork);
    }
    return labWorks;
}

void SubmissionRepository::replaceStudents(const std::vector<domain::Student> &students)
{
    QSqlDatabase db = m_databaseManager.database();
    if (!db.transaction()) {
        qWarning() << "Failed to start transaction for replaceStudents:" << db.lastError().text();
        return;
    }

    for (const auto &student : students) {
        if (student.id <= 0) {
            continue;
        }
        const int groupId = ensureGroup(student.groupName);

        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "INSERT INTO students(id, group_id, full_name, is_active, created_at, updated_at) "
            "VALUES(:id, :group_id, :full_name, 1, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP) "
            "ON CONFLICT(id) DO UPDATE SET "
            "group_id = excluded.group_id, "
            "full_name = excluded.full_name, "
            "is_active = 1, "
            "updated_at = CURRENT_TIMESTAMP"
        ));
        query.bindValue(QStringLiteral(":id"), student.id);
        query.bindValue(QStringLiteral(":group_id"), groupId > 0 ? QVariant(groupId) : QVariant());
        query.bindValue(QStringLiteral(":full_name"), student.name.trimmed());

        if (!query.exec()) {
            qWarning() << "Failed to upsert student:" << student.id << query.lastError().text();
            db.rollback();
            return;
        }
    }

    if (!db.commit()) {
        qWarning() << "Failed to commit replaceStudents:" << db.lastError().text();
    }
}

void SubmissionRepository::replaceLabWorks(const std::vector<domain::LabWork> &labWorks)
{
    QSqlDatabase db = m_databaseManager.database();
    if (!db.transaction()) {
        qWarning() << "Failed to start transaction for replaceLabWorks:" << db.lastError().text();
        return;
    }

    for (const auto &labWork : labWorks) {
        if (!ensureLabWork(labWork)) {
            db.rollback();
            return;
        }
    }

    if (!db.commit()) {
        qWarning() << "Failed to commit replaceLabWorks:" << db.lastError().text();
    }
}

bool SubmissionRepository::addLabWork(const domain::LabWork &labWork, QString *errorMessage)
{
    if (labWork.id <= 0 || labWork.title.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Некорректные данные лабораторной работы.");
        }
        return false;
    }

    if (!ensureLabWork(labWork)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось сохранить лабораторную работу.");
        }
        return false;
    }
    return true;
}

bool SubmissionRepository::updateLabWork(const domain::LabWork &labWork, QString *errorMessage)
{
    if (labWork.id <= 0 || labWork.title.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Некорректные данные лабораторной работы.");
        }
        return false;
    }

    QSqlQuery checkQuery(m_databaseManager.database());
    checkQuery.prepare(QStringLiteral(
        "SELECT COUNT(1) FROM lab_works WHERE id = :id"
    ));
    checkQuery.bindValue(QStringLiteral(":id"), labWork.id);
    if (!checkQuery.exec() || !checkQuery.next() || checkQuery.value(0).toInt() <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Лабораторная работа не найдена.");
        }
        return false;
    }

    if (!ensureLabWork(labWork)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось обновить лабораторную работу.");
        }
        return false;
    }
    return true;
}

bool SubmissionRepository::deactivateLabWork(int labWorkId, QString *errorMessage)
{
    if (labWorkId <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Некорректный идентификатор лабораторной работы.");
        }
        return false;
    }

    QSqlQuery query(m_databaseManager.database());
    query.prepare(QStringLiteral(
        "UPDATE lab_works "
        "SET is_active = 0, updated_at = CURRENT_TIMESTAMP "
        "WHERE id = :id AND is_active = 1"
    ));
    query.bindValue(QStringLiteral(":id"), labWorkId);

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        qWarning() << "Failed to deactivate lab work:" << labWorkId << query.lastError().text();
        return false;
    }

    if (query.numRowsAffected() <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Лабораторная работа не найдена или уже удалена.");
        }
        return false;
    }

    return true;
}

bool SubmissionRepository::addStudent(const domain::Student &student, QString *errorMessage)
{
    if (student.id <= 0 || student.name.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Student data is invalid.");
        }
        return false;
    }

    const int groupId = ensureGroup(student.groupName);

    QSqlQuery query(m_databaseManager.database());
    query.prepare(QStringLiteral(
        "INSERT INTO students(id, group_id, full_name, is_active, created_at, updated_at) "
        "VALUES(:id, :group_id, :full_name, 1, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)"
    ));
    query.bindValue(QStringLiteral(":id"), student.id);
    query.bindValue(QStringLiteral(":group_id"), groupId > 0 ? QVariant(groupId) : QVariant());
    query.bindValue(QStringLiteral(":full_name"), student.name.trimmed());

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        qWarning() << "Failed to add student:" << query.lastError().text();
        return false;
    }
    return true;
}

bool SubmissionRepository::updateStudent(const domain::Student &student, QString *errorMessage)
{
    if (student.id <= 0 || student.name.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Некорректные данные студента.");
        }
        return false;
    }

    const int groupId = ensureGroup(student.groupName);

    QSqlQuery query(m_databaseManager.database());
    query.prepare(QStringLiteral(
        "UPDATE students "
        "SET group_id = :group_id, "
        "    full_name = :full_name, "
        "    updated_at = CURRENT_TIMESTAMP "
        "WHERE id = :id AND is_active = 1"
    ));
    query.bindValue(QStringLiteral(":id"), student.id);
    query.bindValue(QStringLiteral(":group_id"), groupId > 0 ? QVariant(groupId) : QVariant());
    query.bindValue(QStringLiteral(":full_name"), student.name.trimmed());

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        qWarning() << "Failed to update student:" << student.id << query.lastError().text();
        return false;
    }

    if (query.numRowsAffected() <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Студент не найден.");
        }
        return false;
    }

    return true;
}

bool SubmissionRepository::deactivateStudent(int studentId, QString *errorMessage)
{
    if (studentId <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Некорректный идентификатор студента.");
        }
        return false;
    }

    QSqlQuery query(m_databaseManager.database());
    query.prepare(QStringLiteral(
        "UPDATE students "
        "SET is_active = 0, "
        "    updated_at = CURRENT_TIMESTAMP "
        "WHERE id = :id AND is_active = 1"
    ));
    query.bindValue(QStringLiteral(":id"), studentId);

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        qWarning() << "Failed to deactivate student:" << studentId << query.lastError().text();
        return false;
    }

    if (query.numRowsAffected() <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Студент не найден или уже удален.");
        }
        return false;
    }

    return true;
}

bool SubmissionRepository::softDeleteByStudentId(int studentId, QString *errorMessage)
{
    if (studentId <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Некорректный идентификатор студента.");
        }
        return false;
    }

    QSqlQuery query(m_databaseManager.database());
    query.prepare(QStringLiteral(
        "UPDATE submission_entries "
        "SET is_deleted = 1, "
        "    updated_at = CURRENT_TIMESTAMP "
        "WHERE student_id = :student_id AND is_deleted = 0"
    ));
    query.bindValue(QStringLiteral(":student_id"), studentId);

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        qWarning() << "Failed to soft delete submissions by student:" << studentId << query.lastError().text();
        return false;
    }

    return true;
}

bool SubmissionRepository::softDeleteByLabWorkId(int labWorkId, QString *errorMessage)
{
    if (labWorkId <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Некорректный идентификатор лабораторной.");
        }
        return false;
    }

    QSqlQuery query(m_databaseManager.database());
    query.prepare(QStringLiteral(
        "UPDATE submission_entries "
        "SET is_deleted = 1, "
        "    updated_at = CURRENT_TIMESTAMP "
        "WHERE lab_work_id = :lab_work_id AND is_deleted = 0"
    ));
    query.bindValue(QStringLiteral(":lab_work_id"), labWorkId);

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        qWarning() << "Failed to soft delete submissions by lab work:" << labWorkId << query.lastError().text();
        return false;
    }

    return true;
}

std::vector<domain::Submission> SubmissionRepository::fetchAll() const
{
    std::vector<domain::Submission> submissions;
    QSqlQuery query(m_databaseManager.database());
    query.prepare(QStringLiteral(
        "SELECT "
        "se.id, se.source_rel_path, se.status, se.created_at, "
        "s.id, s.full_name, COALESCE(g.name, ''), "
        "l.id, l.title, l.language, COALESCE(l.description, ''), "
        "COALESCE(l.starter_code_rel_path, ''), COALESCE(l.test_suite_rel_path, ''), COALESCE(l.header_rel_path, '') "
        "FROM submission_entries se "
        "JOIN students s ON s.id = se.student_id "
        "LEFT JOIN student_groups g ON g.id = s.group_id "
        "JOIN lab_works l ON l.id = se.lab_work_id "
        "WHERE se.is_deleted = 0 "
        "ORDER BY se.id"
    ));

    if (!query.exec()) {
        qWarning() << "Failed to fetch submissions:" << query.lastError().text();
        return submissions;
    }

    while (query.next()) {
        domain::Submission submission;
        submission.id = query.value(0).toInt();
        submission.sourcePath = QDir::toNativeSeparators(query.value(1).toString());
        submission.status = domain::executionStatusFromString(query.value(2).toString());
        submission.createdAt = parseDateTime(query.value(3));

        submission.student.id = query.value(4).toInt();
        submission.student.name = repairUtf8Mojibake(query.value(5).toString());
        submission.student.groupName = repairUtf8Mojibake(query.value(6).toString());

        submission.labWork.id = query.value(7).toInt();
        submission.labWork.title = repairUtf8Mojibake(query.value(8).toString());
        submission.labWork.language = query.value(9).toString();
        submission.labWork.description = repairUtf8Mojibake(query.value(10).toString());
        submission.labWork.templateFile = QDir::toNativeSeparators(query.value(11).toString());
        submission.labWork.expectedInput = QString();
        submission.labWork.expectedOutput = QString();
        submission.labWork.testSuitePath = QDir::toNativeSeparators(query.value(12).toString());
        submission.labWork.referenceHeaderPath = QDir::toNativeSeparators(query.value(13).toString());

        submissions.push_back(submission);
    }
    return submissions;
}

std::vector<domain::Submission> SubmissionRepository::fetchByStudentId(int studentId) const
{
    std::vector<domain::Submission> submissions;
    QSqlQuery query(m_databaseManager.database());
    query.prepare(QStringLiteral(
        "SELECT "
        "se.id, se.source_rel_path, se.status, se.created_at, "
        "s.id, s.full_name, COALESCE(g.name, ''), "
        "l.id, l.title, l.language, COALESCE(l.description, ''), "
        "COALESCE(l.starter_code_rel_path, ''), COALESCE(l.test_suite_rel_path, ''), COALESCE(l.header_rel_path, '') "
        "FROM submission_entries se "
        "JOIN students s ON s.id = se.student_id "
        "LEFT JOIN student_groups g ON g.id = s.group_id "
        "JOIN lab_works l ON l.id = se.lab_work_id "
        "WHERE se.is_deleted = 0 AND se.student_id = :student_id "
        "ORDER BY se.id"
    ));
    query.bindValue(QStringLiteral(":student_id"), studentId);

    if (!query.exec()) {
        qWarning() << "Failed to fetch submissions by student id:" << query.lastError().text();
        return submissions;
    }

    while (query.next()) {
        domain::Submission submission;
        submission.id = query.value(0).toInt();
        submission.sourcePath = QDir::toNativeSeparators(query.value(1).toString());
        submission.status = domain::executionStatusFromString(query.value(2).toString());
        submission.createdAt = parseDateTime(query.value(3));

        submission.student.id = query.value(4).toInt();
        submission.student.name = repairUtf8Mojibake(query.value(5).toString());
        submission.student.groupName = repairUtf8Mojibake(query.value(6).toString());

        submission.labWork.id = query.value(7).toInt();
        submission.labWork.title = repairUtf8Mojibake(query.value(8).toString());
        submission.labWork.language = query.value(9).toString();
        submission.labWork.description = repairUtf8Mojibake(query.value(10).toString());
        submission.labWork.templateFile = QDir::toNativeSeparators(query.value(11).toString());
        submission.labWork.expectedInput = QString();
        submission.labWork.expectedOutput = QString();
        submission.labWork.testSuitePath = QDir::toNativeSeparators(query.value(12).toString());
        submission.labWork.referenceHeaderPath = QDir::toNativeSeparators(query.value(13).toString());

        submissions.push_back(submission);
    }
    return submissions;
}

void SubmissionRepository::replaceAll(const std::vector<domain::Submission> &submissions)
{
    QSqlDatabase db = m_databaseManager.database();
    if (!db.transaction()) {
        qWarning() << "Failed to start transaction for replaceAll submissions:" << db.lastError().text();
        return;
    }

    QSqlQuery cleanupQuery(db);
    if (!cleanupQuery.exec(QStringLiteral("DELETE FROM submission_entries"))) {
        qWarning() << "Failed to cleanup submission_entries:" << cleanupQuery.lastError().text();
        db.rollback();
        return;
    }

    for (const auto &submission : submissions) {
        if (!upsertSubmission(submission)) {
            db.rollback();
            return;
        }
    }

    if (!db.commit()) {
        qWarning() << "Failed to commit replaceAll submissions:" << db.lastError().text();
    }
}

void SubmissionRepository::add(const domain::Submission &submission)
{
    if (!upsertSubmission(submission)) {
        qWarning() << "Failed to add submission id" << submission.id;
    }
}

bool SubmissionRepository::softDelete(int submissionId, QString *errorMessage)
{
    if (submissionId <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Некорректный идентификатор работы.");
        }
        return false;
    }

    QSqlQuery query(m_databaseManager.database());
    query.prepare(QStringLiteral(
        "UPDATE submission_entries "
        "SET is_deleted = 1, updated_at = CURRENT_TIMESTAMP "
        "WHERE id = :id AND is_deleted = 0"
    ));
    query.bindValue(QStringLiteral(":id"), submissionId);

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        qWarning() << "Failed to soft delete submission:" << submissionId << query.lastError().text();
        return false;
    }

    if (query.numRowsAffected() <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Работа не найдена или уже удалена.");
        }
        return false;
    }

    return true;
}

bool SubmissionRepository::softDeleteAll(QString *errorMessage)
{
    QSqlQuery query(m_databaseManager.database());
    query.prepare(QStringLiteral(
        "UPDATE submission_entries "
        "SET is_deleted = 1, updated_at = CURRENT_TIMESTAMP "
        "WHERE is_deleted = 0"
    ));

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        qWarning() << "Failed to soft delete all submissions:" << query.lastError().text();
        return false;
    }

    return true;
}

void SubmissionRepository::updateStatus(int submissionId, domain::ExecutionStatus status)
{
    QSqlQuery query(m_databaseManager.database());
    query.prepare(QStringLiteral(
        "UPDATE submission_entries "
        "SET status = :status, updated_at = CURRENT_TIMESTAMP "
        "WHERE id = :id"
    ));
    query.bindValue(QStringLiteral(":status"), domain::toStorageString(status));
    query.bindValue(QStringLiteral(":id"), submissionId);

    if (!query.exec()) {
        qWarning() << "Failed to update submission status:" << query.lastError().text();
    }
}

std::optional<domain::Submission> SubmissionRepository::findById(int submissionId) const
{
    QSqlQuery query(m_databaseManager.database());
    query.prepare(QStringLiteral(
        "SELECT "
        "se.id, se.source_rel_path, se.status, se.created_at, "
        "s.id, s.full_name, COALESCE(g.name, ''), "
        "l.id, l.title, l.language, COALESCE(l.description, ''), "
        "COALESCE(l.starter_code_rel_path, ''), COALESCE(l.test_suite_rel_path, ''), COALESCE(l.header_rel_path, '') "
        "FROM submission_entries se "
        "JOIN students s ON s.id = se.student_id "
        "LEFT JOIN student_groups g ON g.id = s.group_id "
        "JOIN lab_works l ON l.id = se.lab_work_id "
        "WHERE se.id = :id AND se.is_deleted = 0"
    ));
    query.bindValue(QStringLiteral(":id"), submissionId);

    if (!query.exec()) {
        qWarning() << "Failed to find submission by id:" << query.lastError().text();
        return std::nullopt;
    }
    if (!query.next()) {
        return std::nullopt;
    }

    domain::Submission submission;
    submission.id = query.value(0).toInt();
    submission.sourcePath = QDir::toNativeSeparators(query.value(1).toString());
    submission.status = domain::executionStatusFromString(query.value(2).toString());
    submission.createdAt = parseDateTime(query.value(3));

    submission.student.id = query.value(4).toInt();
    submission.student.name = repairUtf8Mojibake(query.value(5).toString());
    submission.student.groupName = repairUtf8Mojibake(query.value(6).toString());

    submission.labWork.id = query.value(7).toInt();
    submission.labWork.title = repairUtf8Mojibake(query.value(8).toString());
    submission.labWork.language = query.value(9).toString();
    submission.labWork.description = repairUtf8Mojibake(query.value(10).toString());
    submission.labWork.templateFile = QDir::toNativeSeparators(query.value(11).toString());
    submission.labWork.expectedInput = QString();
    submission.labWork.expectedOutput = QString();
    submission.labWork.testSuitePath = QDir::toNativeSeparators(query.value(12).toString());
    submission.labWork.referenceHeaderPath = QDir::toNativeSeparators(query.value(13).toString());

    return submission;
}

int SubmissionRepository::nextSubmissionId() const
{
    QSqlQuery query(m_databaseManager.database());
    if (!query.exec(QStringLiteral("SELECT COALESCE(MAX(id), 0) + 1 FROM submission_entries"))) {
        qWarning() << "Failed to compute next submission id:" << query.lastError().text();
        return 1;
    }
    if (!query.next()) {
        return 1;
    }
    return query.value(0).toInt();
}

int SubmissionRepository::nextStudentId() const
{
    QSqlQuery query(m_databaseManager.database());
    if (!query.exec(QStringLiteral("SELECT COALESCE(MAX(id), 0) + 1 FROM students"))) {
        qWarning() << "Failed to compute next student id:" << query.lastError().text();
        return 1;
    }
    if (!query.next()) {
        return 1;
    }
    return query.value(0).toInt();
}

int SubmissionRepository::nextLabWorkId() const
{
    QSqlQuery query(m_databaseManager.database());
    if (!query.exec(QStringLiteral("SELECT COALESCE(MAX(id), 0) + 1 FROM lab_works"))) {
        qWarning() << "Failed to compute next lab work id:" << query.lastError().text();
        return 1;
    }
    if (!query.next()) {
        return 1;
    }
    return query.value(0).toInt();
}

bool SubmissionRepository::upsertSubmission(const domain::Submission &submission)
{
    if (submission.student.id <= 0 || submission.labWork.id <= 0) {
        qWarning() << "Submission references invalid student/lab ids.";
        return false;
    }

    if (!ensureStudent(submission.student) || !ensureLabWork(submission.labWork)) {
        return false;
    }

    int submissionId = submission.id;
    if (submissionId <= 0) {
        submissionId = nextSubmissionId();
    }

    const QString sourcePath = QDir::toNativeSeparators(submission.sourcePath);
    const QString sourceName = QFileInfo(sourcePath).fileName();
    const QDateTime createdAt = submission.createdAt.isValid()
        ? submission.createdAt
        : QDateTime::currentDateTime();

    QSqlQuery query(m_databaseManager.database());
    query.prepare(QStringLiteral(
        "INSERT INTO submission_entries("
        "id, student_id, lab_work_id, source_rel_path, source_original_name, status, is_deleted, created_at, updated_at"
        ") VALUES("
        ":id, :student_id, :lab_work_id, :source_rel_path, :source_original_name, :status, 0, :created_at, CURRENT_TIMESTAMP"
        ") "
        "ON CONFLICT(id) DO UPDATE SET "
        "student_id = excluded.student_id, "
        "lab_work_id = excluded.lab_work_id, "
        "source_rel_path = excluded.source_rel_path, "
        "source_original_name = excluded.source_original_name, "
        "status = excluded.status, "
        "is_deleted = 0, "
        "updated_at = CURRENT_TIMESTAMP"
    ));
    query.bindValue(QStringLiteral(":id"), submissionId);
    query.bindValue(QStringLiteral(":student_id"), submission.student.id);
    query.bindValue(QStringLiteral(":lab_work_id"), submission.labWork.id);
    query.bindValue(QStringLiteral(":source_rel_path"), sourcePath);
    query.bindValue(QStringLiteral(":source_original_name"), sourceName);
    query.bindValue(QStringLiteral(":status"), domain::toStorageString(submission.status));
    query.bindValue(QStringLiteral(":created_at"), createdAt.toString(Qt::ISODateWithMs));

    if (!query.exec()) {
        qWarning() << "Failed to upsert submission:" << submissionId << query.lastError().text();
        return false;
    }
    return true;
}

int SubmissionRepository::ensureGroup(const QString &groupName) const
{
    const QString normalizedGroup = groupName.trimmed();
    if (normalizedGroup.isEmpty()) {
        return 0;
    }

    QSqlQuery selectQuery(m_databaseManager.database());
    selectQuery.prepare(QStringLiteral(
        "SELECT id FROM student_groups "
        "WHERE lower(name) = lower(:name) "
        "LIMIT 1"
    ));
    selectQuery.bindValue(QStringLiteral(":name"), normalizedGroup);
    if (selectQuery.exec() && selectQuery.next()) {
        return selectQuery.value(0).toInt();
    }

    QSqlQuery insertQuery(m_databaseManager.database());
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO student_groups(name, created_at, updated_at) "
        "VALUES(:name, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)"
    ));
    insertQuery.bindValue(QStringLiteral(":name"), normalizedGroup);
    if (!insertQuery.exec()) {
        qWarning() << "Failed to ensure student group:" << insertQuery.lastError().text();
        return 0;
    }
    return insertQuery.lastInsertId().toInt();
}

bool SubmissionRepository::ensureStudent(const domain::Student &student) const
{
    if (student.id <= 0 || student.name.trimmed().isEmpty()) {
        return false;
    }

    const int groupId = ensureGroup(student.groupName);
    QSqlQuery query(m_databaseManager.database());
    query.prepare(QStringLiteral(
        "INSERT INTO students(id, group_id, full_name, is_active, created_at, updated_at) "
        "VALUES(:id, :group_id, :full_name, 1, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP) "
        "ON CONFLICT(id) DO UPDATE SET "
        "group_id = excluded.group_id, "
        "full_name = excluded.full_name, "
        "is_active = 1, "
        "updated_at = CURRENT_TIMESTAMP"
    ));
    query.bindValue(QStringLiteral(":id"), student.id);
    query.bindValue(QStringLiteral(":group_id"), groupId > 0 ? QVariant(groupId) : QVariant());
    query.bindValue(QStringLiteral(":full_name"), student.name.trimmed());

    if (!query.exec()) {
        qWarning() << "Failed to ensure student:" << student.id << query.lastError().text();
        return false;
    }
    return true;
}

bool SubmissionRepository::ensureLabWork(const domain::LabWork &labWork) const
{
    if (labWork.id <= 0 || labWork.title.trimmed().isEmpty()) {
        return false;
    }

    QSqlQuery query(m_databaseManager.database());
    query.prepare(QStringLiteral(
        "INSERT INTO lab_works("
        "id, title, description, language, manifest_rel_path, starter_code_rel_path, test_suite_rel_path, header_rel_path, is_active, created_at, updated_at"
        ") VALUES("
        ":id, :title, :description, :language, :manifest_rel_path, :starter_code_rel_path, :test_suite_rel_path, :header_rel_path, 1, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP"
        ") "
        "ON CONFLICT(id) DO UPDATE SET "
        "title = excluded.title, "
        "description = excluded.description, "
        "language = excluded.language, "
        "manifest_rel_path = excluded.manifest_rel_path, "
        "starter_code_rel_path = excluded.starter_code_rel_path, "
        "test_suite_rel_path = excluded.test_suite_rel_path, "
        "header_rel_path = excluded.header_rel_path, "
        "is_active = 1, "
        "updated_at = CURRENT_TIMESTAMP"
    ));
    query.bindValue(QStringLiteral(":id"), labWork.id);
    query.bindValue(QStringLiteral(":title"), labWork.title.trimmed());
    query.bindValue(QStringLiteral(":description"), labWork.description.trimmed());
    query.bindValue(QStringLiteral(":language"), labWork.language.trimmed().isEmpty() ? QStringLiteral("C++") : labWork.language.trimmed());
    query.bindValue(QStringLiteral(":manifest_rel_path"), QDir::toNativeSeparators(labWork.manifestPath));
    query.bindValue(QStringLiteral(":starter_code_rel_path"), QDir::toNativeSeparators(labWork.templateFile));
    query.bindValue(QStringLiteral(":test_suite_rel_path"), QDir::toNativeSeparators(labWork.testSuitePath));
    query.bindValue(QStringLiteral(":header_rel_path"), QDir::toNativeSeparators(labWork.referenceHeaderPath));

    if (!query.exec()) {
        qWarning() << "Failed to ensure lab work:" << labWork.id << query.lastError().text();
        return false;
    }
    return true;
}

} // namespace labtester::database
