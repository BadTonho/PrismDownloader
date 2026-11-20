#include "AppUpdateInstaller.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>

#include <cstdio>
#include <memory>

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
                                 AppUpdatePackageKind packageKind)
{
    const bool installedWindowsCopy = packageKind == AppUpdatePackageKind::WindowsSetup;
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
    switch (packageKind) {
    case AppUpdatePackageKind::LinuxAppImage:
        installAppImage(packagePath, expectedSha256);
        break;
    case AppUpdatePackageKind::LinuxTarball:
        installTarball(packagePath, expectedSha256);
        break;
    default:
        installWithPackageManager(packagePath, expectedSha256, packageKind);
        break;
    }
#endif
}

bool AppUpdateInstaller::fileMatchesSha256(const QString &path, const QString &expectedSha256)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QCryptographicHash hasher(QCryptographicHash::Sha256);
    if (!hasher.addData(&file)) {
        return false;
    }
    return hasher.result().toHex().compare(expectedSha256.toLatin1(), Qt::CaseInsensitive) == 0;
}

void AppUpdateInstaller::installWithPackageManager(const QString &packagePath,
                                                   const QString &expectedSha256,
                                                   AppUpdatePackageKind packageKind)
{
    const QString pkexec = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
    const bool rpm = packageKind == AppUpdatePackageKind::LinuxRpm;
    const QString aptGet = rpm ? QString() : QStandardPaths::findExecutable(QStringLiteral("apt-get"));
    if (pkexec.isEmpty() || (!rpm && aptGet.isEmpty())) {
        fail(rpm ? "pkexec é necessário para instalar atualizações no Linux."
                 : "pkexec e apt-get são necessários para instalar atualizações no Linux.",
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
        if (status != QProcess::NormalExit || exitCode != 0) {
            fail("O gerenciador de pacotes não instalou o pacote validado. "
                 "A versão atual continua em execução.", m_packagePath);
            return;
        }

        QFile::remove(m_packagePath);
        relaunchAfterInstall(QCoreApplication::applicationFilePath());
    });
    QObject::connect(process, &QProcess::errorOccurred, this,
                     [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || m_process != process) {
            return;
        }
        m_process = nullptr;
        process->deleteLater();
        fail("Não foi possível iniciar o pkexec para instalar o pacote validado.",
             m_packagePath);
    });

    // O pacote fica num diretório do usuário, que pode trocá-lo depois do
    // download. Como root, copiamos o arquivo para um diretório privado, o
    // verificamos de novo e instalamos a cópia verificada, nunca o original.
    static const char *const debScript =
        "set -eu; apt_get=$1; pkg=$2; sum=$3; "
        "dir=$(mktemp -d); trap 'rm -rf \"$dir\"' EXIT; chmod 755 \"$dir\"; "
        "cp -- \"$pkg\" \"$dir/package.deb\"; chmod 644 \"$dir/package.deb\"; "
        "echo \"$sum  $dir/package.deb\" | sha256sum -c - >/dev/null; "
        "\"$apt_get\" install --yes \"$dir/package.deb\"";
    static const char *const rpmScript =
        "set -eu; pkg=$1; sum=$2; "
        "dir=$(mktemp -d); trap 'rm -rf \"$dir\"' EXIT; chmod 755 \"$dir\"; "
        "cp -- \"$pkg\" \"$dir/package.rpm\"; chmod 644 \"$dir/package.rpm\"; "
        "echo \"$sum  $dir/package.rpm\" | sha256sum -c - >/dev/null; "
        "if command -v dnf >/dev/null 2>&1; then "
        "dnf install -y --nogpgcheck \"$dir/package.rpm\"; "
        "elif command -v zypper >/dev/null 2>&1; then "
        "zypper --non-interactive --no-gpg-checks install --allow-unsigned-rpm \"$dir/package.rpm\"; "
        "else rpm -U \"$dir/package.rpm\"; fi";
    QStringList arguments{QStringLiteral("/bin/sh"), QStringLiteral("-c"),
                          QString::fromLatin1(rpm ? rpmScript : debScript),
                          QStringLiteral("prism-update")};
    if (!rpm) {
        arguments << aptGet;
    }
    arguments << packagePath << expectedSha256.toLower();
    process->start(pkexec, arguments);
    emit statusMessage("Aguardando autorização para instalar a atualização v" + m_version + ".");
}

void AppUpdateInstaller::installAppImage(const QString &packagePath, const QString &expectedSha256)
{
    const QString target = qEnvironmentVariable("APPIMAGE");
    if (target.isEmpty() || !QFileInfo(target).isFile()) {
        fail("Não foi possível localizar o arquivo AppImage em execução.", packagePath);
        return;
    }
    const QString staged = target + QStringLiteral(".prism-update");
    QFile::remove(staged);
    if (!QFile::copy(packagePath, staged)) {
        fail("Sem permissão para gravar ao lado do AppImage atual; mova-o para uma pasta "
             "gravável ou baixe a nova versão manualmente.", packagePath);
        return;
    }
    // Confere a cópia que de fato será colocada no lugar do AppImage.
    if (!fileMatchesSha256(staged, expectedSha256)) {
        QFile::remove(staged);
        fail("A cópia do AppImage não confere com o SHA-256 esperado.", packagePath);
        return;
    }
    QFile::setPermissions(staged, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                      | QFileDevice::ExeOwner | QFileDevice::ReadGroup
                                      | QFileDevice::ExeGroup | QFileDevice::ReadOther
                                      | QFileDevice::ExeOther);
    // rename(2) substitui o arquivo de forma atômica, mesmo com o AppImage atual em uso.
    if (std::rename(QFile::encodeName(staged).constData(),
                    QFile::encodeName(target).constData()) != 0) {
        QFile::remove(staged);
        fail("Não foi possível substituir o AppImage atual.", packagePath);
        return;
    }
    QFile::remove(packagePath);
    relaunchAfterInstall(target);
}

void AppUpdateInstaller::installTarball(const QString &packagePath, const QString &expectedSha256)
{
    const QString tar = QStandardPaths::findExecutable(QStringLiteral("tar"));
    // O tar.gz extrai uma árvore bin/ lib/ share/ ...; o executável vive em bin/.
    const QString root = QDir::cleanPath(
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("..")));
    const QString relativeExecutable = QStringLiteral("bin/prism-downloader");
    if (tar.isEmpty() || !QFileInfo(QDir(root).filePath(relativeExecutable)).isFile()) {
        fail("Estrutura da instalação (tar.gz) não reconhecida ou tar indisponível.", packagePath);
        return;
    }
    const QFileInfo rootInfo(root);
    const QString parent = rootInfo.dir().absolutePath();
    if (!QFileInfo(parent).isWritable() || !rootInfo.isWritable()) {
        fail("Sem permissão para atualizar a pasta de instalação; baixe a nova versão manualmente.",
             packagePath);
        return;
    }
    if (!fileMatchesSha256(packagePath, expectedSha256)) {
        fail("O pacote mudou antes da instalação; atualização cancelada.", packagePath);
        return;
    }

    auto *staging = new QTemporaryDir(parent + QStringLiteral("/.prism-update-staging-XXXXXX"));
    if (!staging->isValid()) {
        delete staging;
        fail("Não foi possível criar a pasta temporária ao lado da instalação.", packagePath);
        return;
    }

    auto *process = new QProcess(this);
    m_process = process;
    QObject::connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                     this, [this, process, staging, root, parent, relativeExecutable](
                               int exitCode, QProcess::ExitStatus status) {
        std::unique_ptr<QTemporaryDir> stagingGuard(staging);
        if (m_process != process) {
            process->deleteLater();
            return;
        }
        m_process = nullptr;
        process->deleteLater();
        if (status != QProcess::NormalExit || exitCode != 0) {
            fail("Não foi possível extrair o pacote de atualização.", m_packagePath);
            return;
        }
        // O tar.gz tem uma única pasta de topo com a árvore da instalação.
        const QDir stagingDir(staging->path());
        const QFileInfoList tops = stagingDir.entryInfoList(
            QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
        if (tops.size() != 1
            || !QFileInfo(QDir(tops.first().absoluteFilePath()).filePath(relativeExecutable)).isFile()) {
            fail("O pacote baixado não tem a estrutura esperada.", m_packagePath);
            return;
        }
        const QString newRoot = tops.first().absoluteFilePath();
        const QString backup = parent + QStringLiteral("/.prism-update-backup-%1")
                                            .arg(QCoreApplication::applicationPid());
        QDir parentDir(parent);
        QDir(backup).removeRecursively();
        // Os renames abaixo são entre diretórios irmãos (mesmo sistema de arquivos).
        if (!QFile::rename(root, backup)) {
            fail("Não foi possível substituir a pasta de instalação.", m_packagePath);
            return;
        }
        if (!QFile::rename(newRoot, root)) {
            QFile::rename(backup, root);
            fail("Não foi possível colocar a nova versão no lugar; a anterior foi restaurada.",
                 m_packagePath);
            return;
        }
        // Devolve arquivos do usuário que não fazem parte do novo pacote.
        QDir backupDir(backup);
        const QDir rootDir(root);
        const QFileInfoList leftovers = backupDir.entryInfoList(
            QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        for (const QFileInfo &entry : leftovers) {
            if (!rootDir.exists(entry.fileName())) {
                backupDir.rename(entry.fileName(), rootDir.filePath(entry.fileName()));
            }
        }
        QDir(backup).removeRecursively();
        QFile::remove(m_packagePath);
        relaunchAfterInstall(QDir(root).filePath(relativeExecutable));
    });
    QObject::connect(process, &QProcess::errorOccurred, this,
                     [this, process, staging](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || m_process != process) {
            return;
        }
        m_process = nullptr;
        process->deleteLater();
        delete staging;
        fail("Não foi possível iniciar o tar para extrair a atualização.", m_packagePath);
    });
    process->start(tar, {QStringLiteral("-xzf"), packagePath,
                         QStringLiteral("-C"), staging->path()});
    emit statusMessage("Extraindo a atualização v" + m_version + "...");
}

void AppUpdateInstaller::relaunchAfterInstall(const QString &executable)
{
    setInstalling(false);
    if (!QProcess::startDetached(executable)) {
        emit failed("A atualização foi instalada, mas reinicie o Prism Downloader manualmente.");
        return;
    }
    emit logMessage("[Updater] Pacote Linux v" + m_version + " instalado; reiniciando o aplicativo.");
    emit restartRequested();
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
