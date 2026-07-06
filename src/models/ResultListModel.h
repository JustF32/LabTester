#pragma once

#include <QAbstractListModel>

#include <vector>

#include "domain/TestRunResult.h"

namespace labtester::models {

class ResultListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        SubmissionIdRole = Qt::UserRole + 1,
        StudentNameRole,
        LabTitleRole,
        TotalTestsRole,
        PassedTestsRole,
        FailedTestsRole,
        StatusRole,
        StatusKeyRole,
        MessageRole,
        ExecutedAtRole
    };
    Q_ENUM(Role)

    explicit ResultListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setResults(const std::vector<domain::TestRunResult> &results);
    void upsertResult(const domain::TestRunResult &result);

private:
    std::vector<domain::TestRunResult> m_results;
};

} // namespace labtester::models
