#include "MediaToolResolver.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

namespace {

QString packageName(MediaTool tool)
{
    return tool == MediaTool::YtDlp ? QStringLiteral("yt-dlp") : QStringLiteral("ffmpeg");
}

}

QString MediaToolResolver::executableName(MediaTool tool)
{
#ifdef Q_OS_WIN
    return tool == MediaTool::YtDlp ? QStringLiteral("yt-dlp.exe") : QStringLiteral("ffmpeg.exe");
#else
    return packageName(tool);
#endif
}

QString MediaToolResolver::resolve(MediaTool tool, const QString &programPath)
{
    if (!programPath.isEmpty()) {
        return QDir::toNativeSeparators(programPath);
    }

#ifdef Q_OS_WIN
    return QDir::toNativeSeparators(
        QCoreApplication::applicationDirPath() + QLatin1Char('/') + executableName(tool));
#else
    return QStandardPaths::findExecutable(executableName(tool));
#endif
}

QString MediaToolResolver::missingMessage(MediaTool tool)
{
    const QString executable = executableName(tool);
#ifdef Q_OS_WIN
    return QStringLiteral("%1 não foi encontrado na pasta do aplicativo.").arg(executable);
#elif defined(Q_OS_LINUX)
    return QStringLiteral("%1 não foi encontrado. Instale-o com: sudo apt install %2")
        .arg(executable, packageName(tool));
#else
    return QStringLiteral("%1 não foi encontrado. Instale-o e verifique se está disponível no PATH.")
        .arg(executable);
#endif
}
