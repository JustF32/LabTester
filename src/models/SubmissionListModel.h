#pragma once

#include <QAbstractListModel>

#include <vector>

#include "domain/Submission.h"

namespace labtester::models {

class SubmissionListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        SubmissionIdRole = Qt::UserRole + 1,
        StudentNameRole,
        GroupRole,
        LabTitleRole,
        LanguageRole,
        SourcePathRole,
        CreatedAtRole,
        StatusRole,
        StatusKeyRole
    };
    Q_ENUM(Role)

    explicit SubmissionListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setSubmissions(const std::vector<domain::Submission> &submissions);
    const std::vector<domain::Submission> &submissions() const;

private:
    std::vector<domain::Submission> m_submissions;
};

} // namespace labtester::models
