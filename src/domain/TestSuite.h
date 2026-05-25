#pragma once

#include <QString>

namespace labtester::domain {

struct TestSuite {
    int id {0};
    QString name;
    QString language;
    QString runnerType;
};

} // namespace labtester::domain
