#include "YtDlpStatus.h"

#include "MediaToolResolver.h"

namespace YtDlpStatus {

QString description()
{
    const MediaToolInfo info = MediaToolResolver::resolveInfo(MediaTool::YtDlp);
    if (!info.isAvailable()) {
        return QStringLiteral("yt-dlp: nenhuma cópia funcional foi localizada.");
    }
    return QStringLiteral("yt-dlp em uso: %1 (%2)")
        .arg(info.version, MediaToolResolver::sourceLabel(info.source));
}

}
