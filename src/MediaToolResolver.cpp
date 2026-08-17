#include "MediaToolResolver.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QVersionNumber>

namespace {

QString packageName(MediaTool tool)
{
    return tool == MediaTool::YtDlp ? QStringLiteral("yt-dlp") : QStringLiteral("ffmpeg");
}

int sourcePriority(MediaToolSource source)
{
    switch (source) {
    case MediaToolSource::Explicit:
        return 4;
    case MediaToolSource::UserUpdate:
        return 3;
    case MediaToolSource::Path:
        return 2;
    case MediaToolSource::Bundled:
        return 1;
    case MediaToolSource::Unavailable:
        return 0;
    }
    return 0;
}

QVersionNumber parsedVersion(const QString &version)
{
    static const QRegularExpression versionPattern(
        QStringLiteral("(\\d+(?:\\.\\d+)+)"));
    const QRegularExpressionMatch match = versionPattern.match(version);
    return match.hasMatch() ? QVersionNumber::fromString(match.captured(1)) : QVersionNumber();
}

void appendCandidate(QList<MediaToolInfo> &candidates, QSet<QString> &knownPaths,
                     const QString &path, MediaToolSource source)
{
    if (path.isEmpty()) {
        return;
    }

    const QFileInfo fileInfo(path);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return;
    }

    const QString normalizedPath = fileInfo.canonicalFilePath().isEmpty()
        ? fileInfo.absoluteFilePath()
        : fileInfo.canonicalFilePath();
    if (knownPaths.contains(normalizedPath)) {
        return;
    }
    knownPaths.insert(normalizedPath);

    const QString version = MediaToolResolver::versionForExecutable(normalizedPath);
    if (!parsedVersion(version).isNull()) {
        candidates.append({QDir::toNativeSeparators(normalizedPath), version, source});
    }
}

QStringList executablePathsFromEnvironment(const QString &executable)
{
    const QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    QString pathVariable = environment.value(QStringLiteral("PATH"));
#ifdef Q_OS_WIN
    if (pathVariable.isEmpty()) {
        pathVariable = environment.value(QStringLiteral("Path"));
    }
#endif
    QStringList result;
    for (const QString &directory : pathVariable.split(QDir::listSeparator(), Qt::SkipEmptyParts)) {
        result.append(QDir(directory).absoluteFilePath(executable));
    }
    return result;
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
    return resolveInfo(tool, programPath).path;
}

MediaToolInfo MediaToolResolver::resolveInfo(MediaTool tool, const QString &programPath)
{
    if (!programPath.isEmpty()) {
        return {QDir::toNativeSeparators(programPath), {}, MediaToolSource::Explicit};
    }

    if (tool != MediaTool::YtDlp) {
#ifdef Q_OS_WIN
        return {QDir::toNativeSeparators(
                    QCoreApplication::applicationDirPath() + QLatin1Char('/') + executableName(tool)),
                {}, MediaToolSource::Bundled};
#else
        const QString path = QStandardPaths::findExecutable(executableName(tool));
        return {path, {}, path.isEmpty() ? MediaToolSource::Unavailable : MediaToolSource::Path};
#endif
    }

    QList<MediaToolInfo> candidates;
    QSet<QString> knownPaths;
    appendCandidate(candidates, knownPaths, ytDlpUserPath(), MediaToolSource::UserUpdate);
    for (const QString &path : executablePathsFromEnvironment(executableName(MediaTool::YtDlp))) {
        appendCandidate(candidates, knownPaths, path, MediaToolSource::Path);
    }
    appendCandidate(candidates, knownPaths, ytDlpBundledPath(), MediaToolSource::Bundled);
    return selectYtDlpCandidate(candidates);
}

MediaToolInfo MediaToolResolver::selectYtDlpCandidate(const QList<MediaToolInfo> &candidates)
{
    MediaToolInfo selected;
    QVersionNumber selectedVersion;
    for (const MediaToolInfo &candidate : candidates) {
        const QVersionNumber candidateVersion = parsedVersion(candidate.version);
        if (candidate.path.isEmpty() || candidateVersion.isNull()) {
            continue;
        }

        const int comparison = selectedVersion.isNull()
            ? 1
            : QVersionNumber::compare(candidateVersion, selectedVersion);
        if (comparison > 0 || (comparison == 0 && sourcePriority(candidate.source) > sourcePriority(selected.source))) {
            selected = candidate;
            selectedVersion = candidateVersion;
        }
    }
    return selected;
}

QString MediaToolResolver::versionForExecutable(const QString &programPath)
{
    if (programPath.isEmpty() || !QFileInfo::exists(programPath)) {
        return {};
    }

    QProcess probe;
    probe.setProcessChannelMode(QProcess::MergedChannels);
    probe.start(programPath, {QStringLiteral("--version")});
    if (!probe.waitForStarted(1500)) {
        if (probe.state() != QProcess::NotRunning) {
            probe.kill();
            probe.waitForFinished(1000);
        }
        return {};
    }
    if (!probe.waitForFinished(4000)) {
        probe.kill();
        probe.waitForFinished(1000);
        return {};
    }
    if (probe.exitStatus() != QProcess::NormalExit || probe.exitCode() != 0) {
        return {};
    }

    static const QRegularExpression versionPattern(
        QStringLiteral("(\\d+(?:\\.\\d+)+)"));
    const QRegularExpressionMatch match = versionPattern.match(QString::fromUtf8(probe.readAll()));
    return match.hasMatch() ? match.captured(1) : QString();
}

bool MediaToolResolver::isVersionNewer(const QString &candidate, const QString &current)
{
    const QVersionNumber candidateVersion = parsedVersion(candidate);
    const QVersionNumber currentVersion = parsedVersion(current);
    return !candidateVersion.isNull()
        && (currentVersion.isNull() || QVersionNumber::compare(candidateVersion, currentVersion) > 0);
}

QString MediaToolResolver::sourceLabel(MediaToolSource source)
{
    switch (source) {
    case MediaToolSource::Explicit:
        return QStringLiteral("caminho personalizado");
    case MediaToolSource::UserUpdate:
        return QStringLiteral("atualização do Prism");
    case MediaToolSource::Path:
        return QStringLiteral("PATH do sistema");
    case MediaToolSource::Bundled:
        return QStringLiteral("incluído no Prism");
    case MediaToolSource::Unavailable:
        return QStringLiteral("indisponível");
    }
    return QStringLiteral("indisponível");
}

QString MediaToolResolver::ytDlpUserPath()
{
    const QString basePath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return basePath.isEmpty() ? QString()
        : QDir(basePath).absoluteFilePath(QStringLiteral("tools/") + executableName(MediaTool::YtDlp));
}

QString MediaToolResolver::ytDlpBundledPath()
{
#ifdef Q_OS_WIN
    return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(executableName(MediaTool::YtDlp));
#elif defined(Q_OS_LINUX)
    return QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath(QStringLiteral("../lib/prism-downloader/yt-dlp"));
#else
    return {};
#endif
}

QString MediaToolResolver::missingMessage(MediaTool tool)
{
    const QString executable = executableName(tool);
#ifdef Q_OS_WIN
    return tool == MediaTool::YtDlp
        ? QStringLiteral("%1 não foi encontrado ou não respondeu com uma versão válida. Reinstale o Prism Downloader.").arg(executable)
        : QStringLiteral("%1 não foi encontrado na pasta do aplicativo.").arg(executable);
#elif defined(Q_OS_LINUX)
    return tool == MediaTool::YtDlp
        ? QStringLiteral("yt-dlp não foi encontrado ou não respondeu com uma versão válida. Reinstale o Prism Downloader ou adicione um yt-dlp funcional ao PATH.")
        : QStringLiteral("%1 não foi encontrado. Instale-o com: sudo apt install %2")
              .arg(executable, packageName(tool));
#else
    return QStringLiteral("%1 não foi encontrado. Instale-o e verifique se está disponível no PATH.")
        .arg(executable);
#endif
}
