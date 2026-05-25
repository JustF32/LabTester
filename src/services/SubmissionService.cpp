#include "services/SubmissionService.h"

#include <algorithm>

#include <QDebug>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringList>

#include "database/SubmissionRepository.h"

namespace {

QString readTextWithFallbackEncoding(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    const QByteArray raw = file.readAll();
    QString text = QString::fromUtf8(raw);
    if (text.contains(QChar::ReplacementCharacter)) {
        text = QString::fromLocal8Bit(raw);
    }
    return text;
}

} // namespace

namespace labtester::services {

SubmissionService::SubmissionService(database::SubmissionRepository &submissionRepository)
    : m_submissionRepository(submissionRepository)
{
}

const std::vector<domain::Student> &SubmissionService::loadStudents()
{
    ensureCatalogLoaded();
    return m_students;
}

const std::vector<domain::LabWork> &SubmissionService::loadLabWorks()
{
    ensureCatalogLoaded();
    return m_labWorks;
}

std::vector<domain::Submission> SubmissionService::loadSubmissions() const
{
    return m_submissionRepository.fetchAll();
}

std::vector<domain::Submission> SubmissionService::loadSubmissionsForStudent(int studentId) const
{
    if (studentId <= 0) {
        return {};
    }
    return m_submissionRepository.fetchByStudentId(studentId);
}

std::vector<domain::Submission> SubmissionService::loadOrCreateMockSubmissions()
{
    ensureCatalogLoaded();
    return m_submissionRepository.fetchAll();
}

bool SubmissionService::addSubmission(
    int studentId,
    int labWorkId,
    const QString &sourcePath,
    QString *errorMessage
)
{
    ensureCatalogLoaded();

    if (sourcePath.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Выберите файл работы.");
        }
        return false;
    }

    QFileInfo fileInfo(sourcePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Файл не найден.");
        }
        return false;
    }

    auto student = findStudentById(studentId);
    if (!student.has_value()) {
        student = ensureUnassignedStudent(errorMessage);
        if (!student.has_value()) {
            return false;
        }
    }

    const auto labWork = findLabWorkById(labWorkId);
    if (!labWork.has_value()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не выбрана лабораторная работа.");
        }
        return false;
    }

    domain::Submission submission;
    submission.id = m_submissionRepository.nextSubmissionId();
    submission.student = *student;
    submission.labWork = *labWork;
    submission.sourcePath = QDir::toNativeSeparators(fileInfo.absoluteFilePath());
    submission.status = domain::ExecutionStatus::Pending;
    submission.createdAt = QDateTime::currentDateTime();

    m_submissionRepository.add(submission);
    return true;
}

std::optional<domain::Student> SubmissionService::ensureUnassignedStudent(QString *errorMessage)
{
    static const QString kUnassignedName = QStringLiteral("Студент не выбран");
    static const QString kUnassignedGroup = QStringLiteral("Без группы");

    const auto existing = std::find_if(
        m_students.cbegin(),
        m_students.cend(),
        [](const domain::Student &student) {
            return student.name == kUnassignedName && student.groupName == kUnassignedGroup;
        }
    );
    if (existing != m_students.cend()) {
        return *existing;
    }

    domain::Student unassignedStudent;
    unassignedStudent.id = m_submissionRepository.nextStudentId();
    unassignedStudent.name = kUnassignedName;
    unassignedStudent.groupName = kUnassignedGroup;

    QString repositoryError;
    if (!m_submissionRepository.addStudent(unassignedStudent, &repositoryError)) {
        if (errorMessage) {
            *errorMessage = repositoryError.isEmpty()
                ? QStringLiteral("Не удалось создать студента по умолчанию.")
                : repositoryError;
        }
        return std::nullopt;
    }

    m_students = m_submissionRepository.fetchStudents();
    return findStudentById(unassignedStudent.id);
}

bool SubmissionService::addStudent(
    const QString &name,
    const QString &groupName,
    int *createdStudentId,
    QString *errorMessage
)
{
    ensureCatalogLoaded();

    const QString normalizedName = name.trimmed();
    const QString normalizedGroup = groupName.trimmed();

    if (normalizedName.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Введите имя студента.");
        }
        return false;
    }

    if (normalizedGroup.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Введите группу студента.");
        }
        return false;
    }

    const auto duplicate = std::find_if(
        m_students.cbegin(),
        m_students.cend(),
        [&normalizedName, &normalizedGroup](const domain::Student &student) {
            return student.name.compare(normalizedName, Qt::CaseInsensitive) == 0
                && student.groupName.compare(normalizedGroup, Qt::CaseInsensitive) == 0;
        }
    );
    if (duplicate != m_students.cend()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Такой студент уже есть в списке.");
        }
        if (createdStudentId) {
            *createdStudentId = duplicate->id;
        }
        return false;
    }

    domain::Student student;
    student.id = m_submissionRepository.nextStudentId();
    student.name = normalizedName;
    student.groupName = normalizedGroup;

    QString repositoryError;
    if (!m_submissionRepository.addStudent(student, &repositoryError)) {
        if (errorMessage) {
            *errorMessage = repositoryError.isEmpty()
                ? QStringLiteral("Не удалось сохранить студента в базе данных.")
                : repositoryError;
        }
        return false;
    }

    m_students = m_submissionRepository.fetchStudents();

    if (createdStudentId) {
        *createdStudentId = student.id;
    }

    return true;
}

bool SubmissionService::updateStudent(
    int studentId,
    const QString &name,
    const QString &groupName,
    QString *errorMessage
)
{
    ensureCatalogLoaded();

    if (studentId <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Выберите студента.");
        }
        return false;
    }

    const QString normalizedName = name.trimmed();
    const QString normalizedGroup = groupName.trimmed();
    if (normalizedName.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Введите имя студента.");
        }
        return false;
    }
    if (normalizedGroup.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Введите группу студента.");
        }
        return false;
    }

    const auto duplicate = std::find_if(
        m_students.cbegin(),
        m_students.cend(),
        [studentId, &normalizedName, &normalizedGroup](const domain::Student &student) {
            return student.id != studentId
                && student.name.compare(normalizedName, Qt::CaseInsensitive) == 0
                && student.groupName.compare(normalizedGroup, Qt::CaseInsensitive) == 0;
        }
    );
    if (duplicate != m_students.cend()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Такой студент уже есть в списке.");
        }
        return false;
    }

    domain::Student student;
    student.id = studentId;
    student.name = normalizedName;
    student.groupName = normalizedGroup;

    QString repositoryError;
    if (!m_submissionRepository.updateStudent(student, &repositoryError)) {
        if (errorMessage) {
            *errorMessage = repositoryError.isEmpty()
                ? QStringLiteral("Не удалось обновить данные студента.")
                : repositoryError;
        }
        return false;
    }

    m_students = m_submissionRepository.fetchStudents();
    return true;
}

bool SubmissionService::removeStudent(int studentId, QString *errorMessage)
{
    ensureCatalogLoaded();

    if (studentId <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Выберите студента.");
        }
        return false;
    }

    auto targetIt = std::find_if(
        m_students.cbegin(),
        m_students.cend(),
        [studentId](const domain::Student &student) {
            return student.id == studentId;
        }
    );
    if (targetIt == m_students.cend()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Студент не найден.");
        }
        return false;
    }

    if (targetIt->name.compare(QStringLiteral("Студент не выбран"), Qt::CaseInsensitive) == 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Служебного студента удалить нельзя.");
        }
        return false;
    }

    QString repositoryError;
    if (!m_submissionRepository.softDeleteByStudentId(studentId, &repositoryError)) {
        if (errorMessage) {
            *errorMessage = repositoryError.isEmpty()
                ? QStringLiteral("Не удалось удалить работы студента.")
                : repositoryError;
        }
        return false;
    }

    if (!m_submissionRepository.deactivateStudent(studentId, &repositoryError)) {
        if (errorMessage) {
            *errorMessage = repositoryError.isEmpty()
                ? QStringLiteral("Не удалось удалить студента.")
                : repositoryError;
        }
        return false;
    }

    m_students = m_submissionRepository.fetchStudents();
    return true;
}

bool SubmissionService::removeSubmission(int submissionId, QString *errorMessage)
{
    if (submissionId <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Некорректная работа.");
        }
        return false;
    }

    return m_submissionRepository.softDelete(submissionId, errorMessage);
}

bool SubmissionService::createLabWork(
    const QString &title,
    const QString &description,
    const QString &templateSourcePath,
    const QString &testSourcePath,
    const QString &language,
    int *createdLabWorkId,
    QString *errorMessage
)
{
    ensureCatalogLoaded();

    if (!validateLabWorkFields(title, templateSourcePath, testSourcePath, true, errorMessage)) {
        return false;
    }

    const QString normalizedTitle = title.trimmed();
    const auto duplicate = std::find_if(
        m_labWorks.cbegin(),
        m_labWorks.cend(),
        [&normalizedTitle](const domain::LabWork &labWork) {
            return labWork.title.compare(normalizedTitle, Qt::CaseInsensitive) == 0;
        }
    );
    if (duplicate != m_labWorks.cend()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("A lab with this title already exists.");
        }
        return false;
    }

    domain::LabWork labWork;
    labWork.id = m_submissionRepository.nextLabWorkId();
    labWork.title = normalizedTitle;
    labWork.description = description.trimmed();
    labWork.language = language.trimmed().isEmpty() ? QStringLiteral("C++") : language.trimmed();

    if (!copyLabAsset(labWork.id, templateSourcePath, QStringLiteral("starter"), &labWork.templateFile, errorMessage)) {
        return false;
    }
    if (!copyLabAsset(labWork.id, testSourcePath, QStringLiteral("tests"), &labWork.testSuitePath, errorMessage)) {
        return false;
    }

    labWork.referenceHeaderPath = inferHeaderPathFromTestPath(labWork.id, testSourcePath);
    labWork.manifestPath = QDir(managedLabDirectory(labWork.id)).filePath(QStringLiteral("manifest.json"));
    if (!writeLabManifest(labWork, errorMessage)) {
        return false;
    }

    QString repositoryError;
    if (!m_submissionRepository.addLabWork(labWork, &repositoryError)) {
        if (errorMessage) {
            *errorMessage = repositoryError.isEmpty()
                ? QStringLiteral("Failed to save lab work.")
                : repositoryError;
        }
        return false;
    }

    m_labWorks = m_submissionRepository.fetchLabWorks();
    if (createdLabWorkId) {
        *createdLabWorkId = labWork.id;
    }
    return true;
}

bool SubmissionService::updateLabWork(
    int labWorkId,
    const QString &title,
    const QString &description,
    const QString &templateSourcePath,
    const QString &testSourcePath,
    const QString &language,
    QString *errorMessage
)
{
    ensureCatalogLoaded();

    if (labWorkId <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Select a lab work first.");
        }
        return false;
    }

    if (!validateLabWorkFields(title, templateSourcePath, testSourcePath, false, errorMessage)) {
        return false;
    }

    auto existingLab = findLabWorkById(labWorkId);
    if (!existingLab.has_value()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Lab work not found.");
        }
        return false;
    }

    const QString normalizedTitle = title.trimmed();
    const auto duplicate = std::find_if(
        m_labWorks.cbegin(),
        m_labWorks.cend(),
        [labWorkId, &normalizedTitle](const domain::LabWork &labWork) {
            return labWork.id != labWorkId
                && labWork.title.compare(normalizedTitle, Qt::CaseInsensitive) == 0;
        }
    );
    if (duplicate != m_labWorks.cend()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("A lab with this title already exists.");
        }
        return false;
    }

    domain::LabWork updated = *existingLab;
    updated.title = normalizedTitle;
    updated.description = description.trimmed();
    updated.language = language.trimmed().isEmpty() ? QStringLiteral("C++") : language.trimmed();

    const QString normalizedTemplatePath = templateSourcePath.trimmed();
    if (!normalizedTemplatePath.isEmpty()) {
        if (!copyLabAsset(updated.id, normalizedTemplatePath, QStringLiteral("starter"), &updated.templateFile, errorMessage)) {
            return false;
        }
    }

    const QString normalizedTestPath = testSourcePath.trimmed();
    if (!normalizedTestPath.isEmpty()) {
        if (!copyLabAsset(updated.id, normalizedTestPath, QStringLiteral("tests"), &updated.testSuitePath, errorMessage)) {
            return false;
        }

        const QString inferredHeaderPath = inferHeaderPathFromTestPath(updated.id, normalizedTestPath);
        if (!inferredHeaderPath.isEmpty()) {
            updated.referenceHeaderPath = inferredHeaderPath;
        }
    }

    if (updated.manifestPath.trimmed().isEmpty()) {
        updated.manifestPath = QDir(managedLabDirectory(updated.id)).filePath(QStringLiteral("manifest.json"));
    }

    if (!writeLabManifest(updated, errorMessage)) {
        return false;
    }

    QString repositoryError;
    if (!m_submissionRepository.updateLabWork(updated, &repositoryError)) {
        if (errorMessage) {
            *errorMessage = repositoryError.isEmpty()
                ? QStringLiteral("Failed to update lab work.")
                : repositoryError;
        }
        return false;
    }

    m_labWorks = m_submissionRepository.fetchLabWorks();
    return true;
}

bool SubmissionService::removeLabWork(int labWorkId, QString *errorMessage)
{
    ensureCatalogLoaded();

    if (labWorkId <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Select a lab work first.");
        }
        return false;
    }

    QString repositoryError;
    if (!m_submissionRepository.softDeleteByLabWorkId(labWorkId, &repositoryError)) {
        if (errorMessage) {
            *errorMessage = repositoryError.isEmpty()
                ? QStringLiteral("Failed to remove submissions for lab work.")
                : repositoryError;
        }
        return false;
    }

    if (!m_submissionRepository.deactivateLabWork(labWorkId, &repositoryError)) {
        if (errorMessage) {
            *errorMessage = repositoryError.isEmpty()
                ? QStringLiteral("Failed to delete lab work.")
                : repositoryError;
        }
        return false;
    }

    m_labWorks = m_submissionRepository.fetchLabWorks();
    return true;
}

void SubmissionService::clearSubmissions()
{
    QString errorMessage;
    if (!m_submissionRepository.softDeleteAll(&errorMessage)) {
        qWarning() << "Failed to clear submissions:" << errorMessage;
    }
}

void SubmissionService::resetMockSubmissions()
{
    ensureCatalogLoaded();
    m_submissionRepository.replaceAll(createMockSubmissions());
}

void SubmissionService::invalidateCatalogCache()
{
    m_catalogLoaded = false;
    m_students.clear();
    m_labWorks.clear();
}

void SubmissionService::ensureCatalogLoaded()
{
    if (m_catalogLoaded) {
        return;
    }

    m_students = m_submissionRepository.fetchStudents();
    const bool hasStudentRecords = m_submissionRepository.nextStudentId() > 1;
    if (m_students.empty() && !hasStudentRecords) {
        m_students = createMockStudents();
        m_submissionRepository.replaceStudents(m_students);
        m_students = m_submissionRepository.fetchStudents();
    }

    m_labWorks = m_submissionRepository.fetchLabWorks();
    const bool hasLabRecords = m_submissionRepository.nextLabWorkId() > 1;
    if (m_labWorks.empty() && !hasLabRecords) {
        m_labWorks = createMockLabWorks();
        m_submissionRepository.replaceLabWorks(m_labWorks);
        m_labWorks = m_submissionRepository.fetchLabWorks();
    }

    bool labsStorageUpdated = false;
    for (const auto &lab : m_labWorks) {
        if (lab.id <= 0) {
            continue;
        }

        const QString desiredLabDir = QDir::cleanPath(managedLabDirectory(lab.id));
        auto isAlreadyInExternalLabs = [&desiredLabDir](const QString &path) {
            if (path.trimmed().isEmpty()) {
                return true;
            }
            return QDir::cleanPath(path).startsWith(desiredLabDir, Qt::CaseInsensitive);
        };

        if (isAlreadyInExternalLabs(lab.templateFile)
            && isAlreadyInExternalLabs(lab.testSuitePath)
            && isAlreadyInExternalLabs(lab.referenceHeaderPath)
            && isAlreadyInExternalLabs(lab.manifestPath)) {
            continue;
        }

        domain::LabWork updatedLab = lab;
        QString errorMessage;

        if (!lab.templateFile.trimmed().isEmpty()) {
            if (!copyLabAsset(lab.id, lab.templateFile, QStringLiteral("starter"), &updatedLab.templateFile, &errorMessage)) {
                qWarning() << "Failed to migrate lab template to external labs folder:" << errorMessage;
                continue;
            }
        }

        if (!lab.testSuitePath.trimmed().isEmpty()) {
            if (!copyLabAsset(lab.id, lab.testSuitePath, QStringLiteral("tests"), &updatedLab.testSuitePath, &errorMessage)) {
                qWarning() << "Failed to migrate lab tests to external labs folder:" << errorMessage;
                continue;
            }
        }

        if (!lab.referenceHeaderPath.trimmed().isEmpty()) {
            if (!copyLabAsset(lab.id, lab.referenceHeaderPath, QStringLiteral("tests"), &updatedLab.referenceHeaderPath, &errorMessage)) {
                qWarning() << "Failed to migrate lab header to external labs folder:" << errorMessage;
                continue;
            }
        }

        updatedLab.manifestPath = QDir(desiredLabDir).filePath(QStringLiteral("manifest.json"));
        if (!writeLabManifest(updatedLab, &errorMessage)) {
            qWarning() << "Failed to write migrated lab manifest:" << errorMessage;
            continue;
        }

        if (!m_submissionRepository.updateLabWork(updatedLab, &errorMessage)) {
            qWarning() << "Failed to persist migrated lab paths:" << errorMessage;
            continue;
        }

        labsStorageUpdated = true;
    }

    if (labsStorageUpdated) {
        m_labWorks = m_submissionRepository.fetchLabWorks();
    }

    m_catalogLoaded = true;
}

bool SubmissionService::validateLabWorkFields(
    const QString &title,
    const QString &templateSourcePath,
    const QString &testSourcePath,
    bool requireFiles,
    QString *errorMessage
) const
{
    if (title.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Enter lab title.");
        }
        return false;
    }

    auto validateExistingFile = [errorMessage](const QString &path, const QString &label) -> bool {
        if (path.trimmed().isEmpty()) {
            return true;
        }

        const QFileInfo info(path);
        if (!info.exists() || !info.isFile()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("%1 file not found.").arg(label);
            }
            return false;
        }
        return true;
    };

    if (requireFiles && templateSourcePath.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Select starter template file.");
        }
        return false;
    }
    if (requireFiles && testSourcePath.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Select tests file.");
        }
        return false;
    }

    if (!validateExistingFile(templateSourcePath, QStringLiteral("Starter template"))) {
        return false;
    }
    if (!validateExistingFile(testSourcePath, QStringLiteral("Tests"))) {
        return false;
    }
    return true;
}

bool SubmissionService::copyLabAsset(
    int labWorkId,
    const QString &sourcePath,
    const QString &subDir,
    QString *targetPath,
    QString *errorMessage
) const
{
    const QString normalizedSource = sourcePath.trimmed();
    if (normalizedSource.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Asset path is empty.");
        }
        return false;
    }

    const QFileInfo sourceInfo(normalizedSource);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Selected file does not exist.");
        }
        return false;
    }

    QDir labDir(managedLabDirectory(labWorkId));
    if (!labDir.mkpath(subDir)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to prepare lab storage directory.");
        }
        return false;
    }

    const QString destinationPath = QDir(labDir.filePath(subDir)).filePath(sourceInfo.fileName());
    const QString sourceAbsolute = QDir::cleanPath(sourceInfo.absoluteFilePath());
    const QString destinationAbsolute = QDir::cleanPath(QFileInfo(destinationPath).absoluteFilePath());
    if (sourceAbsolute.compare(destinationAbsolute, Qt::CaseInsensitive) == 0) {
        if (targetPath) {
            *targetPath = QDir::toNativeSeparators(destinationAbsolute);
        }
        return true;
    }

    QFile::remove(destinationPath);
    if (!QFile::copy(sourceAbsolute, destinationPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to copy selected file to lab storage.");
        }
        return false;
    }

    if (targetPath) {
        *targetPath = QDir::toNativeSeparators(QFileInfo(destinationPath).absoluteFilePath());
    }
    return true;
}

QString SubmissionService::inferHeaderPathFromTestPath(int labWorkId, const QString &testFilePath) const
{
    const QFileInfo testInfo(testFilePath);
    if (!testInfo.exists() || !testInfo.isFile()) {
        return QString();
    }

    const QDir sourceDir = testInfo.absoluteDir();
    QString selectedHeaderPath;

    const QString testSourceText = readTextWithFallbackEncoding(testInfo.absoluteFilePath());
    if (!testSourceText.isEmpty()) {
        const QRegularExpression includeRegex(
            QStringLiteral("^\\s*#\\s*include\\s*\"([^\"]+\\.(h|hpp|hh))\""),
            QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption
        );
        QRegularExpressionMatchIterator includeMatches = includeRegex.globalMatch(testSourceText);
        while (includeMatches.hasNext()) {
            const QRegularExpressionMatch match = includeMatches.next();
            const QString includePath = match.captured(1).trimmed();
            if (includePath.isEmpty()) {
                continue;
            }

            const QString normalizedIncludePath = QDir::fromNativeSeparators(includePath);
            QFileInfo includedHeaderInfo(sourceDir.filePath(normalizedIncludePath));
            if (!includedHeaderInfo.exists() || !includedHeaderInfo.isFile()) {
                includedHeaderInfo = QFileInfo(
                    sourceDir.filePath(QFileInfo(normalizedIncludePath).fileName())
                );
            }

            if (includedHeaderInfo.exists() && includedHeaderInfo.isFile()) {
                selectedHeaderPath = includedHeaderInfo.absoluteFilePath();
                break;
            }
        }
    }

    if (!selectedHeaderPath.isEmpty()) {
        QString copiedHeaderPath;
        QString ignoredError;
        if (copyLabAsset(
                labWorkId,
                selectedHeaderPath,
                QStringLiteral("tests"),
                &copiedHeaderPath,
                &ignoredError)) {
            return copiedHeaderPath;
        }
    }

    const QStringList headerCandidates = sourceDir.entryList(
        {QStringLiteral("*.h"), QStringLiteral("*.hpp")},
        QDir::Files,
        QDir::Name
    );
    if (headerCandidates.isEmpty()) {
        return QString();
    }

    QString selectedHeaderName;
    const auto ccHeader = std::find_if(
        headerCandidates.cbegin(),
        headerCandidates.cend(),
        [](const QString &fileName) {
            return fileName.compare(QStringLiteral("cc.h"), Qt::CaseInsensitive) == 0;
        }
    );
    if (ccHeader != headerCandidates.cend()) {
        selectedHeaderName = *ccHeader;
    } else {
        selectedHeaderName = headerCandidates.front();
    }

    const QString sourceHeaderPath = sourceDir.filePath(selectedHeaderName);
    QString copiedHeaderPath;
    QString ignoredError;
    if (!copyLabAsset(
            labWorkId,
            sourceHeaderPath,
            QStringLiteral("tests"),
            &copiedHeaderPath,
            &ignoredError)) {
        return QString();
    }
    return copiedHeaderPath;
}

bool SubmissionService::writeLabManifest(const domain::LabWork &labWork, QString *errorMessage) const
{
    if (labWork.id <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cannot write manifest for invalid lab id.");
        }
        return false;
    }

    const QString manifestPath = labWork.manifestPath.trimmed().isEmpty()
        ? QDir(managedLabDirectory(labWork.id)).filePath(QStringLiteral("manifest.json"))
        : labWork.manifestPath.trimmed();

    QFileInfo manifestInfo(manifestPath);
    QDir manifestDir = manifestInfo.absoluteDir();
    if (!manifestDir.mkpath(QStringLiteral("."))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to prepare manifest directory.");
        }
        return false;
    }

    QJsonObject manifest;
    manifest.insert(QStringLiteral("id"), labWork.id);
    manifest.insert(QStringLiteral("title"), labWork.title);
    manifest.insert(QStringLiteral("description"), labWork.description);
    manifest.insert(QStringLiteral("language"), labWork.language);
    manifest.insert(QStringLiteral("starterFile"), QDir::toNativeSeparators(labWork.templateFile));
    manifest.insert(QStringLiteral("testSuiteFile"), QDir::toNativeSeparators(labWork.testSuitePath));
    manifest.insert(QStringLiteral("headerFile"), QDir::toNativeSeparators(labWork.referenceHeaderPath));
    manifest.insert(
        QStringLiteral("updatedAt"),
        QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
    );

    QSaveFile saveFile(manifestInfo.absoluteFilePath());
    if (!saveFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to open manifest file for writing.");
        }
        return false;
    }

    const QJsonDocument document(manifest);
    saveFile.write(document.toJson(QJsonDocument::Indented));
    if (!saveFile.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to commit manifest file.");
        }
        return false;
    }

    return true;
}

QString SubmissionService::managedLabDirectory(int labWorkId) const
{
    const QDir currentDir(QDir::currentPath());
    const QDir appDir(QCoreApplication::applicationDirPath());
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    const QStringList rootCandidates = {
        currentDir.filePath(QStringLiteral("..")),
        appDir.filePath(QStringLiteral("..")),
        currentDir.absolutePath(),
        appDir.absolutePath()
    };

    QString rootPath;
    for (const QString &candidate : rootCandidates) {
        const QDir candidateDir(QDir::cleanPath(candidate));
        if (candidateDir.exists(QStringLiteral("CMakeLists.txt")) && candidateDir.exists(QStringLiteral("qml"))) {
            rootPath = candidateDir.absolutePath();
            break;
        }
    }

    if (rootPath.isEmpty()) {
        if (!appDataPath.trimmed().isEmpty()) {
            rootPath = QDir::cleanPath(appDataPath);
        } else {
            rootPath = QDir::cleanPath(appDir.filePath(QStringLiteral("..")));
        }
    }

    QDir baseDir(rootPath);
    if (!baseDir.mkpath(QStringLiteral("labs"))) {
        QDir fallbackDir(QDir::tempPath());
        fallbackDir.mkpath(QStringLiteral("LabTester/labs"));
        return fallbackDir.filePath(QStringLiteral("LabTester/labs/lab_%1").arg(labWorkId));
    }
    return baseDir.filePath(QStringLiteral("labs/lab_%1").arg(labWorkId));
}

QString SubmissionService::resolveUTestRootPath() const
{
    const QDir currentDir(QDir::currentPath());
    const QDir appDir(QCoreApplication::applicationDirPath());

    const QStringList candidates = {
        currentDir.filePath(QStringLiteral("UTEST")),
        currentDir.filePath(QStringLiteral("../UTEST")),
        appDir.filePath(QStringLiteral("../../UTEST")),
        appDir.filePath(QStringLiteral("../UTEST"))
    };

    for (const auto &candidate : candidates) {
        const QDir dir(candidate);
        if (dir.exists(QStringLiteral("sort_test.cpp"))
            && dir.exists(QStringLiteral("cc.h"))) {
            return QDir::cleanPath(dir.absolutePath());
        }
    }

    return QString();
}

std::optional<domain::Student> SubmissionService::findStudentById(int id) const
{
    for (const auto &student : m_students) {
        if (student.id == id) {
            return student;
        }
    }
    return std::nullopt;
}

std::optional<domain::LabWork> SubmissionService::findLabWorkById(int id) const
{
    for (const auto &labWork : m_labWorks) {
        if (labWork.id == id) {
            return labWork;
        }
    }
    return std::nullopt;
}

std::vector<domain::Student> SubmissionService::createMockStudents() const
{
    return {
        {1, QStringLiteral("Анисимов Владислав"), QStringLiteral("ИВБ-417")},
        {2, QStringLiteral("Гущин Александр"), QStringLiteral("ИВБ-417")},
        {3, QStringLiteral("Автайкин Дмитрий"), QStringLiteral("ИВБ-417")},
        {4, QStringLiteral("Дурасов Матвей"), QStringLiteral("ИВБ-417")}
    };
}

std::vector<domain::LabWork> SubmissionService::createMockLabWorks() const
{
    const QDir currentDir(QDir::currentPath());
    const QDir appDir(QCoreApplication::applicationDirPath());
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    struct LabAssetPaths {
        QString suitePath;
        QString headerPath;
        QString templatePath;
    };

    auto resolveLabAssets = [&](int labId) -> LabAssetPaths {
        const QString labFolder = QStringLiteral("lab_%1").arg(labId);
        const QStringList candidates = {
            currentDir.filePath(QStringLiteral("labs/%1").arg(labFolder)),
            currentDir.filePath(QStringLiteral("../labs/%1").arg(labFolder)),
            appDir.filePath(QStringLiteral("labs/%1").arg(labFolder)),
            appDir.filePath(QStringLiteral("../labs/%1").arg(labFolder)),
            appDir.filePath(QStringLiteral("../../labs/%1").arg(labFolder)),
            appDataPath.trimmed().isEmpty()
                ? QString()
                : QDir(appDataPath).filePath(QStringLiteral("labs/%1").arg(labFolder))
        };

        for (const QString &candidate : candidates) {
            if (candidate.trimmed().isEmpty()) {
                continue;
            }
            const QFileInfo suiteInfo(QDir(candidate).filePath(QStringLiteral("tests/sort_test.cpp")));
            const QFileInfo headerInfo(QDir(candidate).filePath(QStringLiteral("tests/cc.h")));
            const QFileInfo templateInfo(QDir(candidate).filePath(QStringLiteral("starter/cc_template.cpp")));
            if (!suiteInfo.exists() || !headerInfo.exists() || !templateInfo.exists()) {
                continue;
            }

            return {
                suiteInfo.absoluteFilePath(),
                headerInfo.absoluteFilePath(),
                templateInfo.absoluteFilePath()
            };
        }

        return {};
    };

    LabAssetPaths lab1Assets = resolveLabAssets(1);

    const QString utestRoot = resolveUTestRootPath();
    if (lab1Assets.suitePath.isEmpty()) {
        lab1Assets.suitePath = utestRoot.isEmpty()
            ? QStringLiteral("UTEST/sort_test.cpp")
            : QDir(utestRoot).filePath(QStringLiteral("sort_test.cpp"));
    }
    if (lab1Assets.headerPath.isEmpty()) {
        lab1Assets.headerPath = utestRoot.isEmpty()
            ? QStringLiteral("UTEST/cc.h")
            : QDir(utestRoot).filePath(QStringLiteral("cc.h"));
    }
    if (lab1Assets.templatePath.isEmpty()) {
        lab1Assets.templatePath = utestRoot.isEmpty()
            ? QStringLiteral("UTEST/cc_template.cpp")
            : QDir(utestRoot).filePath(QStringLiteral("cc_template.cpp"));
    }

    const LabAssetPaths lab2Assets = resolveLabAssets(2);
    const LabAssetPaths lab3Assets = resolveLabAssets(3);
    const LabAssetPaths lab4Assets = resolveLabAssets(4);

    return {
        {
            1,
            QStringLiteral("Лабораторная 1: Алгоритмы сортировки"),
            QStringLiteral("C++"),
            QStringLiteral("Пузырьковая, сортировка выбором, вставками, слиянием, пирамидальная, быстрая и лексикографическая сортировка: базовые случаи, краевые ситуации и эффективность на больших данных."),
            QString(),
            QDir::toNativeSeparators(lab1Assets.templatePath),
            QStringLiteral("Массивы int[] и std::vector<std::string>"),
            QStringLiteral("Отсортированные по возрастанию/лексикографически данные"),
            QDir::toNativeSeparators(lab1Assets.suitePath),
            QDir::toNativeSeparators(lab1Assets.headerPath)
        },
        {
            2,
            QStringLiteral("Лабораторная 2: Структуры данных"),
            QStringLiteral("C++"),
            QStringLiteral("Стек, очередь, дек, связный список и бинарное дерево поиска: корректность базовых операций, рост динамических массивов, краевые случаи и эффективность."),
            QString(),
            QDir::toNativeSeparators(lab2Assets.templatePath),
            QStringLiteral("Операции push/pop/front/back/insert/remove/get/contains"),
            QStringLiteral("Корректное состояние структуры и возвращаемые значения"),
            QDir::toNativeSeparators(lab2Assets.suitePath),
            QDir::toNativeSeparators(lab2Assets.headerPath)
        },
        {
            3,
            QStringLiteral("Лабораторная 3: Хеш-функции и хеш-таблицы"),
            QStringLiteral("C++"),
            QStringLiteral("Хеш-функции для строк, анализ распределения и хеш-таблица с добавлением, поиском и удалением."),
            QString(),
            QDir::toNativeSeparators(lab3Assets.templatePath),
            QStringLiteral("Строковые ключи и наборы данных для анализа распределения"),
            QStringLiteral("Корректные хеши, статистика коллизий и операции таблицы"),
            QDir::toNativeSeparators(lab3Assets.suitePath),
            QDir::toNativeSeparators(lab3Assets.headerPath)
        },
        {
            4,
            QStringLiteral("Лабораторная 4: Алгоритмы на строках"),
            QStringLiteral("C++"),
            QStringLiteral("Рабин-Карп, Кнут-Моррис-Пратт, Бойер-Мур и Ахо-Корасик: поиск подстрок и множественных шаблонов."),
            QString(),
            QDir::toNativeSeparators(lab4Assets.templatePath),
            QStringLiteral("Текст, шаблон или набор шаблонов"),
            QStringLiteral("Позиции найденных вхождений"),
            QDir::toNativeSeparators(lab4Assets.suitePath),
            QDir::toNativeSeparators(lab4Assets.headerPath)
        }
    };
}

std::vector<domain::Submission> SubmissionService::createMockSubmissions() const
{
    const QDateTime now = QDateTime::currentDateTime();
    const QDir currentDir(QDir::currentPath());
    const QDir appDir(QCoreApplication::applicationDirPath());
    const QStringList sampleCandidates = {
        currentDir.filePath(QStringLiteral("test_samples/lab1")),
        currentDir.filePath(QStringLiteral("../test_samples/lab1")),
        appDir.filePath(QStringLiteral("../test_samples/lab1")),
        appDir.filePath(QStringLiteral("../../test_samples/lab1"))
    };

    QString samplesRoot;
    for (const auto &candidate : sampleCandidates) {
        const QDir dir(candidate);
        if (dir.exists(QStringLiteral("student_pass.cpp"))) {
            samplesRoot = dir.absolutePath();
            break;
        }
    }
    if (samplesRoot.isEmpty()) {
        samplesRoot = currentDir.filePath(QStringLiteral("test_samples/lab1"));
    }

    const auto student1 = m_students[0];
    const auto student2 = m_students[1];
    const auto student3 = m_students[2];
    const auto lab1 = m_labWorks[0];

    return {
        {
            1,
            student1,
            lab1,
            QDir::toNativeSeparators(QDir(samplesRoot).filePath(QStringLiteral("student_pass.cpp"))),
            domain::ExecutionStatus::Pending,
            now.addSecs(-3600)
        },
        {
            2,
            student2,
            lab1,
            QDir::toNativeSeparators(QDir(samplesRoot).filePath(QStringLiteral("student_fail.cpp"))),
            domain::ExecutionStatus::Pending,
            now.addSecs(-2700)
        },
        {
            3,
            student3,
            lab1,
            QDir::toNativeSeparators(QDir(samplesRoot).filePath(QStringLiteral("student_build_error.cpp"))),
            domain::ExecutionStatus::Pending,
            now.addSecs(-1800)
        }
    };
}

} // namespace labtester::services
