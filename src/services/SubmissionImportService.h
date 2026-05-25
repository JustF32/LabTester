#pragma once

#include <vector>

#include <QString>
#include <QStringList>

namespace labtester::services {

class SubmissionService;

struct SubmissionImportItem {
    QString sourcePath;
    int studentId {0};
    int labWorkId {0};
};

struct SubmissionImportSummary {
    int total {0};
    int imported {0};
    int skipped {0};
    QStringList errors;

    [[nodiscard]] bool hasFailures() const;
    [[nodiscard]] QString toUserMessage() const;
};

class SubmissionImportService {
public:
    explicit SubmissionImportService(SubmissionService &submissionService);

    QStringList resolveImportSources(
        const QStringList &sourcePaths,
        QStringList *warnings = nullptr
    ) const;

    SubmissionImportSummary importSingle(
        const QString &sourcePath,
        int studentId,
        int labWorkId
    );

    SubmissionImportSummary importDroppedFiles(
        const QStringList &filePaths,
        int studentId,
        int labWorkId
    );

    SubmissionImportSummary importBatch(
        const std::vector<SubmissionImportItem> &items,
        int fallbackStudentId = 0,
        int fallbackLabWorkId = 0
    );

private:
    SubmissionImportSummary importInternal(
        const std::vector<SubmissionImportItem> &items,
        int fallbackStudentId,
        int fallbackLabWorkId
    );

    SubmissionService &m_submissionService;
};

} // namespace labtester::services
