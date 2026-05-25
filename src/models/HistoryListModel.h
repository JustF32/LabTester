#pragma once

#include <QAbstractListModel>

#include <vector>

#include "domain/CheckRunHistoryEntry.h"

namespace labtester::models {

class HistoryListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        CheckRunIdRole = Qt::UserRole + 1,
        SubmissionIdRole,
        StudentNameRole,
        GroupNameRole,
        LabTitleRole,
        TotalTestsRole,
        PassedTestsRole,
        FailedTestsRole,
        StarsRole,
        StatusRole,
        StatusKeyRole,
        MessageRole,
        ExecutedAtRole
    };

    explicit HistoryListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntries(const std::vector<domain::CheckRunHistoryEntry> &entries);

private:
    std::vector<domain::CheckRunHistoryEntry> m_entries;
};

} // namespace labtester::models
