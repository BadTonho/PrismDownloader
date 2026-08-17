#include "MediaToolResolver.h"

#include <QCoreApplication>

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
    bool success = check(MediaToolResolver::resolve(MediaTool::YtDlp, customYtDlp) == customYtDlp,
                         "custom yt-dlp path is preserved")
        && check(MediaToolResolver::resolve(MediaTool::Ffmpeg, customFfmpeg) == customFfmpeg,
                 "custom FFmpeg path is preserved");

#ifdef Q_OS_WIN
    success = check(MediaToolResolver::executableName(MediaTool::YtDlp) == QStringLiteral("yt-dlp.exe"),
                    "Windows uses yt-dlp.exe")
        && check(MediaToolResolver::missingMessage(MediaTool::Ffmpeg).contains(QStringLiteral("ffmpeg.exe")),
                 "Windows missing-tool message names FFmpeg executable")
        && success;
#else
    success = check(MediaToolResolver::executableName(MediaTool::YtDlp) == QStringLiteral("yt-dlp"),
                    "Linux uses yt-dlp from PATH")
        && check(MediaToolResolver::executableName(MediaTool::Ffmpeg) == QStringLiteral("ffmpeg"),
                 "Linux uses FFmpeg from PATH")
        && success;
#endif

#ifdef Q_OS_LINUX
    success = check(MediaToolResolver::missingMessage(MediaTool::YtDlp)
                        .contains(QStringLiteral("sudo apt install yt-dlp")),
                    "Linux missing-tool message gives apt command")
        && success;
#endif

    return success ? 0 : 1;
}
