#include "models/SubmissionListModel.h"

namespace labtester::models {

SubmissionListModel::SubmissionListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int SubmissionListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_submissions.size());
}

QVariant SubmissionListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return {};
    }

    const int row = index.row();
    if (row < 0 || row >= static_cast<int>(m_submissions.size())) {
        return {};
    }

    const domain::Submission &submission = m_submissions[static_cast<size_t>(row)];
    switch (role) {
    case SubmissionIdRole:
        return submission.id;
    case StudentNameRole:
        return submission.student.name;
    case GroupRole:
        return submission.student.groupName;
    case LabTitleRole:
        return submission.labWork.title;
    case LanguageRole:
        return submission.labWork.language;
    case SourcePathRole:
        return submission.sourcePath;
    case CreatedAtRole:
        return submission.createdAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    case StatusRole:
        return domain::toDisplayString(submission.status);
    case StatusKeyRole:
        return domain::toStorageString(submission.status);
    default:
        return {};
    }
}

QHash<int, QByteArray> SubmissionListModel::roleNames() const
{
    return {
        {SubmissionIdRole, "submissionId"},
        {StudentNameRole, "studentName"},
        {GroupRole, "groupName"},
        {LabTitleRole, "labTitle"},
        {LanguageRole, "language"},
        {SourcePathRole, "sourcePath"},
        {CreatedAtRole, "createdAt"},
        {StatusRole, "status"},
        {StatusKeyRole, "statusKey"},
    };
}

void SubmissionListModel::setSubmissions(const std::vector<domain::Submission> &submissions)
{
    beginResetModel();
    m_submissions = submissions;
    endResetModel();
}

const std::vector<domain::Submission> &SubmissionListModel::submissions() const
{
    return m_submissions;
}

} // namespace labtester::models
