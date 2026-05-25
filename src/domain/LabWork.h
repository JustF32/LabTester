#pragma once

#include <QString>

namespace labtester::domain {

struct LabWork {
    int id {0};
    QString title;
    QString language;
    QString description;
    QString manifestPath;
    QString templateFile;
    QString expectedInput;
    QString expectedOutput;
    QString testSuitePath;
    QString referenceHeaderPath;
};

} // namespace labtester::domain
