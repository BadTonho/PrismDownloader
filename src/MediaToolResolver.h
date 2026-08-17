#ifndef MEDIATOOLRESOLVER_H
#define MEDIATOOLRESOLVER_H

#include <QString>

enum class MediaTool {
    YtDlp,
    Ffmpeg
};

class MediaToolResolver {
public:
    static QString executableName(MediaTool tool);
    static QString resolve(MediaTool tool, const QString &programPath = {});
    static QString missingMessage(MediaTool tool);
};

#endif // MEDIATOOLRESOLVER_H
