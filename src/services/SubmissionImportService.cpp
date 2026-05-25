#include "services/SubmissionImportService.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>

#include "services/SubmissionService.h"

namespace {

bool isCppFile(const QFileInfo &fileInfo)
{
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return false;
    }

    const QString suffix = fileInfo.suffix().toLower();
    return suffix == QStringLiteral("cpp")
        || suffix == QStringLiteral("cc")
        || suffix == QStringLiteral("cxx");
}

QStringList expandImportPaths(const QStringList &paths, QStringList *warnings)
{
    QStringList expanded;
    QSet<QString> unique;

    for (const QString &rawPath : paths) {
        const QString trimmedPath = rawPath.trimmed();
        if (trimmedPath.isEmpty()) {
            continue;
        }

        QFileInfo inputInfo(trimmedPath);
        if (!inputInfo.exists()) {
            if (warnings) {
                warnings->push_back(QStringLiteral("Путь не найден: %1").arg(trimmedPath));
            }
            continue;
        }

        if (inputInfo.isFile()) {
            if (!isCppFile(inputInfo)) {
                if (warnings) {
                    warnings->push_back(
                        QStringLiteral("Файл пропущен (не C++): %1").arg(inputInfo.fileName())
                    );
                }
                continue;
            }

            const QString absolutePath = QDir::toNativeSeparators(inputInfo.absoluteFilePath());
            if (!unique.contains(absolutePath)) {
                unique.insert(absolutePath);
                expanded.push_back(absolutePath);
            }
            continue;
        }

        if (!inputInfo.isDir()) {
            continue;
        }

        bool foundCpp = false;
        QDirIterator iterator(
            inputInfo.absoluteFilePath(),
            QDir::Files | QDir::NoSymLinks,
            QDirIterator::Subdirectories
        );

        while (iterator.hasNext()) {
            iterator.next();
            const QFileInfo fileInfo = iterator.fileInfo();
            if (!isCppFile(fileInfo)) {
                continue;
            }

            foundCpp = true;
            const QString absolutePath = QDir::toNativeSeparators(fileInfo.absoluteFilePath());
            if (!unique.contains(absolutePath)) {
                unique.insert(absolutePath);
                expanded.push_back(absolutePath);
            }
        }

        if (!foundCpp && warnings) {
            warnings->push_back(
                QStringLiteral("В папке нет C++ файлов: %1").arg(inputInfo.absoluteFilePath())
            );
        }
    }

    return expanded;
}

} // namespace

namespace labtester::services {

bool SubmissionImportSummary::hasFailures() const
{
    return skipped > 0 || !errors.isEmpty();
}

QString SubmissionImportSummary::toUserMessage() const
{
    if (total <= 0) {
        if (!errors.isEmpty()) {
            return errors.constFirst();
        }
        return QStringLiteral("Не выбраны файлы для импорта.");
    }

    QString message = QStringLiteral("Импортировано: %1 из %2").arg(imported).arg(total);
    if (skipped > 0) {
        message += QStringLiteral(", пропущено: %1").arg(skipped);
    }
    if (!errors.isEmpty()) {
        message += QStringLiteral(". %1").arg(errors.constFirst());
    }
    return message;
}

SubmissionImportService::SubmissionImportService(SubmissionService &submissionService)
    : m_submissionService(submissionService)
{
}

QStringList SubmissionImportService::resolveImportSources(
    const QStringList &sourcePaths,
    QStringList *warnings
) const
{
    return expandImportPaths(sourcePaths, warnings);
}

SubmissionImportSummary SubmissionImportService::importSingle(
    const QString &sourcePath,
    int studentId,
    int labWorkId
)
{
    qInfo() << "[ImportService] importSingle path=" << sourcePath
            << "studentId=" << studentId
            << "labId=" << labWorkId;

    const SubmissionImportItem item {
        sourcePath,
        studentId,
        labWorkId
    };
    return importInternal({item}, 0, 0);
}

SubmissionImportSummary SubmissionImportService::importDroppedFiles(
    const QStringList &filePaths,
    int studentId,
    int labWorkId
)
{
    qInfo() << "[ImportService] importDroppedFiles rawCount=" << filePaths.size()
            << "studentId=" << studentId
            << "labId=" << labWorkId;

    QStringList warnings;
    const QStringList expandedPaths = expandImportPaths(filePaths, &warnings);

    qInfo() << "[ImportService] expanded paths count=" << expandedPaths.size();
    for (const QString &path : expandedPaths) {
        qInfo() << "[ImportService] expanded path:" << path;
    }

    std::vector<SubmissionImportItem> items;
    items.reserve(static_cast<size_t>(expandedPaths.size()));
    for (const QString &path : expandedPaths) {
        items.push_back({
            path,
            studentId,
            labWorkId
        });
    }

    SubmissionImportSummary summary = importInternal(items, studentId, labWorkId);
    for (const QString &warning : warnings) {
        summary.errors.push_back(warning);
        qWarning() << "[ImportService] warning:" << warning;
    }
    if (summary.total == 0 && !warnings.isEmpty()) {
        summary.total = static_cast<int>(filePaths.size());
        summary.skipped = summary.total;
    }

    qInfo() << "[ImportService] result total=" << summary.total
            << "imported=" << summary.imported
            << "skipped=" << summary.skipped
            << "errorsCount=" << summary.errors.size();
    return summary;
}

SubmissionImportSummary SubmissionImportService::importBatch(
    const std::vector<SubmissionImportItem> &items,
    int fallbackStudentId,
    int fallbackLabWorkId
)
{
    qInfo() << "[ImportService] importBatch items=" << items.size()
            << "fallbackStudentId=" << fallbackStudentId
            << "fallbackLabId=" << fallbackLabWorkId;

    std::vector<SubmissionImportItem> expandedItems;
    expandedItems.reserve(items.size());

    SubmissionImportSummary summary;
    summary.total = static_cast<int>(items.size());

    for (const SubmissionImportItem &item : items) {
        QStringList warnings;
        const QStringList expandedPaths = expandImportPaths({item.sourcePath}, &warnings);

        for (const QString &expandedPath : expandedPaths) {
            SubmissionImportItem expandedItem = item;
            expandedItem.sourcePath = expandedPath;
            expandedItems.push_back(expandedItem);
        }

        for (const QString &warning : warnings) {
            summary.errors.push_back(warning);
            qWarning() << "[ImportService] batch warning:" << warning;
        }
    }

    const SubmissionImportSummary importedSummary = importInternal(
        expandedItems,
        fallbackStudentId,
        fallbackLabWorkId
    );

    summary.total = importedSummary.total;
    summary.imported = importedSummary.imported;
    summary.skipped = importedSummary.skipped;
    for (const QString &error : importedSummary.errors) {
        summary.errors.push_back(error);
    }

    qInfo() << "[ImportService] batch result total=" << summary.total
            << "imported=" << summary.imported
            << "skipped=" << summary.skipped
            << "errorsCount=" << summary.errors.size();
    return summary;
}

SubmissionImportSummary SubmissionImportService::importInternal(
    const std::vector<SubmissionImportItem> &items,
    int fallbackStudentId,
    int fallbackLabWorkId
)
{
    SubmissionImportSummary summary;
    summary.total = static_cast<int>(items.size());

    QSet<QString> uniqueKeys;
    for (int index = 0; index < summary.total; ++index) {
        const SubmissionImportItem &item = items[static_cast<size_t>(index)];
        const QString normalizedPath = item.sourcePath.trimmed();
        const int resolvedStudentId = item.studentId > 0 ? item.studentId : fallbackStudentId;
        const int resolvedLabWorkId = item.labWorkId > 0 ? item.labWorkId : fallbackLabWorkId;

        if (normalizedPath.isEmpty()) {
            ++summary.skipped;
            summary.errors.push_back(
                QStringLiteral("Строка %1: путь к файлу пустой.").arg(index + 1)
            );
            continue;
        }

        const QString canonicalPath = QDir::toNativeSeparators(QFileInfo(normalizedPath).absoluteFilePath());
        const QString dedupKey = QStringLiteral("%1|%2|%3")
            .arg(canonicalPath)
            .arg(resolvedStudentId)
            .arg(resolvedLabWorkId);

        if (uniqueKeys.contains(dedupKey)) {
            ++summary.skipped;
            summary.errors.push_back(
                QStringLiteral("Строка %1: дубликат файла в одном пакете импорта.").arg(index + 1)
            );
            continue;
        }
        uniqueKeys.insert(dedupKey);

        QString errorMessage;
        if (!m_submissionService.addSubmission(
                resolvedStudentId,
                resolvedLabWorkId,
                canonicalPath,
                &errorMessage)) {
            ++summary.skipped;
            const QString fileName = QFileInfo(canonicalPath).fileName();
            summary.errors.push_back(
                QStringLiteral("%1: %2")
                    .arg(fileName.isEmpty() ? canonicalPath : fileName)
                    .arg(errorMessage.isEmpty()
                        ? QStringLiteral("не удалось добавить работу.")
                        : errorMessage)
            );
            qWarning() << "[ImportService] addSubmission failed path=" << canonicalPath
                       << "studentId=" << resolvedStudentId
                       << "labId=" << resolvedLabWorkId
                       << "error=" << errorMessage;
            continue;
        }

        ++summary.imported;
        qInfo() << "[ImportService] imported path=" << canonicalPath
                << "studentId=" << resolvedStudentId
                << "labId=" << resolvedLabWorkId;
    }

    return summary;
}

} // namespace labtester::services
