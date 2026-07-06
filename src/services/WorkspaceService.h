#pragma once

#include <QString>

#include "domain/Submission.h"

namespace labtester::services {

class WorkspaceService {
public:
    WorkspaceService() = default;

    QString resolveWorkspaceFor(const domain::Submission &submission) const;
};

} // namespace labtester::services
