#include "MediaToolResolver.h"

#include <QCoreApplication>
#include <QDir>

#include <iostream>

namespace {

bool check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);

    const QString customYtDlp = QStringLiteral("/tmp/prism-test-yt-dlp");
    const QString customFfmpeg = QStringLiteral("/tmp/prism-test-ffmpeg");
    bool success = check(MediaToolResolver::resolve(MediaTool::YtDlp, customYtDlp)
                             == QDir::toNativeSeparators(customYtDlp),
                         "custom yt-dlp path is preserved")
        && check(MediaToolResolver::resolve(MediaTool::Ffmpeg, customFfmpeg)
                     == QDir::toNativeSeparators(customFfmpeg),
                 "custom FFmpeg path is preserved")
        && check(MediaToolResolver::isVersionNewer(QStringLiteral("2026.08.17.073947"),
                                                   QStringLiteral("2026.08.16.235959")),
                 "newer nightly version is detected")
        && check(!MediaToolResolver::isVersionNewer(QStringLiteral("invalid"),
                                                    QStringLiteral("2026.08.16")),
                 "invalid version is never newer");

    const MediaToolInfo bundled{
        QStringLiteral("/bundle/yt-dlp"), QStringLiteral("2026.08.15"), MediaToolSource::Bundled};
    const MediaToolInfo path{
        QStringLiteral("/usr/local/bin/yt-dlp"), QStringLiteral("2026.08.16"), MediaToolSource::Path};
    const MediaToolInfo userUpdate{
        QStringLiteral("/home/user/.local/share/prism/yt-dlp"), QStringLiteral("2026.08.17"), MediaToolSource::UserUpdate};
    const MediaToolInfo selection = MediaToolResolver::selectYtDlpCandidate({bundled, path, userUpdate});
    success = check(selection.path == userUpdate.path && selection.version == userUpdate.version,
                    "newest user update wins over PATH and bundled copy")
        && success;

    const MediaToolInfo tiedSelection = MediaToolResolver::selectYtDlpCandidate({
        {bundled.path, QStringLiteral("2026.08.17"), MediaToolSource::Bundled},
        {path.path, QStringLiteral("2026.08.17"), MediaToolSource::Path},
        {userUpdate.path, QStringLiteral("2026.08.17"), MediaToolSource::UserUpdate}
    });
    success = check(tiedSelection.path == userUpdate.path,
                    "user update wins ties deterministically")
        && check(MediaToolResolver::selectYtDlpCandidate({
                      {QStringLiteral("/bad/yt-dlp"), QStringLiteral("not-a-version"), MediaToolSource::Path}
                  }).path.isEmpty(),
                  "invalid yt-dlp candidate is ignored")
        && success;

#ifdef Q_OS_WIN
    success = check(MediaToolResolver::executableName(MediaTool::YtDlp) == QStringLiteral("yt-dlp.exe"),
                    "Windows uses yt-dlp.exe")
        && check(MediaToolResolver::missingMessage(MediaTool::Ffmpeg).contains(QStringLiteral("ffmpeg.exe")),
                 "Windows missing-tool message names FFmpeg executable")
        && success;
#else
    success = check(MediaToolResolver::executableName(MediaTool::YtDlp) == QStringLiteral("yt-dlp"),
                    "Linux uses yt-dlp executable name")
        && check(MediaToolResolver::executableName(MediaTool::Ffmpeg) == QStringLiteral("ffmpeg"),
                 "Linux uses FFmpeg from PATH")
        && success;
#endif

#ifdef Q_OS_LINUX
    success = check(MediaToolResolver::missingMessage(MediaTool::YtDlp)
                        .contains(QStringLiteral("Reinstale o Prism Downloader")),
                    "Linux missing yt-dlp message explains the bundled recovery")
        && success;
#endif

    return success ? 0 : 1;
}
