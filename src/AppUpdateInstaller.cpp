#include "AppUpdateInstaller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QUuid>

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
                                 bool installedWindowsCopy)
{
    if (isInstalling()) {
        return;
    }

    m_version = version;
    m_packagePath = packagePath;
    setInstalling(true);

#ifdef Q_OS_WIN
    if (installedWindowsCopy) {
        const bool started = QProcess::startDetached(packagePath, {
            QStringLiteral("/VERYSILENT"), QStringLiteral("/SUPPRESSMSGBOXES"),
            QStringLiteral("/NORESTART"), QStringLiteral("/CLOSEAPPLICATIONS")});
        if (!started) {
            fail("Não foi possível iniciar o instalador validado.", packagePath);
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
    if (!QFileInfo(helperSource).isFile() || !QDir().mkpath(helperDirectory)
        || !QFile::copy(helperSource, helperCopy)) {
        fail("O helper obrigatório para atualizar a cópia Portable não está disponível.",
             packagePath);
        return;
    }

    const bool started = QProcess::startDetached(helperCopy, {
        QStringLiteral("--parent-pid"), QString::number(QCoreApplication::applicationPid()),
        QStringLiteral("--archive"), packagePath,
        QStringLiteral("--target"), QCoreApplication::applicationDirPath()});
    if (!started) {
        QFile::remove(helperCopy);
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
    process->start(pkexec, {
        aptGet, QStringLiteral("install"), QStringLiteral("--yes"), packagePath});
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
    if (!packagePath.isEmpty()) {
        QFile::remove(packagePath);
    }
    setInstalling(false);
    emit failed(message);
}
