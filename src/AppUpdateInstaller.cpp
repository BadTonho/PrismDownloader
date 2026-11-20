#include "AppUpdateInstaller.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QUuid>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

#ifdef Q_OS_WIN
HANDLE g_lockedPackage = INVALID_HANDLE_VALUE;

void releaseLockedPackage()
{
    if (g_lockedPackage != INVALID_HANDLE_VALUE) {
        CloseHandle(g_lockedPackage);
        g_lockedPackage = INVALID_HANDLE_VALUE;
    }
}

// Abre o pacote negando escrita/remoção por terceiros, confere o SHA-256 pelo
// próprio handle e o mantém aberto até o processo encerrar. Assim o arquivo que
// será executado é exatamente o que foi verificado.
bool lockAndVerifyPackage(const QString &path, const QString &expectedSha256)
{
    HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ,
                                FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    QCryptographicHash hasher(QCryptographicHash::Sha256);
    QByteArray buffer(1024 * 1024, Qt::Uninitialized);
    DWORD read = 0;
    while (ReadFile(handle, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)
           && read > 0) {
        hasher.addData(QByteArrayView(buffer.constData(), static_cast<qsizetype>(read)));
    }
    if (hasher.result().toHex().compare(expectedSha256.toLatin1(), Qt::CaseInsensitive) != 0) {
        CloseHandle(handle);
        return false;
    }
    releaseLockedPackage();
    g_lockedPackage = handle; // mantido aberto até a instalação ser entregue ou falhar
    return true;
}
#endif

void removeOldEntries(const QString &directoryPath, qint64 maximumAgeSeconds)
{
    const QDir directory(directoryPath);
    const QFileInfoList entries = directory.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    const QDateTime limit = QDateTime::currentDateTime().addSecs(-maximumAgeSeconds);
    for (const QFileInfo &entry : entries) {
        if (entry.lastModified() > limit) {
            continue;
        }
        if (entry.isDir()) {
            QDir(entry.absoluteFilePath()).removeRecursively();
        } else {
            QFile::remove(entry.absoluteFilePath());
        }
    }
}

}

void AppUpdateInstaller::cleanupStaleFiles()
{
    const QString temp = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (temp.isEmpty()) {
        return;
    }
    const QDir root(QDir(temp).filePath(QStringLiteral("PrismDownloader")));
    removeOldEntries(root.filePath(QStringLiteral("updates")), 24 * 3600);
    removeOldEntries(root.filePath(QStringLiteral("update-helper")), 3600);
}

AppUpdateInstaller::AppUpdateInstaller(QObject *parent)
    : QObject(parent)
{
}

bool AppUpdateInstaller::isInstalling() const
{
    return m_installing;
}

void AppUpdateInstaller::install(const QString &version,
                                 const QString &packagePath,
                                 const QString &expectedSha256,
                                 bool installedWindowsCopy)
{
    if (isInstalling()) {
        return;
    }

    m_version = version;
    m_packagePath = packagePath;
    setInstalling(true);

#ifdef Q_OS_WIN
    if (!lockAndVerifyPackage(packagePath, expectedSha256)) {
        fail("O pacote mudou ou não pôde ser relido antes da instalação; atualização cancelada.",
             packagePath);
        return;
    }
    if (installedWindowsCopy) {
        // O instalador exige elevação (manifesto requireAdministrator), o que
        // CreateProcess/startDetached não consegue pedir. "runas" mostra o UAC e
        // só retorna sucesso se o usuário aceitar; assim o app continua aberto
        // caso a autorização seja recusada.
        const QString arguments = QStringLiteral(
            "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CLOSEAPPLICATIONS");
        const HINSTANCE result = ShellExecuteW(
            nullptr, L"runas", reinterpret_cast<LPCWSTR>(packagePath.utf16()),
            reinterpret_cast<LPCWSTR>(arguments.utf16()), nullptr, SW_HIDE);
        if (reinterpret_cast<INT_PTR>(result) <= 32) {
            fail("A instalação não foi autorizada ou o instalador não pôde ser iniciado. "
                 "A versão atual continua em execução.", packagePath);
            return;
        }
        emit logMessage("[Updater] Instalador Windows validado iniciado; encerrando a versão atual.");
        emit restartRequested();
        return;
    }

    const QString helperSource = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("PrismPortableUpdateHelper.exe"));
    const QString helperDirectory = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QStringLiteral("PrismDownloader/update-helper/%1")
                     .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    const QString helperCopy = QDir(helperDirectory)
        .filePath(QStringLiteral("PrismPortableUpdateHelper.exe"));
    // O helper depende de Qt6Core.dll; sem ela ao lado do .exe copiado ele
    // falharia ao carregar, silenciosamente.
    const QString coreSource = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("Qt6Core.dll"));
    if (!QFileInfo(helperSource).isFile() || !QFileInfo(coreSource).isFile()
        || !QDir().mkpath(helperDirectory)
        || !QFile::copy(helperSource, helperCopy)
        || !QFile::copy(coreSource, QDir(helperDirectory).filePath(QStringLiteral("Qt6Core.dll")))) {
        QDir(helperDirectory).removeRecursively();
        fail("O helper obrigatório para atualizar a cópia Portable não está disponível.",
             packagePath);
        return;
    }

    const bool started = QProcess::startDetached(helperCopy, {
        QStringLiteral("--parent-pid"), QString::number(QCoreApplication::applicationPid()),
        QStringLiteral("--archive"), packagePath,
        QStringLiteral("--target"), QCoreApplication::applicationDirPath()});
    if (!started) {
        QDir(helperDirectory).removeRecursively();
        fail("Não foi possível iniciar o helper da atualização Portable.", packagePath);
        return;
    }
    emit logMessage("[Updater] Helper Portable iniciado com pacote validado; encerrando a versão atual.");
    emit restartRequested();
#else
    const QString pkexec = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
    const QString aptGet = QStandardPaths::findExecutable(QStringLiteral("apt-get"));
    if (pkexec.isEmpty() || aptGet.isEmpty()) {
        fail("pkexec e apt-get são necessários para instalar atualizações no Linux.",
             packagePath);
        return;
    }

    auto *process = new QProcess(this);
    m_process = process;
    QObject::connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                     this, [this, process](int exitCode, QProcess::ExitStatus status) {
        if (m_process != process) {
            process->deleteLater();
            return;
        }
        m_process = nullptr;
        process->deleteLater();
        setInstalling(false);
        if (status != QProcess::NormalExit || exitCode != 0) {
            fail("O APT não instalou o pacote validado. A versão atual continua em execução.",
                 m_packagePath);
            return;
        }

        QFile::remove(m_packagePath);
        if (!QProcess::startDetached(QCoreApplication::applicationFilePath())) {
            emit failed("O pacote foi instalado, mas reinicie o Prism Downloader manualmente.");
            return;
        }
        emit logMessage("[Updater] Pacote Linux v" + m_version
                        + " instalado; reiniciando o aplicativo.");
        emit restartRequested();
    });
    QObject::connect(process, &QProcess::errorOccurred, this,
                     [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || m_process != process) {
            return;
        }
        m_process = nullptr;
        process->deleteLater();
        setInstalling(false);
        fail("Não foi possível iniciar o pkexec para instalar o pacote validado.",
             m_packagePath);
    });
    // O pacote fica num diretório do usuário, que pode trocá-lo depois do
    // download. Como root, copiamos o arquivo para um diretório privado, o
    // verificamos de novo e instalamos a cópia verificada, nunca o original.
    static const char *const script =
        "set -eu; apt_get=$1; pkg=$2; sum=$3; "
        "dir=$(mktemp -d); trap 'rm -rf \"$dir\"' EXIT; chmod 755 \"$dir\"; "
        "cp -- \"$pkg\" \"$dir/package.deb\"; chmod 644 \"$dir/package.deb\"; "
        "echo \"$sum  $dir/package.deb\" | sha256sum -c - >/dev/null; "
        "\"$apt_get\" install --yes \"$dir/package.deb\"";
    process->start(pkexec, {
        QStringLiteral("/bin/sh"), QStringLiteral("-c"), QString::fromLatin1(script),
        QStringLiteral("prism-update"), aptGet, packagePath, expectedSha256.toLower()});
    emit statusMessage("Aguardando autorização para instalar a atualização v" + version + ".");
#endif
}

void AppUpdateInstaller::setInstalling(bool installing)
{
    if (m_installing == installing) {
        return;
    }
    m_installing = installing;
    emit installingChanged(installing);
}

void AppUpdateInstaller::fail(const QString &message, const QString &packagePath)
{
#ifdef Q_OS_WIN
    releaseLockedPackage();
#endif
    if (!packagePath.isEmpty()) {
        QFile::remove(packagePath);
    }
    setInstalling(false);
    emit failed(message);
}
