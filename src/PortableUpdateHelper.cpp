#include "PortableUpdateCommon.h"

#include <QCoreApplication>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

bool waitForParent(qint64 pid)
{
#ifdef Q_OS_WIN
    HANDLE handle = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (!handle) {
        return true;
    }
    const DWORD result = WaitForSingleObject(handle, 120000);
    CloseHandle(handle);
    return result == WAIT_OBJECT_0;
#else
    Q_UNUSED(pid)
    return false;
#endif
}

bool renameDirectory(const QString &from, const QString &to)
{
    const QFileInfo source(from);
    const QFileInfo destination(to);
    return source.dir().rename(source.fileName(), destination.fileName());
}

}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList arguments = app.arguments();
    if (arguments.size() != 7 || arguments.at(1) != QStringLiteral("--parent-pid")
        || arguments.at(3) != QStringLiteral("--archive")
        || arguments.at(5) != QStringLiteral("--target")) {
        return 2;
    }
    bool pidOk = false;
    const qint64 parentPid = arguments.at(2).toLongLong(&pidOk);
    const QString archive = arguments.at(4);
    const QString target = QDir::cleanPath(arguments.at(6));
    if (!pidOk || parentPid <= 0 || !QFileInfo::exists(archive) || !QFileInfo(target).isDir()) {
        return 3;
    }
    if (!waitForParent(parentPid)) {
        return 4;
    }

    const QFileInfo targetInfo(target);
    const QString parentDirectory = targetInfo.dir().absolutePath();
    QTemporaryDir staging(parentDirectory + QStringLiteral("/.prism-update-staging-XXXXXX"));
    if (!staging.isValid()) {
        return 5;
    }

    QProcess extractor;
    const QString script = PortableUpdateCommon::extractionScript(archive, staging.path());
    const QByteArray scriptBytes(reinterpret_cast<const char *>(script.utf16()),
                                 script.size() * static_cast<int>(sizeof(ushort)));
    const QString encodedScript = QString::fromLatin1(scriptBytes.toBase64());
    extractor.start(QStringLiteral("powershell.exe"), {
        QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
        QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
        QStringLiteral("-EncodedCommand"), encodedScript});
    if (!extractor.waitForStarted(10000) || !extractor.waitForFinished(-1)
        || extractor.exitStatus() != QProcess::NormalExit || extractor.exitCode() != 0) {
        return 6;
    }

    const QString executable = QDir(staging.path()).filePath(QStringLiteral("PrismDownloader.exe"));
    if (!QFileInfo(executable).isFile()) {
        return 7;
    }

    const QString backup = parentDirectory + QStringLiteral("/.prism-update-backup-%1").arg(parentPid);
    if (QFileInfo::exists(backup) || !renameDirectory(target, backup)
        || !renameDirectory(staging.path(), target)) {
        if (QFileInfo::exists(backup) && !QFileInfo::exists(target)) {
            renameDirectory(backup, target);
        }
        return 8;
    }
    staging.setAutoRemove(false);

    const QString newExecutable = QDir(target).filePath(QStringLiteral("PrismDownloader.exe"));
    if (!QProcess::startDetached(newExecutable)) {
        QDir(target).removeRecursively();
        renameDirectory(backup, target);
        return 9;
    }
    QFile::remove(archive);
    QDir(backup).removeRecursively();
    return 0;
}
