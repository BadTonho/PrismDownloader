#include "MainWindowUpdateCoordinator.h"

#include "AppUpdateInstaller.h"
#include "AppUpdateService.h"
#include "MainWindow.h"
#include "MediaToolResolver.h"
#include "YtDlpStatus.h"
#include "YtDlpUpdateService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QProgressBar>
#include <QVersionNumber>

#include <utility>

MainWindowUpdateCoordinator::MainWindowUpdateCoordinator(MainWindow *window,
                                                         QString versionTag,
                                                         QString versionNumber)
    : m_window(window),
      m_versionTag(std::move(versionTag)),
      m_versionNumber(std::move(versionNumber))
{
    m_appUpdateInstaller = new AppUpdateInstaller(m_window);
    QObject::connect(m_appUpdateInstaller, &AppUpdateInstaller::statusMessage, m_window,
            [this](const QString &message) {
        if (m_window->m_updateStatusLabel) {
            m_window->m_updateStatusLabel->setText(message);
        }
    });
    QObject::connect(m_appUpdateInstaller, &AppUpdateInstaller::logMessage,
            m_window, &MainWindow::logMessage);
    QObject::connect(m_appUpdateInstaller, &AppUpdateInstaller::failed, m_window,
            [this](const QString &message) {
        if (m_window->m_updateAppBtn) {
            m_window->m_updateAppBtn->setEnabled(true);
        }
        if (m_window->m_updateStatusLabel) {
            m_window->m_updateStatusLabel->setText("Falha ao instalar a atualização: " + message);
        }
        m_window->logMessage("[Updater] " + message);
        QMessageBox::warning(m_window, "Atualização preservada", message);
    });
    QObject::connect(m_appUpdateInstaller, &AppUpdateInstaller::restartRequested, m_window,
            [this]() {
        m_window->m_closing = true;
        QCoreApplication::quit();
    });

    m_appUpdateService = new AppUpdateService(
        AppUpdateService::packageForCurrentPlatform(isInstalledWindowsCopy()), m_window);
    QObject::connect(m_appUpdateService, &AppUpdateService::releaseChecked, m_window,
            [this](const AppUpdateReleaseInfo &release) {
        if (m_window->m_checkUpdateBtn) {
            m_window->m_checkUpdateBtn->setEnabled(true);
            m_window->m_checkUpdateBtn->setText("VERIFICAR NO GITHUB AGORA");
        }
        const QVersionNumber local = QVersionNumber::fromString(m_versionNumber);
        const QVersionNumber remote = QVersionNumber::fromString(release.version);
        const int comparison = QVersionNumber::compare(remote, local);
        if (comparison <= 0) {
            if (m_window->m_updateAppBtn) {
                m_window->m_updateAppBtn->setVisible(false);
                m_window->m_updateAppBtn->setEnabled(false);
            }
            if (m_window->m_updateStatusLabel) {
                m_window->m_updateStatusLabel->setText(comparison == 0
                    ? QString("Versão %1 está atualizada e validada por assinatura.").arg(m_versionTag)
                    : QString("Release %1 é mais antiga e foi ignorada.").arg(release.version));
            }
            if (m_window->m_sidebarUpdateNotification) {
                m_window->m_sidebarUpdateNotification->setText(m_versionTag + " (Em Dia)");
                m_window->m_sidebarUpdateNotification->setStyleSheet("color: #10b981; font-size: 12px; font-weight: bold; margin-bottom: 2px;");
            }
            if (!m_appUpdateCheckSilent) {
                QMessageBox::information(m_window, "Atualizações",
                                          m_window->m_updateStatusLabel->text());
            }
            return;
        }

        if (m_window->m_updateStatusLabel) {
            m_window->m_updateStatusLabel->setText(
                QString("Nova versão v%1 disponível e autenticada.").arg(release.version));
            m_window->m_updateStatusLabel->setStyleSheet(
                "color: #10b981; font-weight: bold; font-size: 14px;");
        }
        if (m_window->m_sidebarUpdateNotification) {
            m_window->m_sidebarUpdateNotification->setText(
                QString("Nova Versão v%1!").arg(release.version));
            m_window->m_sidebarUpdateNotification->setStyleSheet(
                "color: #f59e0b; background-color: #2a1f0c; border: 1px solid #f59e0b; "
                "border-radius: 4px; padding: 4px; font-size: 12px; font-weight: bold; "
                "margin: 0 10px 2px 10px;");
        }
        if (m_window->m_updateAppBtn) {
            m_window->m_updateAppBtn->setVisible(true);
            m_window->m_updateAppBtn->setEnabled(true);
            m_window->m_updateAppBtn->setText(
                QString("BAIXAR E ATUALIZAR PARA v%1").arg(release.version));
        }
        m_window->logMessage(QString("[Updater] Release v%1 autenticada; pacote %2 selecionado.")
                                 .arg(release.version, release.assetName));
        if (m_window->m_autoDownloadUpdatesChk
            && m_window->m_autoDownloadUpdatesChk->isChecked()) {
            m_appUpdatePending = true;
            tryStartPendingAppUpdate();
        }
    });
    QObject::connect(m_appUpdateService, &AppUpdateService::checkFailed, m_window,
            [this](const QString &message) {
        if (m_window->m_checkUpdateBtn) {
            m_window->m_checkUpdateBtn->setEnabled(true);
            m_window->m_checkUpdateBtn->setText("VERIFICAR NO GITHUB AGORA");
        }
        if (m_window->m_updateStatusLabel) {
            m_window->m_updateStatusLabel->setText(
                "Atualização do aplicativo indisponível: " + message);
        }
        if (m_window->m_sidebarUpdateNotification) {
            m_window->m_sidebarUpdateNotification->setText(
                m_versionTag + " (Não validado)");
            m_window->m_sidebarUpdateNotification->setStyleSheet(
                "color: #737373; font-size: 12px; font-weight: bold; margin-bottom: 2px;");
        }
        if (m_window->m_updateAppBtn) {
            m_window->m_updateAppBtn->setVisible(false);
            m_window->m_updateAppBtn->setEnabled(false);
        }
        m_window->logMessage("[Updater] " + message);
        if (!m_appUpdateCheckSilent) {
            QMessageBox::warning(m_window, "Atualizações", message);
        }
    });
    QObject::connect(m_appUpdateService, &AppUpdateService::downloadProgress, m_window,
            [this](qint64 received, qint64 total) {
        if (!m_window->m_updateProgressBar) return;
        m_window->m_updateProgressBar->setVisible(true);
        if (total > 0) {
            m_window->m_updateProgressBar->setRange(0, 100);
            m_window->m_updateProgressBar->setValue(static_cast<int>((received * 100) / total));
        } else {
            m_window->m_updateProgressBar->setRange(0, 0);
        }
    });
    QObject::connect(m_appUpdateService, &AppUpdateService::packageVerified, m_window,
            [this](const QString &version, const QString &path) {
        if (m_window->m_updateProgressBar) {
            m_window->m_updateProgressBar->setVisible(false);
            m_window->m_updateProgressBar->setRange(0, 100);
        }
        installVerifiedAppPackage(version, path);
    });
    QObject::connect(m_appUpdateService, &AppUpdateService::updateFailed, m_window,
            [this](const QString &message) {
        if (m_window->m_updateProgressBar) {
            m_window->m_updateProgressBar->setVisible(false);
            m_window->m_updateProgressBar->setRange(0, 100);
        }
        if (m_window->m_updateAppBtn) {
            m_window->m_updateAppBtn->setEnabled(true);
        }
        if (m_window->m_updateStatusLabel) {
            m_window->m_updateStatusLabel->setText(
                "Falha ao validar a atualização: " + message);
        }
        m_window->logMessage("[Updater] " + message);
        QMessageBox::warning(m_window, "Atualização preservada", message);
    });

    m_ytdlpUpdateService = new YtDlpUpdateService(m_window);
    if (m_window->m_ytdlpStatusLabel) {
        m_window->m_ytdlpStatusLabel->setText(YtDlpStatus::description());
    }
    QObject::connect(m_ytdlpUpdateService, &YtDlpUpdateService::releaseChecked, m_window,
            [this](const YtDlpReleaseInfo &release) {
        const MediaToolInfo current = MediaToolResolver::resolveInfo(MediaTool::YtDlp);
        const bool updateAvailable = !current.isAvailable()
            || MediaToolResolver::isVersionNewer(release.version, current.version);
        if (m_window->m_ytdlpStatusLabel) {
            m_window->m_ytdlpStatusLabel->setText(updateAvailable
                ? QString("%1 — Nightly %2 disponível.")
                      .arg(YtDlpStatus::description(), release.version)
                : QString("%1 — Nightly %2 já está em uso.")
                      .arg(YtDlpStatus::description(), release.version));
        }
        if (m_window->m_updateYtdlpBtn) {
            m_window->m_updateYtdlpBtn->setEnabled(true);
            m_window->m_updateYtdlpBtn->setText(updateAvailable
                ? QStringLiteral("ATUALIZAR YT-DLP NIGHTLY AGORA")
                : QStringLiteral("VERIFICAR YT-DLP NIGHTLY"));
        }
        m_window->logMessage(updateAvailable
            ? QString("[Motor Extrator] Nightly %1 disponível; atualização aguarda confirmação.")
                  .arg(release.version)
            : QString("[Motor Extrator] yt-dlp já está atualizado na Nightly %1.")
                  .arg(release.version));
        if (!m_ytdlpCheckSilent && !updateAvailable) {
            QMessageBox::information(m_window, "yt-dlp",
                                      m_window->m_ytdlpStatusLabel->text());
        }
    });
    QObject::connect(m_ytdlpUpdateService, &YtDlpUpdateService::checkFailed, m_window,
            [this](const QString &message) {
        if (m_window->m_ytdlpStatusLabel) {
            m_window->m_ytdlpStatusLabel->setText(YtDlpStatus::description() + " — " + message);
        }
        if (m_window->m_updateYtdlpBtn) {
            m_window->m_updateYtdlpBtn->setEnabled(true);
            m_window->m_updateYtdlpBtn->setText("VERIFICAR YT-DLP NIGHTLY");
        }
        m_window->logMessage("[Motor Extrator] " + message);
        if (!m_ytdlpCheckSilent) {
            QMessageBox::warning(m_window, "yt-dlp", message);
        }
    });
    QObject::connect(m_ytdlpUpdateService, &YtDlpUpdateService::updateProgress, m_window,
            [this](qint64 received, qint64 total) {
        if (!m_window->m_updateProgressBar) return;
        m_window->m_updateProgressBar->setVisible(true);
        if (total > 0) {
            m_window->m_updateProgressBar->setRange(0, 100);
            m_window->m_updateProgressBar->setValue(static_cast<int>((received * 100) / total));
        } else {
            m_window->m_updateProgressBar->setRange(0, 0);
        }
    });
    QObject::connect(m_ytdlpUpdateService, &YtDlpUpdateService::updateCompleted, m_window,
            [this](const QString &version, const QString &path) {
        if (m_window->m_updateProgressBar) {
            m_window->m_updateProgressBar->setVisible(false);
            m_window->m_updateProgressBar->setRange(0, 100);
        }
        if (m_window->m_ytdlpStatusLabel) {
            m_window->m_ytdlpStatusLabel->setText(
                YtDlpStatus::description() + QString(" — Nightly %1 instalada.").arg(version));
        }
        if (m_window->m_updateYtdlpBtn) {
            m_window->m_updateYtdlpBtn->setEnabled(true);
            m_window->m_updateYtdlpBtn->setText("VERIFICAR YT-DLP NIGHTLY");
        }
        m_window->logMessage(QString("[Motor Extrator] yt-dlp Nightly %1 atualizado em %2.")
                                 .arg(version, QDir::toNativeSeparators(path)));
        QMessageBox::information(
            m_window, "yt-dlp atualizado",
            "O yt-dlp Nightly foi atualizado e será usado nas próximas tarefas.");
    });
    QObject::connect(m_ytdlpUpdateService, &YtDlpUpdateService::updateFailed, m_window,
            [this](const QString &message) {
        if (m_window->m_updateProgressBar) {
            m_window->m_updateProgressBar->setVisible(false);
            m_window->m_updateProgressBar->setRange(0, 100);
        }
        if (m_window->m_ytdlpStatusLabel) {
            m_window->m_ytdlpStatusLabel->setText(YtDlpStatus::description() + " — " + message);
        }
        if (m_window->m_updateYtdlpBtn) {
            m_window->m_updateYtdlpBtn->setEnabled(true);
            m_window->m_updateYtdlpBtn->setText("VERIFICAR YT-DLP NIGHTLY");
        }
        m_window->logMessage("[Motor Extrator] " + message);
        QMessageBox::warning(m_window, "Falha ao atualizar yt-dlp", message);
    });
}

void MainWindowUpdateCoordinator::checkForUpdates(bool silent)
{
    if (!m_appUpdateService || m_appUpdateService->isBusy()) {
        return;
    }
    m_appUpdateCheckSilent = silent;
    m_appUpdatePending = false;
    if (m_window->m_updateAppBtn) {
        m_window->m_updateAppBtn->setVisible(false);
        m_window->m_updateAppBtn->setEnabled(false);
    }
    if (m_window->m_updateStatusLabel) {
        m_window->m_updateStatusLabel->setText("Consultando a release assinada no GitHub...");
    }
    if (m_window->m_sidebarUpdateNotification) {
        m_window->m_sidebarUpdateNotification->setText("🔄 Checando...");
        m_window->m_sidebarUpdateNotification->setStyleSheet(
            "color: #38bdf8; font-size: 12px; font-weight: bold; margin-bottom: 2px;");
    }
    if (m_window->m_checkUpdateBtn) {
        m_window->m_checkUpdateBtn->setEnabled(false);
        m_window->m_checkUpdateBtn->setText("VERIFICANDO RELEASE...");
    }
    m_window->logMessage("[Updater] Consultando release e manifesto assinado no GitHub.");
    m_appUpdateService->checkLatestRelease();
}

void MainWindowUpdateCoordinator::requestAppUpdate()
{
    if (!m_appUpdateService || !m_appUpdateService->hasLatestRelease()
        || isInstallingAppUpdate()) {
        return;
    }
    m_appUpdatePending = true;
    if (m_window->m_updateAppBtn) {
        m_window->m_updateAppBtn->setEnabled(false);
        m_window->m_updateAppBtn->setText("ATUALIZAÇÃO AGENDADA...");
    }
    tryStartPendingAppUpdate();
}

bool MainWindowUpdateCoordinator::canInstallAppUpdate() const
{
    return !m_window->m_closing
        && !isInstallingAppUpdate()
        && !m_window->m_downloadManager->hasWork()
        && !m_window->m_conversionManager->hasWork()
        && (!m_window->m_playlistPreviewService
            || !m_window->m_playlistPreviewService->isBusy());
}

void MainWindowUpdateCoordinator::tryStartPendingAppUpdate()
{
    if (!m_appUpdatePending || !m_appUpdateService || !m_appUpdateService->hasLatestRelease()
        || m_appUpdateService->isBusy() || isInstallingAppUpdate()) {
        return;
    }
    if (!canInstallAppUpdate()) {
        if (m_window->m_updateStatusLabel) {
            m_window->m_updateStatusLabel->setText(
                "Atualização autenticada aguardando downloads, conversões ou prévia terminarem.");
        }
        return;
    }

    m_appUpdatePending = false;
    if (m_window->m_updateStatusLabel) {
        m_window->m_updateStatusLabel->setText(
            "Baixando pacote autenticado; validando SHA-256...");
    }
    if (m_window->m_updateAppBtn) {
        m_window->m_updateAppBtn->setEnabled(false);
        m_window->m_updateAppBtn->setText("BAIXANDO ATUALIZAÇÃO...");
    }
    m_window->logMessage(
        "[Updater] A fila está ociosa; iniciando download do pacote autenticado.");
    m_appUpdateService->downloadLatestRelease();
}

bool MainWindowUpdateCoordinator::isInstallingAppUpdate() const
{
    return m_appUpdateInstaller && m_appUpdateInstaller->isInstalling();
}

bool MainWindowUpdateCoordinator::isInstalledWindowsCopy() const
{
#ifdef Q_OS_WIN
    return QFileInfo(QDir(QCoreApplication::applicationDirPath())
                         .filePath("unins000.exe"))
        .isFile();
#else
    return false;
#endif
}

void MainWindowUpdateCoordinator::installVerifiedAppPackage(const QString &version,
                                                            const QString &packagePath)
{
    if (!canInstallAppUpdate()) {
        m_appUpdatePending = true;
        if (m_window->m_updateStatusLabel) {
            m_window->m_updateStatusLabel->setText(
                "Pacote v" + version
                + " validado; instalação aguardando a fila ficar ociosa.");
        }
        return;
    }
    if (!m_appUpdateInstaller) {
        QMessageBox::critical(m_window, "Atualização indisponível",
                              "O instalador de atualizações não está disponível.");
        QFile::remove(packagePath);
        return;
    }

    m_appUpdateInstaller->install(version, packagePath, isInstalledWindowsCopy());
}

void MainWindowUpdateCoordinator::checkYtDlpUpdates(bool silent)
{
    if (!m_ytdlpUpdateService || m_ytdlpUpdateService->isBusy()) {
        return;
    }
    m_ytdlpCheckSilent = silent;
    if (m_window->m_ytdlpStatusLabel) {
        m_window->m_ytdlpStatusLabel->setText(
            YtDlpStatus::description() + " — consultando a Nightly oficial...");
    }
    if (m_window->m_updateYtdlpBtn) {
        m_window->m_updateYtdlpBtn->setEnabled(false);
        m_window->m_updateYtdlpBtn->setText("VERIFICANDO YT-DLP...");
    }
    m_window->logMessage(
        "[Motor Extrator] Consultando a release Nightly oficial do yt-dlp...");
    m_ytdlpUpdateService->checkLatestRelease();
}

void MainWindowUpdateCoordinator::updateYtdlpEngine()
{
    if (!m_ytdlpUpdateService || m_ytdlpUpdateService->isBusy()) {
        return;
    }
    if (!m_ytdlpUpdateService->hasLatestRelease()) {
        checkYtDlpUpdates(false);
        return;
    }

    const MediaToolInfo current = MediaToolResolver::resolveInfo(MediaTool::YtDlp);
    if (current.isAvailable()
        && !MediaToolResolver::isVersionNewer(m_ytdlpUpdateService->latestVersion(),
                                              current.version)) {
        QMessageBox::information(
            m_window, "yt-dlp",
            YtDlpStatus::description()
                + " já é igual ou mais recente que a Nightly publicada.");
        return;
    }
    if (m_window->m_downloadManager->hasWork()
        || m_window->m_conversionManager->hasWork()
        || (m_window->m_playlistPreviewService
            && m_window->m_playlistPreviewService->isBusy())) {
        QMessageBox::warning(
            m_window, "Atualização adiada",
            "Conclua ou cancele downloads, conversões e prévias antes de atualizar o yt-dlp.");
        return;
    }

    const QMessageBox::StandardButton response = QMessageBox::question(
        m_window,
        "Atualizar yt-dlp Nightly",
        QString("A versão Nightly %1 será baixada da release oficial, validada por SHA-256 "
                "e salva apenas na pasta de dados do seu usuário.\n\nDeseja continuar?")
            .arg(m_ytdlpUpdateService->latestVersion()),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Yes);
    if (response != QMessageBox::Yes) {
        return;
    }

    if (m_window->m_updateYtdlpBtn) {
        m_window->m_updateYtdlpBtn->setEnabled(false);
        m_window->m_updateYtdlpBtn->setText("ATUALIZANDO YT-DLP...");
    }
    if (m_window->m_updateProgressBar) {
        m_window->m_updateProgressBar->setVisible(true);
        m_window->m_updateProgressBar->setRange(0, 0);
    }
    m_window->logMessage(
        "[Motor Extrator] Atualização Nightly confirmada pelo usuário; baixando checksum e binário.");
    m_ytdlpUpdateService->installLatestRelease();
}
