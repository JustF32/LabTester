#pragma once

#include <QDateTime>
#include <QString>

#include "domain/ExecutionStatus.h"
#include "domain/LabWork.h"
#include "domain/Student.h"

namespace labtester::domain {

struct Submission {
    int id {0};
    Student student;
    LabWork labWork;
    QString sourcePath;
    ExecutionStatus status {ExecutionStatus::Pending};
    QDateTime createdAt {QDateTime::currentDateTime()};
};

} // namespace labtester::domain
