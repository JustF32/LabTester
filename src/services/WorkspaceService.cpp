#include "services/WorkspaceService.h"

namespace labtester::services {

QString WorkspaceService::resolveWorkspaceFor(const domain::Submission &submission) const
{
    if (!submission.sourcePath.isEmpty()) {
        return submission.sourcePath;
    }
    return QStringLiteral("workspace/submission_%1").arg(submission.id);
}

} // namespace labtester::services
