#pragma once

#include <QString>

namespace labtester::domain {

struct Student {
    int id {0};
    QString name;
    QString groupName;
};

} // namespace labtester::domain
