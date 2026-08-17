#ifndef MEDIATOOLRESOLVER_H
#define MEDIATOOLRESOLVER_H

#include <QString>
#include <QList>

enum class MediaTool {
    YtDlp,
    Ffmpeg
};

enum class MediaToolSource {
    Unavailable,
    Explicit,
    UserUpdate,
    Path,
    Bundled
};

struct MediaToolInfo {
    QString path;
    QString version;
    MediaToolSource source{MediaToolSource::Unavailable};

    bool isAvailable() const { return !path.isEmpty(); }
};

class MediaToolResolver {
public:
    static QString executableName(MediaTool tool);
    static QString resolve(MediaTool tool, const QString &programPath = {});
    static MediaToolInfo resolveInfo(MediaTool tool, const QString &programPath = {});
    static MediaToolInfo selectYtDlpCandidate(const QList<MediaToolInfo> &candidates);
    static QString versionForExecutable(const QString &programPath);
    static bool isVersionNewer(const QString &candidate, const QString &current);
    static QString sourceLabel(MediaToolSource source);
    static QString ytDlpUserPath();
    static QString ytDlpBundledPath();
    static QString missingMessage(MediaTool tool);
};

#endif // MEDIATOOLRESOLVER_H
