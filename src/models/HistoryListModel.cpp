#include "models/HistoryListModel.h"

namespace labtester::models {

HistoryListModel::HistoryListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int HistoryListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_entries.size());
}

QVariant HistoryListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return {};
    }

    const int row = index.row();
    if (row < 0 || row >= static_cast<int>(m_entries.size())) {
        return {};
    }

    const domain::CheckRunHistoryEntry &entry = m_entries[static_cast<size_t>(row)];
    switch (role) {
    case CheckRunIdRole:
        return entry.checkRunId;
    case SubmissionIdRole:
        return entry.submissionId;
    case StudentNameRole:
        return entry.studentName;
    case GroupNameRole:
        return entry.groupName;
    case LabTitleRole:
        return entry.labTitle;
    case TotalTestsRole:
        return entry.totalTests;
    case PassedTestsRole:
        return entry.passedTests;
    case FailedTestsRole:
        return entry.failedTests;
    case StarsRole:
        return entry.stars;
    case StatusRole:
        return entry.statusText.isEmpty()
            ? domain::toDisplayString(entry.status)
            : entry.statusText;
    case StatusKeyRole:
        return entry.statusKey.isEmpty()
            ? domain::toStorageString(entry.status)
            : entry.statusKey;
    case MessageRole:
        return entry.message;
    case ExecutedAtRole:
        return entry.executedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    default:
        return {};
    }
}

QHash<int, QByteArray> HistoryListModel::roleNames() const
{
    return {
        {CheckRunIdRole, "checkRunId"},
        {SubmissionIdRole, "submissionId"},
        {StudentNameRole, "studentName"},
        {GroupNameRole, "groupName"},
        {LabTitleRole, "labTitle"},
        {TotalTestsRole, "totalTests"},
        {PassedTestsRole, "passedTests"},
        {FailedTestsRole, "failedTests"},
        {StarsRole, "stars"},
        {StatusRole, "status"},
        {StatusKeyRole, "statusKey"},
        {MessageRole, "message"},
        {ExecutedAtRole, "executedAt"},
    };
}

void HistoryListModel::setEntries(const std::vector<domain::CheckRunHistoryEntry> &entries)
{
    beginResetModel();
    m_entries = entries;
    endResetModel();
}

} // namespace labtester::models
