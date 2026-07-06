#include "models/ResultListModel.h"

namespace labtester::models {

ResultListModel::ResultListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ResultListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_results.size());
}

QVariant ResultListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return {};
    }

    const int row = index.row();
    if (row < 0 || row >= static_cast<int>(m_results.size())) {
        return {};
    }

    const domain::TestRunResult &result = m_results[static_cast<size_t>(row)];
    switch (role) {
    case SubmissionIdRole:
        return result.submissionId;
    case StudentNameRole:
        return result.studentName;
    case LabTitleRole:
        return result.labTitle;
    case TotalTestsRole:
        return result.totalTests;
    case PassedTestsRole:
        return result.passedTests;
    case FailedTestsRole:
        return result.failedTests;
    case StatusRole:
        return result.displayStatus.isEmpty()
            ? domain::toDisplayString(result.status)
            : result.displayStatus;
    case StatusKeyRole:
        return result.displayStatusKey.isEmpty()
            ? domain::toStorageString(result.status)
            : result.displayStatusKey;
    case MessageRole:
        return result.message;
    case ExecutedAtRole:
        return result.executedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    default:
        return {};
    }
}

QHash<int, QByteArray> ResultListModel::roleNames() const
{
    return {
        {SubmissionIdRole, "submissionId"},
        {StudentNameRole, "studentName"},
        {LabTitleRole, "labTitle"},
        {TotalTestsRole, "totalTests"},
        {PassedTestsRole, "passedTests"},
        {FailedTestsRole, "failedTests"},
        {StatusRole, "status"},
        {StatusKeyRole, "statusKey"},
        {MessageRole, "message"},
        {ExecutedAtRole, "executedAt"},
    };
}

void ResultListModel::setResults(const std::vector<domain::TestRunResult> &results)
{
    beginResetModel();
    m_results = results;
    endResetModel();
}

void ResultListModel::upsertResult(const domain::TestRunResult &result)
{
    for (int i = 0; i < static_cast<int>(m_results.size()); ++i) {
        if (m_results[static_cast<size_t>(i)].submissionId == result.submissionId) {
            m_results[static_cast<size_t>(i)] = result;
            const QModelIndex modelIndex = index(i, 0);
            emit dataChanged(modelIndex, modelIndex);
            return;
        }
    }

    const int insertRow = static_cast<int>(m_results.size());
    beginInsertRows(QModelIndex(), insertRow, insertRow);
    m_results.push_back(result);
    endInsertRows();
}

} // namespace labtester::models
