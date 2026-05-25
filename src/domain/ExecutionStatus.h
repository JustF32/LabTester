#pragma once

#include <QMetaType>
#include <QString>

namespace labtester::domain {

enum class ExecutionStatus {
    Pending,
    Passed,
    Failed,
    BuildError
};

inline QString toDisplayString(ExecutionStatus status)
{
    switch (status) {
    case ExecutionStatus::Pending:
        return QStringLiteral("Ожидает");
    case ExecutionStatus::Passed:
        return QStringLiteral("Успешно");
    case ExecutionStatus::Failed:
        return QStringLiteral("Провалено");
    case ExecutionStatus::BuildError:
        return QStringLiteral("Ошибка сборки");
    }
    return QStringLiteral("Ожидает");
}

inline QString toStorageString(ExecutionStatus status)
{
    switch (status) {
    case ExecutionStatus::Pending:
        return QStringLiteral("Pending");
    case ExecutionStatus::Passed:
        return QStringLiteral("Passed");
    case ExecutionStatus::Failed:
        return QStringLiteral("Failed");
    case ExecutionStatus::BuildError:
        return QStringLiteral("BuildError");
    }
    return QStringLiteral("Pending");
}

inline ExecutionStatus executionStatusFromString(const QString &value)
{
    const QString normalized = value.trimmed();

    if (normalized.compare(QStringLiteral("Passed"), Qt::CaseInsensitive) == 0
        || normalized.compare(QStringLiteral("Успешно"), Qt::CaseInsensitive) == 0) {
        return ExecutionStatus::Passed;
    }
    if (normalized.compare(QStringLiteral("Failed"), Qt::CaseInsensitive) == 0
        || normalized.compare(QStringLiteral("Провалено"), Qt::CaseInsensitive) == 0) {
        return ExecutionStatus::Failed;
    }
    if (normalized.compare(QStringLiteral("BuildError"), Qt::CaseInsensitive) == 0
        || normalized.compare(QStringLiteral("Build Error"), Qt::CaseInsensitive) == 0
        || normalized.compare(QStringLiteral("Ошибка сборки"), Qt::CaseInsensitive) == 0) {
        return ExecutionStatus::BuildError;
    }
    return ExecutionStatus::Pending;
}

} // namespace labtester::domain

Q_DECLARE_METATYPE(labtester::domain::ExecutionStatus)
