#include "PortableUpdateCommon.h"

#include <QCoreApplication>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

#include <string>

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

// A atualização troca a pasta inteira. Tudo o que o usuário deixou na pasta
// Portable e que não faz parte do novo pacote (downloads, binários próprios,
// anotações) é devolvido à pasta nova antes de o backup ser descartado.
void restoreUserEntries(const QString &backup, const QString &target)
{
    QDir backupDirectory(backup);
    const QDir targetDirectory(target);
    const QFileInfoList entries = backupDirectory.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo &entry : entries) {
        if (targetDirectory.exists(entry.fileName())) {
            continue;
        }
        backupDirectory.rename(entry.fileName(), targetDirectory.filePath(entry.fileName()));
    }
}

void reportFailure(int code)
{
#ifdef Q_OS_WIN
    const wchar_t *reason = L"Erro desconhecido.";
    switch (code) {
    case 3: reason = L"Os argumentos da atualização são inválidos."; break;
    case 4: reason = L"O Prism Downloader não foi encerrado a tempo."; break;
    case 5: reason = L"Não foi possível criar a pasta temporária ao lado da instalação."; break;
    case 6: reason = L"Não foi possível extrair o pacote de atualização."; break;
    case 7: reason = L"O pacote baixado não contém o PrismDownloader.exe."; break;
    case 8: reason = L"Não foi possível substituir os arquivos (algum arquivo da pasta está em uso?)."; break;
    case 9: reason = L"A nova versão não pôde ser iniciada; a versão anterior foi restaurada."; break;
    default: break;
    }
    const std::wstring message = std::wstring(L"A atualização Portable falhou (código ")
        + std::to_wstring(code) + L").\n\n" + reason
        + L"\n\nA versão anterior foi mantida.";
    MessageBoxW(nullptr, message.c_str(), L"Prism Downloader - Atualização",
                MB_OK | MB_ICONWARNING | MB_SETFOREGROUND);
#else
    Q_UNUSED(code)
#endif
}

int run(const QStringList &arguments)
{
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

    // O tar.exe nativo do Windows 10+ (bsdtar) lê .zip e recusa por padrão
    // entradas com ".." ou caminhos absolutos. Evita PowerShell codificado.
    const QString systemRoot = qEnvironmentVariable("SystemRoot", QStringLiteral("C:/Windows"));
    const QString tar = QDir(systemRoot).filePath(QStringLiteral("System32/tar.exe"));
    QProcess extractor;
    extractor.start(tar, {QStringLiteral("-xf"), QDir::toNativeSeparators(archive),
                          QStringLiteral("-C"), QDir::toNativeSeparators(staging.path())});
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
    restoreUserEntries(backup, target);
    QDir(backup).removeRecursively();
    return 0;
}

}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList arguments = app.arguments();
    const int code = run(arguments);
    if (code != 0) {
        reportFailure(code);
        // Reabre a versão atual para que o app não simplesmente desapareça.
        const QString target = arguments.size() == 7 ? QDir::cleanPath(arguments.at(6)) : QString();
        const QString executable = QDir(target).filePath(QStringLiteral("PrismDownloader.exe"));
        if (!target.isEmpty() && QFileInfo(executable).isFile()) {
            QProcess::startDetached(executable);
        }
    }
    return code;
}
