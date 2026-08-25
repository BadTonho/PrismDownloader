#include "MainWindow.h"
#include "AppUpdateService.h"
#include "MediaToolResolver.h"
#include "PrismVersion.h"
#include "YtDlpUpdateService.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QColor>
#include <QGridLayout>
#include <QGroupBox>
#include <QWidget>
#include <QMetaObject>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QListWidget>
#include <QListWidgetItem>
#include <QProgressDialog>
#include <QPixmap>
#include <QTableWidgetItem>
#include <QThread>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QCloseEvent>
#include <QRegularExpression>
#include <QPointer>
#include <QUrlQuery>
#include <QVersionNumber>
#include <QUuid>

#include <memory>
#include <utility>

static const QString NEOV_VERSION_TAG = QStringLiteral(PRISM_VERSION_TAG);
static const QString NEOV_VERSION_NUMBER = QStringLiteral(PRISM_VERSION_NUMBER);

namespace {
constexpr int kMaximumLogEntries = 5000;
constexpr int kMaximumPlaylistItems = 500;
constexpr qsizetype kMaximumPlaylistOutputBytes = 4 * 1024 * 1024;
constexpr qsizetype kMaximumPlaylistErrorBytes = 128 * 1024;
constexpr qsizetype kMaximumLibraryThumbnailBytes = 3 * 1024 * 1024;

qint64 toSeconds(const QRegularExpressionMatch &match, int hourIndex, int minuteIndex, int secondIndex)
{
    return match.captured(hourIndex).toLongLong() * 3600
        + match.captured(minuteIndex).toLongLong() * 60
        + match.captured(secondIndex).toLongLong();
}

bool isValidTimeRange(const QString &timeRange)
{
    if (timeRange.isEmpty()) {
        return true;
    }

    static const QRegularExpression pattern(
        "^(\\d{1,3}):([0-5]\\d):([0-5]\\d)-(\\d{1,3}):([0-5]\\d):([0-5]\\d)$");
    const QRegularExpressionMatch match = pattern.match(timeRange);
    return match.hasMatch() && toSeconds(match, 1, 2, 3) < toSeconds(match, 4, 5, 6);
}

QList<PlaylistItem> parsePlaylistPreview(const QByteArray &output)
{
    QList<PlaylistItem> items;
    QSet<QString> seenUrls;
    const QStringList lines = QString::fromUtf8(output).split('\n');
    for (const QString &rawLine : lines) {
        if (items.size() >= kMaximumPlaylistItems) {
            break;
        }
        const QString line = rawLine.trimmed();
        const QStringList fields = line.split('\t');
        if (fields.size() < 2) {
            continue;
        }

        PlaylistItem item;
        item.title = fields.at(0).trimmed();
        item.url = QUrl(fields.at(1).trimmed());
        item.duration = fields.value(2).trimmed();
        item.thumbnailUrl = QUrl(fields.value(3).trimmed());
        if (item.title.isEmpty() || !item.url.isValid() || item.url.host().isEmpty()
            || (item.url.scheme() != "http" && item.url.scheme() != "https")) {
            continue;
        }

        const QString normalizedUrl = item.url.adjusted(QUrl::RemoveFragment).toString(QUrl::FullyEncoded);
        if (!seenUrls.contains(normalizedUrl)) {
            seenUrls.insert(normalizedUrl);
            items.append(item);
        }
    }
    return items;
}

QString ytdlpCurrentDescription()
{
    const MediaToolInfo info = MediaToolResolver::resolveInfo(MediaTool::YtDlp);
    if (!info.isAvailable()) {
        return QStringLiteral("yt-dlp: nenhuma cópia funcional foi localizada.");
    }
    return QStringLiteral("yt-dlp em uso: %1 (%2)")
        .arg(info.version, MediaToolResolver::sourceLabel(info.source));
}

}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_downloadManager(new DownloadManager(this)),
      m_conversionManager(new ConversionManager(this))
{
    setWindowTitle("Prism Downloader - Studio Suite");
    resize(980, 620);

    setupUI();
    setupStyles();

    m_logFlushTimer = new QTimer(this);
    m_logFlushTimer->setInterval(75);
    connect(m_logFlushTimer, &QTimer::timeout, this, &MainWindow::flushLogBuffer);

    m_thumbnailNetwork = new QNetworkAccessManager(this);
    auto *thumbnailCache = new QNetworkDiskCache(m_thumbnailNetwork);
    QString thumbnailCachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (!thumbnailCachePath.isEmpty()) {
        thumbnailCachePath = QDir(thumbnailCachePath).filePath(QStringLiteral("thumbnails"));
        if (QDir().mkpath(thumbnailCachePath)) {
            thumbnailCache->setCacheDirectory(thumbnailCachePath);
            thumbnailCache->setMaximumCacheSize(32LL * 1024 * 1024);
            m_thumbnailNetwork->setCache(thumbnailCache);
        }
    }

    m_appUpdateService = new AppUpdateService(
        AppUpdateService::packageForCurrentPlatform(isInstalledWindowsCopy()), this);
    connect(m_appUpdateService, &AppUpdateService::releaseChecked, this,
            [this](const AppUpdateReleaseInfo &release) {
        if (m_checkUpdateBtn) {
            m_checkUpdateBtn->setEnabled(true);
            m_checkUpdateBtn->setText("VERIFICAR NO GITHUB AGORA");
        }
        const QVersionNumber local = QVersionNumber::fromString(NEOV_VERSION_NUMBER);
        const QVersionNumber remote = QVersionNumber::fromString(release.version);
        const int comparison = QVersionNumber::compare(remote, local);
        if (comparison <= 0) {
            if (m_updateAppBtn) {
                m_updateAppBtn->setVisible(false);
                m_updateAppBtn->setEnabled(false);
            }
            if (m_updateStatusLabel) {
                m_updateStatusLabel->setText(comparison == 0
                    ? QString("Versão %1 está atualizada e validada por assinatura.").arg(NEOV_VERSION_TAG)
                    : QString("Release %1 é mais antiga e foi ignorada.").arg(release.version));
            }
            if (m_sidebarUpdateNotification) {
                m_sidebarUpdateNotification->setText("✅ " + NEOV_VERSION_TAG + " (Em Dia)");
                m_sidebarUpdateNotification->setStyleSheet("color: #10b981; font-size: 12px; font-weight: bold; margin-bottom: 2px;");
            }
            if (!m_appUpdateCheckSilent) {
                QMessageBox::information(this, "Atualizações", m_updateStatusLabel->text());
            }
            return;
        }

        if (m_updateStatusLabel) {
            m_updateStatusLabel->setText(QString("🚀 Nova versão v%1 disponível e autenticada.").arg(release.version));
            m_updateStatusLabel->setStyleSheet("color: #10b981; font-weight: bold; font-size: 14px;");
        }
        if (m_sidebarUpdateNotification) {
            m_sidebarUpdateNotification->setText(QString("🔔 Nova Versão v%1!").arg(release.version));
            m_sidebarUpdateNotification->setStyleSheet("color: #f59e0b; background-color: #2a1f0c; border: 1px solid #f59e0b; border-radius: 4px; padding: 4px; font-size: 12px; font-weight: bold; margin: 0 10px 2px 10px;");
        }
        if (m_updateAppBtn) {
            m_updateAppBtn->setVisible(true);
            m_updateAppBtn->setEnabled(true);
            m_updateAppBtn->setText(QString("BAIXAR E ATUALIZAR PARA v%1").arg(release.version));
        }
        logMessage(QString("[Updater] Release v%1 autenticada; pacote %2 selecionado.")
                       .arg(release.version, release.assetName));
        if (m_autoDownloadUpdatesChk && m_autoDownloadUpdatesChk->isChecked()) {
            m_appUpdatePending = true;
            tryStartPendingAppUpdate();
        }
    });
    connect(m_appUpdateService, &AppUpdateService::checkFailed, this,
            [this](const QString &message) {
        if (m_checkUpdateBtn) {
            m_checkUpdateBtn->setEnabled(true);
            m_checkUpdateBtn->setText("VERIFICAR NO GITHUB AGORA");
        }
        if (m_updateStatusLabel) {
            m_updateStatusLabel->setText("Atualização do aplicativo indisponível: " + message);
        }
        if (m_sidebarUpdateNotification) {
            m_sidebarUpdateNotification->setText("🛡️ " + NEOV_VERSION_TAG + " (Não validado)");
            m_sidebarUpdateNotification->setStyleSheet("color: #737373; font-size: 12px; font-weight: bold; margin-bottom: 2px;");
        }
        if (m_updateAppBtn) {
            m_updateAppBtn->setVisible(false);
            m_updateAppBtn->setEnabled(false);
        }
        logMessage("[Updater] " + message);
        if (!m_appUpdateCheckSilent) {
            QMessageBox::warning(this, "Atualizações", message);
        }
    });
    connect(m_appUpdateService, &AppUpdateService::downloadProgress, this,
            [this](qint64 received, qint64 total) {
        if (!m_updateProgressBar) return;
        m_updateProgressBar->setVisible(true);
        if (total > 0) {
            m_updateProgressBar->setRange(0, 100);
            m_updateProgressBar->setValue(static_cast<int>((received * 100) / total));
        } else {
            m_updateProgressBar->setRange(0, 0);
        }
    });
    connect(m_appUpdateService, &AppUpdateService::packageVerified, this,
            [this](const QString &version, const QString &path) {
        if (m_updateProgressBar) {
            m_updateProgressBar->setVisible(false);
            m_updateProgressBar->setRange(0, 100);
        }
        installVerifiedAppPackage(version, path);
    });
    connect(m_appUpdateService, &AppUpdateService::updateFailed, this,
            [this](const QString &message) {
        if (m_updateProgressBar) {
            m_updateProgressBar->setVisible(false);
            m_updateProgressBar->setRange(0, 100);
        }
        if (m_updateAppBtn) {
            m_updateAppBtn->setEnabled(true);
        }
        if (m_updateStatusLabel) {
            m_updateStatusLabel->setText("Falha ao validar a atualização: " + message);
        }
        logMessage("[Updater] " + message);
        QMessageBox::warning(this, "Atualização preservada", message);
    });

    m_ytdlpUpdateService = new YtDlpUpdateService(this);
    if (m_ytdlpStatusLabel) {
        m_ytdlpStatusLabel->setText(ytdlpCurrentDescription());
    }
    connect(m_ytdlpUpdateService, &YtDlpUpdateService::releaseChecked, this,
            [this](const YtDlpReleaseInfo &release) {
        const MediaToolInfo current = MediaToolResolver::resolveInfo(MediaTool::YtDlp);
        const bool updateAvailable = !current.isAvailable()
            || MediaToolResolver::isVersionNewer(release.version, current.version);
        if (m_ytdlpStatusLabel) {
            m_ytdlpStatusLabel->setText(updateAvailable
                ? QString("%1 — Nightly %2 disponível.")
                      .arg(ytdlpCurrentDescription(), release.version)
                : QString("%1 — Nightly %2 já está em uso.")
                      .arg(ytdlpCurrentDescription(), release.version));
        }
        if (m_updateYtdlpBtn) {
            m_updateYtdlpBtn->setEnabled(true);
            m_updateYtdlpBtn->setText(updateAvailable
                ? QStringLiteral("ATUALIZAR YT-DLP NIGHTLY AGORA")
                : QStringLiteral("VERIFICAR YT-DLP NIGHTLY"));
        }
        logMessage(updateAvailable
            ? QString("[Motor Extrator] Nightly %1 disponível; atualização aguarda confirmação.")
                  .arg(release.version)
            : QString("[Motor Extrator] yt-dlp já está atualizado na Nightly %1.")
                  .arg(release.version));
        if (!m_ytdlpCheckSilent && !updateAvailable) {
            QMessageBox::information(this, "yt-dlp", m_ytdlpStatusLabel->text());
        }
    });
    connect(m_ytdlpUpdateService, &YtDlpUpdateService::checkFailed, this,
            [this](const QString &message) {
        if (m_ytdlpStatusLabel) {
            m_ytdlpStatusLabel->setText(ytdlpCurrentDescription() + " — " + message);
        }
        if (m_updateYtdlpBtn) {
            m_updateYtdlpBtn->setEnabled(true);
            m_updateYtdlpBtn->setText("VERIFICAR YT-DLP NIGHTLY");
        }
        logMessage("[Motor Extrator] " + message);
        if (!m_ytdlpCheckSilent) {
            QMessageBox::warning(this, "yt-dlp", message);
        }
    });
    connect(m_ytdlpUpdateService, &YtDlpUpdateService::updateProgress, this,
            [this](qint64 received, qint64 total) {
        if (!m_updateProgressBar) return;
        m_updateProgressBar->setVisible(true);
        if (total > 0) {
            m_updateProgressBar->setRange(0, 100);
            m_updateProgressBar->setValue(static_cast<int>((received * 100) / total));
        } else {
            m_updateProgressBar->setRange(0, 0);
        }
    });
    connect(m_ytdlpUpdateService, &YtDlpUpdateService::updateCompleted, this,
            [this](const QString &version, const QString &path) {
        if (m_updateProgressBar) {
            m_updateProgressBar->setVisible(false);
            m_updateProgressBar->setRange(0, 100);
        }
        if (m_ytdlpStatusLabel) {
            m_ytdlpStatusLabel->setText(ytdlpCurrentDescription()
                                        + QString(" — Nightly %1 instalada.").arg(version));
        }
        if (m_updateYtdlpBtn) {
            m_updateYtdlpBtn->setEnabled(true);
            m_updateYtdlpBtn->setText("VERIFICAR YT-DLP NIGHTLY");
        }
        logMessage(QString("[Motor Extrator] yt-dlp Nightly %1 atualizado em %2.")
                       .arg(version, QDir::toNativeSeparators(path)));
        QMessageBox::information(this, "yt-dlp atualizado",
                                 "O yt-dlp Nightly foi atualizado e será usado nas próximas tarefas.");
    });
    connect(m_ytdlpUpdateService, &YtDlpUpdateService::updateFailed, this,
            [this](const QString &message) {
        if (m_updateProgressBar) {
            m_updateProgressBar->setVisible(false);
            m_updateProgressBar->setRange(0, 100);
        }
        if (m_ytdlpStatusLabel) {
            m_ytdlpStatusLabel->setText(ytdlpCurrentDescription() + " — " + message);
        }
        if (m_updateYtdlpBtn) {
            m_updateYtdlpBtn->setEnabled(true);
            m_updateYtdlpBtn->setText("VERIFICAR YT-DLP NIGHTLY");
        }
        logMessage("[Motor Extrator] " + message);
        QMessageBox::warning(this, "Falha ao atualizar yt-dlp", message);
    });

    startGpuProbe();

    connect(m_downloadManager, &DownloadManager::jobProgress, this, &MainWindow::onDownloadProgress);
    connect(m_downloadManager, &DownloadManager::jobStatus, this, &MainWindow::onDownloadStatus);
    connect(m_downloadManager, &DownloadManager::jobCompleted, this, &MainWindow::onDownloadCompleted);
    connect(m_downloadManager, &DownloadManager::queueStateChanged, this, &MainWindow::onDownloadQueueStateChanged);
    connect(m_downloadManager, &DownloadManager::queueIdle, this, &MainWindow::maybeShowQueueSummary);
    connect(m_downloadManager, &DownloadManager::queueIdle, this, &MainWindow::tryStartPendingAppUpdate);
    connect(m_downloadManager, &DownloadManager::jobLog, this, [this](DownloadId id, const QString &message) {
        logMessage(QString("[Download #%1] %2").arg(id).arg(message));
    });

    connect(m_conversionManager, &ConversionManager::conversionStatus, this, &MainWindow::onConversionStatus);
    connect(m_conversionManager, &ConversionManager::conversionProgress, this, &MainWindow::onConversionProgress);
    connect(m_conversionManager, &ConversionManager::conversionCompleted, this, &MainWindow::onConversionCompleted);
    connect(m_conversionManager, &ConversionManager::conversionFailed, this, &MainWindow::onConversionFailed);
    connect(m_conversionManager, &ConversionManager::conversionCancelled, this, &MainWindow::onConversionCancelled);
    connect(m_conversionManager, &ConversionManager::queueIdle, this, &MainWindow::maybeShowQueueSummary);
    connect(m_conversionManager, &ConversionManager::queueIdle, this, &MainWindow::tryStartPendingAppUpdate);
    connect(m_conversionManager, &ConversionManager::queueStateChanged, this, [this](bool, int) {
        onDownloadQueueStateChanged(m_downloadManager->activeCount(), m_downloadManager->pendingCount());
    });
    connect(m_conversionManager, &ConversionManager::conversionLog, this,
            [this](ConversionId id, DownloadId owner, const QString &message) {
        const QString ownerText = owner == 0 ? "manual" : QString("download #%1").arg(owner);
        logMessage(QString("[Conversão #%1 / %2] %3").arg(id).arg(ownerText, message));
    });

    refreshLibrary();

    // Carregar configurações do Auto-Updater e iniciar checagem silenciosa ao start (se habilitado)
    QSettings settings("Tonho Studios", "PrismDownloader");
    bool checkOnStart = settings.value("checkUpdatesOnStart", true).toBool(); // Por padrão ATIVADO
    const int concurrency = qBound(1, settings.value("maxConcurrentDownloads", 2).toInt(), 5);
    if (m_concurrencySpin) m_concurrencySpin->setValue(concurrency);
    m_downloadManager->setConcurrencyLimit(concurrency);
    if (m_checkUpdatesOnStartChk) m_checkUpdatesOnStartChk->setChecked(checkOnStart);
    if (m_autoDownloadUpdatesChk) {
        m_autoDownloadUpdatesChk->setChecked(settings.value("autoDownloadUpdates", false).toBool());
        connect(m_autoDownloadUpdatesChk, &QCheckBox::toggled, this, [this](bool enabled) {
            logMessage(enabled
                ? "[Updater] Download e instalação automáticos foram ativados pelo usuário."
                : "[Updater] Download e instalação automáticos foram desativados pelo usuário.");
            if (enabled) {
                tryStartPendingAppUpdate();
            }
        });
    }

    if (checkOnStart) {
        QTimer::singleShot(2500, this, [this]() {
            logMessage("[Updater] Iniciando checagem automática silenciosa de novas versões no GitHub...");
            checkForUpdates(true); // silent = true ao iniciar para não interromper o usuário com pop-up se não houver update
            checkYtDlpUpdates(true);
        });
    } else {
        logMessage("[Updater] Busca automática de atualizações ao iniciar está desativada nas configurações.");
    }
}

void MainWindow::startGpuProbe()
{
    if (m_gpuProbeThread) {
        return;
    }
    auto *result = new GPUDetector;
    auto *thread = QThread::create([result]() {
        result->detect();
    });
    m_gpuProbeResult = result;
    m_gpuProbeThread = thread;
    connect(thread, &QThread::finished, this, [this, thread, result]() {
        if (m_gpuProbeResult == result) {
            m_gpuDetector = *result;
            m_gpuProbeResult = nullptr;
            applyGpuProbeResult();
        }
        if (m_gpuProbeThread == thread) {
            m_gpuProbeThread = nullptr;
        }
        delete result;
        thread->deleteLater();
    }, Qt::QueuedConnection);
    thread->start();
}

void MainWindow::applyGpuProbeResult()
{
    GPUDetector *gpu = &m_gpuDetector;
    const QString gpuName = QString::fromStdString(gpu->getGPUName());
    const QString codec = QString::fromStdString(gpu->getRecommendedCodec());
    const bool hasAccel = gpu->hasHardwareAcceleration();

    if (hasAccel) {
        if (m_gpuModelLabel) m_gpuModelLabel->setText(gpuName);
        if (m_gpuCodecLabel) m_gpuCodecLabel->setText(codec);
        if (m_gpuStatusLabel) {
            m_gpuStatusLabel->setText("ATIVO E OPERANTE (" + codec.toUpper() + ")");
            m_gpuStatusLabel->setStyleSheet("color: #10b981; font-weight: bold; font-size: 13px;");
        }
        if (m_convertEngineLabel) {
            m_convertEngineLabel->setText("Acelerado por Hardware (" + gpuName + " / " + codec + ")");
            m_convertEngineLabel->setStyleSheet("color: #10b981; font-weight: bold;");
        }
        logMessage(QString("[System] Placa gráfica ativa no motor: %1 (Codec: %2)").arg(gpuName, codec));
    } else {
        const QString gpuDiagnostic = QString::fromStdString(gpu->getDiagnostic());
        if (m_gpuModelLabel) m_gpuModelLabel->setText("Nenhuma aceleração de hardware compatível foi localizada");
        if (m_gpuCodecLabel) m_gpuCodecLabel->setText("Codec Fallback CPU Padrão");
        if (m_gpuStatusLabel) {
            m_gpuStatusLabel->setText("MODO FALLBACK CPU (Multi-thread)");
            m_gpuStatusLabel->setStyleSheet("color: #f59e0b; font-weight: bold; font-size: 13px;");
        }
        if (m_convertEngineLabel) {
            m_convertEngineLabel->setText("Modo Fallback CPU Padrão (Multi-thread)");
            m_convertEngineLabel->setStyleSheet("color: #f59e0b; font-weight: bold;");
        }
        logMessage("[System] Operando no modo Fallback Multi-thread CPU.");
        if (!gpuDiagnostic.isEmpty()) {
            logMessage("[GPUDetector] " + gpuDiagnostic);
        }
    }
}

MainWindow::~MainWindow()
{
    QSettings settings("Tonho Studios", "PrismDownloader");
    settings.setValue("outputFolder", m_outputDirInput->text().trimmed());
    settings.setValue("showNotifications", m_notifyCheckBox->isChecked());
    settings.setValue("selectedQuality", m_qualityCombo->currentIndex());
    settings.setValue("defaultTimeRange", m_timeRangeInput->text().trimmed());
    if (m_checkUpdatesOnStartChk) settings.setValue("checkUpdatesOnStart", m_checkUpdatesOnStartChk->isChecked());
    if (m_concurrencySpin) settings.setValue("maxConcurrentDownloads", m_concurrencySpin->value());
    if (m_autoDownloadUpdatesChk) settings.setValue("autoDownloadUpdates", m_autoDownloadUpdatesChk->isChecked());
    stopLibraryThumbnailProcesses();
    if (m_gpuProbeThread) {
        m_gpuProbeThread->wait();
        delete m_gpuProbeThread;
        m_gpuProbeThread = nullptr;
    }
    delete m_gpuProbeResult;
    m_gpuProbeResult = nullptr;
}

void MainWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QHBoxLayout *rootLayout = new QHBoxLayout(centralWidget);
    rootLayout->setSpacing(0);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    // ==========================================
    // 1. BARRA LATERAL (SIDEBAR NAVIGATION)
    // ==========================================
    QFrame *sidebar = new QFrame(this);
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(220);
    QVBoxLayout *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setSpacing(10);
    sidebarLayout->setContentsMargins(0, 20, 0, 20);

    QLabel *brandLabel = new QLabel("Studio Suite", sidebar);
    brandLabel->setAlignment(Qt::AlignCenter);
    brandLabel->setStyleSheet("font-weight: bold; font-size: 16px; color: #10b981; margin-bottom: 14px;");
    sidebarLayout->addWidget(brandLabel);

    m_navDownloadBtn = new QPushButton("Downloads", sidebar);
    m_navDownloadBtn->setObjectName("navBtn");
    m_navDownloadBtn->setCheckable(true);
    m_navDownloadBtn->setChecked(true);
    m_navDownloadBtn->setCursor(Qt::PointingHandCursor);

    m_navLibraryBtn = new QPushButton("Biblioteca", sidebar);
    m_navLibraryBtn->setObjectName("navBtn");
    m_navLibraryBtn->setCheckable(true);
    m_navLibraryBtn->setCursor(Qt::PointingHandCursor);

    m_navConverterBtn = new QPushButton("Conversor de vídeo", sidebar);
    m_navConverterBtn->setObjectName("navBtn");
    m_navConverterBtn->setCheckable(true);
    m_navConverterBtn->setCursor(Qt::PointingHandCursor);

    m_navLogsBtn = new QPushButton("Terminal de logs", sidebar);
    m_navLogsBtn->setObjectName("navBtn");
    m_navLogsBtn->setCheckable(true);
    m_navLogsBtn->setCursor(Qt::PointingHandCursor);

    m_navInfoBtn = new QPushButton("Informações", sidebar);
    m_navInfoBtn->setObjectName("navBtn");
    m_navInfoBtn->setCheckable(true);
    m_navInfoBtn->setCursor(Qt::PointingHandCursor);

    m_sidebarUpdateBtn = new QPushButton("Atualizações", sidebar);
    m_sidebarUpdateBtn->setObjectName("updateSideBtn");
    m_sidebarUpdateBtn->setCheckable(true);
    m_sidebarUpdateBtn->setCursor(Qt::PointingHandCursor);

    QButtonGroup *navGroup = new QButtonGroup(this);
    navGroup->setExclusive(true);
    navGroup->addButton(m_navDownloadBtn, 0);
    navGroup->addButton(m_navLibraryBtn, 1);
    navGroup->addButton(m_navConverterBtn, 2);
    navGroup->addButton(m_navLogsBtn, 3);
    navGroup->addButton(m_navInfoBtn, 4);
    navGroup->addButton(m_sidebarUpdateBtn, 5);

    sidebarLayout->addWidget(m_navDownloadBtn);
    sidebarLayout->addWidget(m_navLibraryBtn);
    sidebarLayout->addWidget(m_navConverterBtn);
    sidebarLayout->addWidget(m_navLogsBtn);
    sidebarLayout->addWidget(m_navInfoBtn);
    sidebarLayout->addStretch();

    m_sidebarUpdateNotification = new QLabel("🛡️ " + NEOV_VERSION_TAG + " (Em Dia)", sidebar);
    m_sidebarUpdateNotification->setAlignment(Qt::AlignCenter);
    m_sidebarUpdateNotification->setStyleSheet("color: #737373; font-size: 12px; font-weight: bold; margin-bottom: 2px;");
    sidebarLayout->addWidget(m_sidebarUpdateNotification);

    sidebarLayout->addWidget(m_sidebarUpdateBtn);

    m_openFolderBtn = new QPushButton("Abrir Pasta", sidebar);
    m_openFolderBtn->setObjectName("openFolderSideBtn");
    m_openFolderBtn->setCursor(Qt::PointingHandCursor);
    sidebarLayout->addWidget(m_openFolderBtn);

    rootLayout->addWidget(sidebar);

    // ==========================================
    // 2. ÁREA CENTRAL (STACKED WIDGET DE TELAS)
    // ==========================================
    m_stackedWidget = new QStackedWidget(this);
    m_stackedWidget->setObjectName("mainArea");
    rootLayout->addWidget(m_stackedWidget);

    // ---> TELA 0: DOWNLOADS <---
    QWidget *pageDownloads = new QWidget(m_stackedWidget);
    pageDownloads->setObjectName("downloadPage");
    QVBoxLayout *downloadsLayout = new QVBoxLayout(pageDownloads);
    downloadsLayout->setSpacing(20);
    downloadsLayout->setContentsMargins(28, 26, 28, 24);

    // LINHA SUPERIOR: INPUT DA URL + BOTÃO BAIXAR EM DESTAQUE
    QHBoxLayout *topInputLayout = new QHBoxLayout();
    topInputLayout->setSpacing(12);

    m_urlInput = new QLineEdit(pageDownloads);
    m_urlInput->setPlaceholderText("Insira nesta caixa o URL completo do seu vídeo (exemplo: https://youtube.com/watch?v=...)");
    m_urlInput->setMinimumHeight(44);
    m_urlInput->setStyleSheet("font-size: 14px; padding: 10px 14px;");

    m_startBtn = new QPushButton("ADICIONAR À FILA", pageDownloads);
    m_startBtn->setObjectName("startBtn");
    m_startBtn->setCursor(Qt::PointingHandCursor);
    m_startBtn->setMinimumHeight(44);
    m_startBtn->setFixedWidth(185);
    m_startBtn->setStyleSheet("font-size: 15px; font-weight: bold;");

    topInputLayout->addWidget(m_urlInput, 1);
    topInputLayout->addWidget(m_startBtn, 0);
    downloadsLayout->addLayout(topInputLayout);

    // LINHA SECUNDÁRIA: PERFIL DE SAÍDA E PASTA
    QGridLayout *paramLayout = new QGridLayout();
    paramLayout->setSpacing(12);
    paramLayout->setContentsMargins(0, 0, 0, 8);

    QLabel *lblProfile = new QLabel("Perfil de saída padrão:", pageDownloads);
    lblProfile->setStyleSheet("color: #a3a3a3; font-weight: bold; font-size: 13px;");
    m_qualityCombo = new QComboBox(pageDownloads);
    m_qualityCombo->addItem("4K / Melhor Disponível no Servidor (Original)");
    m_qualityCombo->addItem("1080p Full HD (Vídeo MP4 Alta Definição)");
    m_qualityCombo->addItem("720p HD (Vídeo MP4 Qualidade Padrão)");
    m_qualityCombo->addItem("Áudio MP3 (Obter apenas o áudio 320 kbps)");

    QLabel *lblTime = new QLabel("Recorte de tempo (opcional):", pageDownloads);
    lblTime->setStyleSheet("color: #a3a3a3; font-weight: bold; font-size: 13px;");
    m_timeRangeInput = new QLineEdit(pageDownloads);
    m_timeRangeInput->setPlaceholderText("Ex: 00:01:15-00:03:00 (Vazio = baixar completo)");

    QLabel *lblSave = new QLabel("Salvar downloads em:", pageDownloads);
    lblSave->setStyleSheet("color: #a3a3a3; font-weight: bold; font-size: 13px;");
    
    QHBoxLayout *saveLayout = new QHBoxLayout();
    m_outputDirInput = new QLineEdit(pageDownloads);
    m_outputDirInput->setReadOnly(false);
    m_browseDirBtn = new QPushButton("Alterar...", pageDownloads);
    m_browseDirBtn->setObjectName("browseBtn");
    m_browseDirBtn->setCursor(Qt::PointingHandCursor);
    m_browseDirBtn->setMinimumHeight(32);
    saveLayout->addWidget(m_outputDirInput, 1);
    saveLayout->addWidget(m_browseDirBtn, 0);

    QSettings settings("Tonho Studios", "PrismDownloader");
    QString savedFolder = settings.value("outputFolder", "").toString();
    if (savedFolder.isEmpty() || !QDir(savedFolder).exists()) {
        savedFolder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (savedFolder.isEmpty()) savedFolder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    }
    m_outputDirInput->setText(savedFolder);

    int savedQualityIndex = settings.value("selectedQuality", 1).toInt();
    if (savedQualityIndex >= 0 && savedQualityIndex < m_qualityCombo->count()) {
        m_qualityCombo->setCurrentIndex(savedQualityIndex);
    }

    paramLayout->addWidget(lblProfile, 0, 0);
    paramLayout->addWidget(m_qualityCombo, 0, 1);
    paramLayout->addWidget(lblTime, 1, 0);
    paramLayout->addWidget(m_timeRangeInput, 1, 1);
    paramLayout->addWidget(lblSave, 2, 0);
    paramLayout->addLayout(saveLayout, 2, 1);
    paramLayout->setColumnStretch(1, 1);

    downloadsLayout->addLayout(paramLayout);

    // PAINEL CENTRAL DE PROCESSAMENTO E MONITORAMENTO AO VIVO
    QGroupBox *centralPanel = new QGroupBox("Área de Processamento e Monitoramento de Download", pageDownloads);
    QVBoxLayout *centerLayout = new QVBoxLayout(centralPanel);
    centerLayout->setSpacing(16);
    centerLayout->setContentsMargins(20, 30, 20, 24);

    m_statusLabel = new QLabel("Status: Pronto. Aguardando você inserir uma URL acima para começar...", centralPanel);
    m_statusLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: #10b981;");

    m_progressBar = new QProgressBar(centralPanel);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setMinimumHeight(28);
    m_progressBar->setStyleSheet("font-size: 14px;");

    QHBoxLayout *statsLayout = new QHBoxLayout();
    m_speedLabel = new QLabel("Velocidade de Download: 0.0 MB/s", centralPanel);
    m_speedLabel->setStyleSheet("font-size: 13px; color: #cbd5e1;");
    m_etaLabel = new QLabel("Tempo Restante: --:--", centralPanel);
    m_etaLabel->setStyleSheet("font-size: 13px; color: #cbd5e1;");
    statsLayout->addWidget(m_speedLabel);
    statsLayout->addStretch();
    statsLayout->addWidget(m_etaLabel);

    QHBoxLayout *actionBottomLayout = new QHBoxLayout();
    m_cancelBtn = new QPushButton("CANCELAR SELECIONADO", centralPanel);
    m_cancelBtn->setObjectName("cancelBtn");
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    m_cancelBtn->setMinimumHeight(40);
    m_cancelBtn->setFixedWidth(210);
    m_cancelBtn->setEnabled(false);

    m_cancelAllBtn = new QPushButton("CANCELAR TODOS", centralPanel);
    m_cancelAllBtn->setObjectName("cancelBtn");
    m_cancelAllBtn->setCursor(Qt::PointingHandCursor);
    m_cancelAllBtn->setMinimumHeight(40);
    m_cancelAllBtn->setFixedWidth(160);
    m_cancelAllBtn->setEnabled(false);

    QLabel *concurrencyLabel = new QLabel("Simultâneos:", centralPanel);
    m_concurrencySpin = new QSpinBox(centralPanel);
    m_concurrencySpin->setRange(1, 5);
    m_concurrencySpin->setValue(2);
    m_concurrencySpin->setToolTip("Quantidade máxima de downloads ativos ao mesmo tempo");

    m_notifyCheckBox = new QCheckBox("Exibir resumo quando toda a fila terminar", centralPanel);
    bool notifyPref = settings.value("showNotifications", false).toBool();
    m_notifyCheckBox->setChecked(notifyPref);
    m_notifyCheckBox->setCursor(Qt::PointingHandCursor);

    actionBottomLayout->addWidget(m_notifyCheckBox);
    actionBottomLayout->addWidget(concurrencyLabel);
    actionBottomLayout->addWidget(m_concurrencySpin);
    actionBottomLayout->addStretch();
    actionBottomLayout->addWidget(m_cancelBtn);
    actionBottomLayout->addWidget(m_cancelAllBtn);

    m_downloadsQueueTable = new QTableWidget(0, 6, centralPanel);
    QStringList queueHeaders;
    queueHeaders << "Vídeo / Arquivo" << "Formato" << "Progresso" << "Velocidade / ETA" << "Tamanho" << "Status";
    m_downloadsQueueTable->setHorizontalHeaderLabels(queueHeaders);
    m_downloadsQueueTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_downloadsQueueTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_downloadsQueueTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_downloadsQueueTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_downloadsQueueTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_downloadsQueueTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_downloadsQueueTable->verticalHeader()->setVisible(false);
    m_downloadsQueueTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_downloadsQueueTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_downloadsQueueTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_downloadsQueueTable->setAlternatingRowColors(true);
    m_downloadsQueueTable->setObjectName("libraryTable");
    connect(m_downloadsQueueTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::onDownloadQueueDoubleClicked);
    connect(m_downloadsQueueTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::onQueueSelectionChanged);

    centerLayout->addWidget(m_statusLabel);
    centerLayout->addWidget(m_progressBar);
    centerLayout->addLayout(statsLayout);
    centerLayout->addWidget(m_downloadsQueueTable, 1);
    centerLayout->addLayout(actionBottomLayout);

    downloadsLayout->addWidget(centralPanel, 1);
    m_stackedWidget->addWidget(pageDownloads);

    // ---> TELA 1: BIBLIOTECA DE MÍDIAS <---
    QWidget *pageLibrary = new QWidget(m_stackedWidget);
    QVBoxLayout *libLayout = new QVBoxLayout(pageLibrary);
    libLayout->setSpacing(14);
    libLayout->setContentsMargins(24, 20, 24, 20);

    QHBoxLayout *libTopLayout = new QHBoxLayout();
    QLabel *libTitle = new QLabel("Biblioteca de Mídias (Arquivos na Pasta de Destino):", pageLibrary);
    libTitle->setStyleSheet("font-weight: bold; color: #10b981; font-size: 15px;");
    libTopLayout->addWidget(libTitle);
    libTopLayout->addStretch();

    QPushButton *btnRefreshLib = new QPushButton("Atualizar Lista", pageLibrary);
    btnRefreshLib->setObjectName("browseBtn");
    btnRefreshLib->setCursor(Qt::PointingHandCursor);
    btnRefreshLib->setMinimumHeight(32);
    connect(btnRefreshLib, &QPushButton::clicked, this, &MainWindow::refreshLibrary);
    libTopLayout->addWidget(btnRefreshLib);

    m_libraryListViewBtn = new QPushButton("☷ Lista", pageLibrary);
    m_libraryListViewBtn->setObjectName("libraryViewBtn");
    m_libraryListViewBtn->setCheckable(true);
    m_libraryListViewBtn->setChecked(true);
    m_libraryListViewBtn->setCursor(Qt::PointingHandCursor);
    m_libraryListViewBtn->setMinimumHeight(32);
    connect(m_libraryListViewBtn, &QPushButton::clicked, this, [this]() {
        setLibraryViewMode(false);
    });

    m_libraryBlocksViewBtn = new QPushButton("▦ Blocos", pageLibrary);
    m_libraryBlocksViewBtn->setObjectName("libraryViewBtn");
    m_libraryBlocksViewBtn->setCheckable(true);
    m_libraryBlocksViewBtn->setCursor(Qt::PointingHandCursor);
    m_libraryBlocksViewBtn->setMinimumHeight(32);
    connect(m_libraryBlocksViewBtn, &QPushButton::clicked, this, [this]() {
        setLibraryViewMode(true);
    });

    auto *libraryViewGroup = new QButtonGroup(pageLibrary);
    libraryViewGroup->setExclusive(true);
    libraryViewGroup->addButton(m_libraryListViewBtn);
    libraryViewGroup->addButton(m_libraryBlocksViewBtn);
    libTopLayout->addWidget(m_libraryListViewBtn);
    libTopLayout->addWidget(m_libraryBlocksViewBtn);

    libLayout->addLayout(libTopLayout);

    m_libraryTable = new QTableWidget(0, 3, pageLibrary);
    QStringList headers;
    headers << "Arquivo de Mídia" << "Formato" << "Tamanho";
    m_libraryTable->setHorizontalHeaderLabels(headers);
    m_libraryTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_libraryTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_libraryTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_libraryTable->verticalHeader()->setVisible(false);
    m_libraryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_libraryTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_libraryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_libraryTable->setAlternatingRowColors(true);
    m_libraryTable->setObjectName("libraryTable");
    connect(m_libraryTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::onLibraryDoubleClicked);

    m_libraryBlocks = new QListWidget(pageLibrary);
    m_libraryBlocks->setObjectName("libraryBlocks");
    m_libraryBlocks->setViewMode(QListView::IconMode);
    m_libraryBlocks->setFlow(QListView::LeftToRight);
    m_libraryBlocks->setResizeMode(QListView::Adjust);
    m_libraryBlocks->setMovement(QListView::Static);
    m_libraryBlocks->setWrapping(true);
    m_libraryBlocks->setSpacing(12);
    m_libraryBlocks->setGridSize(QSize(220, 204));
    m_libraryBlocks->setSelectionMode(QAbstractItemView::SingleSelection);
    m_libraryBlocks->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_libraryBlocks->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    connect(m_libraryBlocks, &QListWidget::itemDoubleClicked,
            this, &MainWindow::onLibraryBlockDoubleClicked);

    m_libraryViewStack = new QStackedWidget(pageLibrary);
    m_libraryViewStack->addWidget(m_libraryTable);
    m_libraryViewStack->addWidget(m_libraryBlocks);
    m_libraryViewStack->setCurrentIndex(0);
    libLayout->addWidget(m_libraryViewStack);

    QHBoxLayout *libBottomLayout = new QHBoxLayout();
    QPushButton *btnPlay = new QPushButton("REPRODUZIR / ABRIR SELECIONADO", pageLibrary);
    btnPlay->setObjectName("startBtn");
    btnPlay->setCursor(Qt::PointingHandCursor);
    btnPlay->setMinimumHeight(40);
    connect(btnPlay, &QPushButton::clicked, this, &MainWindow::onPlaySelectedMedia);

    QPushButton *btnOpenExp = new QPushButton("ABRIR NO EXPLORER", pageLibrary);
    btnOpenExp->setObjectName("openFolderSideBtn");
    btnOpenExp->setCursor(Qt::PointingHandCursor);
    btnOpenExp->setMinimumHeight(40);
    connect(btnOpenExp, &QPushButton::clicked, this, &MainWindow::onOpenFolderClicked);

    libBottomLayout->addWidget(btnPlay, 2);
    libBottomLayout->addWidget(btnOpenExp, 1);
    libLayout->addLayout(libBottomLayout);

    m_stackedWidget->addWidget(pageLibrary);

    // ---> TELA 2: CONVERSOR DE VÍDEO E ÁUDIO <---
    QWidget *pageConverter = new QWidget(m_stackedWidget);
    QVBoxLayout *convLayout = new QVBoxLayout(pageConverter);
    convLayout->setSpacing(16);
    convLayout->setContentsMargins(24, 20, 24, 20);

    QGroupBox *convGroup = new QGroupBox("Conversor Nativo de Mídia", pageConverter);
    QGridLayout *convGrid = new QGridLayout(convGroup);
    convGrid->setSpacing(14);
    convGrid->setContentsMargins(16, 26, 16, 18);

    QLabel *lblConvFile = new QLabel("Arquivo de Origem:", pageConverter);
    m_convertInput = new QLineEdit(pageConverter);
    m_convertInput->setPlaceholderText("Selecione um vídeo ou música no seu computador...");
    m_convertBrowseBtn = new QPushButton("Selecionar...", pageConverter);
    m_convertBrowseBtn->setObjectName("browseBtn");
    m_convertBrowseBtn->setCursor(Qt::PointingHandCursor);
    m_convertBrowseBtn->setMinimumHeight(34);
    connect(m_convertBrowseBtn, &QPushButton::clicked, this, &MainWindow::onConvertBrowseClicked);

    QHBoxLayout *convFileLayout = new QHBoxLayout();
    convFileLayout->addWidget(m_convertInput);
    convFileLayout->addWidget(m_convertBrowseBtn);

    QLabel *lblConvFormat = new QLabel("Formato de Saída:", pageConverter);
    m_convertFormatCombo = new QComboBox(pageConverter);
    m_convertFormatCombo->addItem("MP4 (H.264 / Aceleração quando disponível)");
    m_convertFormatCombo->addItem("MP4 (HEVC / H.265 - Compressão de Alta Densidade)");
    m_convertFormatCombo->addItem("MKV (Matroska - Container Sem Perdas)");
    m_convertFormatCombo->addItem("MP3 (Áudio MP3 Alta Fidelidade - 320kbps)");
    m_convertFormatCombo->addItem("WAV (Áudio Sem Compressão / Estúdios)");
    m_convertFormatCombo->addItem("WEBM (Otimizado para Web e Redes Sociais)");

    QLabel *lblConvEngineTitle = new QLabel("Motor de Aceleração:", pageConverter);
    m_convertEngineLabel = new QLabel("Sondando GPU...", pageConverter);

    convGrid->addWidget(lblConvFile, 0, 0);
    convGrid->addLayout(convFileLayout, 0, 1);
    convGrid->addWidget(lblConvFormat, 1, 0);
    convGrid->addWidget(m_convertFormatCombo, 1, 1);
    convGrid->addWidget(lblConvEngineTitle, 2, 0);
    convGrid->addWidget(m_convertEngineLabel, 2, 1);

    convLayout->addWidget(convGroup);

    QHBoxLayout *convBtnLayout = new QHBoxLayout();
    m_startConvertBtn = new QPushButton("INICIAR CONVERSÃO RÁPIDA", pageConverter);
    m_startConvertBtn->setObjectName("startBtn");
    m_startConvertBtn->setCursor(Qt::PointingHandCursor);
    m_startConvertBtn->setMinimumHeight(44);
    connect(m_startConvertBtn, &QPushButton::clicked, this, &MainWindow::onStartConvertClicked);

    m_cancelConvertBtn = new QPushButton("CANCELAR", pageConverter);
    m_cancelConvertBtn->setObjectName("cancelBtn");
    m_cancelConvertBtn->setCursor(Qt::PointingHandCursor);
    m_cancelConvertBtn->setMinimumHeight(44);
    m_cancelConvertBtn->setEnabled(false);
    connect(m_cancelConvertBtn, &QPushButton::clicked, this, &MainWindow::onCancelConvertClicked);

    convBtnLayout->addWidget(m_startConvertBtn, 3);
    convBtnLayout->addWidget(m_cancelConvertBtn, 1);
    convLayout->addLayout(convBtnLayout);

    QGroupBox *convMonGroup = new QGroupBox("Progresso da Conversão", pageConverter);
    QVBoxLayout *convMonLayout = new QVBoxLayout(convMonGroup);
    convMonLayout->setSpacing(12);
    convMonLayout->setContentsMargins(16, 24, 16, 16);

    m_convertStatusLabel = new QLabel("Status do Conversor: Aguardando seleção do arquivo...", pageConverter);
    m_convertStatusLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #38bdf8;");

    m_convertProgressBar = new QProgressBar(pageConverter);
    m_convertProgressBar->setRange(0, 100);
    m_convertProgressBar->setValue(0);
    m_convertProgressBar->setMinimumHeight(22);

    convMonLayout->addWidget(m_convertStatusLabel);
    convMonLayout->addWidget(m_convertProgressBar);

    convLayout->addWidget(convMonGroup);
    convLayout->addStretch();
    m_stackedWidget->addWidget(pageConverter);

    // ---> TELA 3: TERMINAL DE LOGS <---
    QWidget *pageLogs = new QWidget(m_stackedWidget);
    QVBoxLayout *logsLayout = new QVBoxLayout(pageLogs);
    logsLayout->setSpacing(12);
    logsLayout->setContentsMargins(24, 20, 24, 20);

    QLabel *logsTitle = new QLabel("Terminal de logs do processador e telemetria:", pageLogs);
    logsTitle->setStyleSheet("font-weight: bold; color: #10b981; font-size: 15px;");
    logsLayout->addWidget(logsTitle);

    QHBoxLayout *logFilterLayout = new QHBoxLayout();
    logFilterLayout->setSpacing(8);

    m_filterAllBtn = new QPushButton("🌐 Todos os Logs", pageLogs);
    m_filterProcessesBtn = new QPushButton("⚙️ Apenas Processos", pageLogs);
    m_filterErrorsBtn = new QPushButton("❌ Apenas Erros", pageLogs);
    m_filterGeneralBtn = new QPushButton("📌 Sistema & Gerais", pageLogs);
    m_clearLogsBtn = new QPushButton("🧹 Limpar Terminal", pageLogs);

    m_filterAllBtn->setCursor(Qt::PointingHandCursor);
    m_filterProcessesBtn->setCursor(Qt::PointingHandCursor);
    m_filterErrorsBtn->setCursor(Qt::PointingHandCursor);
    m_filterGeneralBtn->setCursor(Qt::PointingHandCursor);
    m_clearLogsBtn->setCursor(Qt::PointingHandCursor);

    logFilterLayout->addWidget(m_filterAllBtn);
    logFilterLayout->addWidget(m_filterProcessesBtn);
    logFilterLayout->addWidget(m_filterErrorsBtn);
    logFilterLayout->addWidget(m_filterGeneralBtn);
    logFilterLayout->addStretch();
    logFilterLayout->addWidget(m_clearLogsBtn);
    logsLayout->addLayout(logFilterLayout);

    connect(m_filterAllBtn, &QPushButton::clicked, this, [this]() { updateLogFilter(0); });
    connect(m_filterProcessesBtn, &QPushButton::clicked, this, [this]() { updateLogFilter(1); });
    connect(m_filterErrorsBtn, &QPushButton::clicked, this, [this]() { updateLogFilter(2); });
    connect(m_filterGeneralBtn, &QPushButton::clicked, this, [this]() { updateLogFilter(3); });
    connect(m_clearLogsBtn, &QPushButton::clicked, this, [this]() { m_allLogs.clear(); refreshLogDisplay(); });

    m_logEdit = new QPlainTextEdit(pageLogs);
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumBlockCount(kMaximumLogEntries);
    m_logEdit->setObjectName("logArea");
    logsLayout->addWidget(m_logEdit);
    m_stackedWidget->addWidget(pageLogs);

    updateLogFilter(0); // Inicializa com estilo ativo após m_logEdit já existir com segurança!

    // ---> TELA 4: INFORMAÇÕES E HARDWARE <---
    QWidget *pageInfo = new QWidget(m_stackedWidget);
    QVBoxLayout *infoLayout = new QVBoxLayout(pageInfo);
    infoLayout->setSpacing(16);
    infoLayout->setContentsMargins(24, 20, 24, 20);



    QGroupBox *appInfoGroup = new QGroupBox("Informações do Aplicativo", pageInfo);
    QGridLayout *appLayout = new QGridLayout(appInfoGroup);
    appLayout->setSpacing(10);
    appLayout->setContentsMargins(16, 24, 16, 16);

    QLabel *lblAppNameKey = new QLabel("Nome Oficial:", appInfoGroup);
    lblAppNameKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    QLabel *lblAppNameVal = new QLabel("Prism Downloader (Studio Suite Edition)", appInfoGroup);
    lblAppNameVal->setStyleSheet("color: #ffffff; font-weight: bold; font-size: 13px;");

    QLabel *lblAppVerKey = new QLabel("Versão Atual:", appInfoGroup);
    lblAppVerKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    QLabel *lblAppVerVal = new QLabel(NEOV_VERSION_NUMBER + " (Estável / Release)", appInfoGroup);
    lblAppVerVal->setStyleSheet("color: #10b981; font-weight: bold; font-size: 13px;");

    QLabel *lblAppArchKey = new QLabel("Arquitetura:", appInfoGroup);
    lblAppArchKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    QLabel *lblAppArchVal = new QLabel("Núcleo em C++17 Padrão + Interface Gráfica Qt 6.7", appInfoGroup);
    lblAppArchVal->setStyleSheet("color: #e2e8f0; font-size: 13px;");

    QLabel *lblAppEngineKey = new QLabel("Motor de Processamento:", appInfoGroup);
    lblAppEngineKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    QLabel *lblAppEngineVal = new QLabel("Multi-Thread Paralelo com Isolamento de Execução", appInfoGroup);
    lblAppEngineVal->setStyleSheet("color: #e2e8f0; font-size: 13px;");

    appLayout->addWidget(lblAppNameKey, 0, 0);
    appLayout->addWidget(lblAppNameVal, 0, 1);
    appLayout->addWidget(lblAppVerKey, 1, 0);
    appLayout->addWidget(lblAppVerVal, 1, 1);
    appLayout->addWidget(lblAppArchKey, 2, 0);
    appLayout->addWidget(lblAppArchVal, 2, 1);
    appLayout->addWidget(lblAppEngineKey, 3, 0);
    appLayout->addWidget(lblAppEngineVal, 3, 1);
    appLayout->setColumnStretch(1, 1);
    infoLayout->addWidget(appInfoGroup);

    QGroupBox *hwInfoGroup = new QGroupBox("Diagnóstico de Hardware e Aceleração", pageInfo);
    QGridLayout *hwLayout = new QGridLayout(hwInfoGroup);
    hwLayout->setSpacing(10);
    hwLayout->setContentsMargins(16, 24, 16, 16);

    QLabel *lblGpuModelKey = new QLabel("Placa Gráfica Detectada:", hwInfoGroup);
    lblGpuModelKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    m_gpuModelLabel = new QLabel("Sondando hardware...", hwInfoGroup);
    m_gpuModelLabel->setStyleSheet("color: #38bdf8; font-weight: bold; font-size: 13px;");

    QLabel *lblGpuCodecKey = new QLabel("Codec de Aceleração:", hwInfoGroup);
    lblGpuCodecKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    m_gpuCodecLabel = new QLabel("---", hwInfoGroup);
    m_gpuCodecLabel->setStyleSheet("color: #e2e8f0; font-size: 13px; font-family: 'Consolas', monospace;");

    QLabel *lblGpuStatusKey = new QLabel("Status do Motor:", hwInfoGroup);
    lblGpuStatusKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    m_gpuStatusLabel = new QLabel("Verificando...", hwInfoGroup);

    hwLayout->addWidget(lblGpuModelKey, 0, 0);
    hwLayout->addWidget(m_gpuModelLabel, 0, 1);
    hwLayout->addWidget(lblGpuCodecKey, 1, 0);
    hwLayout->addWidget(m_gpuCodecLabel, 1, 1);
    hwLayout->addWidget(lblGpuStatusKey, 2, 0);
    hwLayout->addWidget(m_gpuStatusLabel, 2, 1);
    hwLayout->setColumnStretch(1, 1);
    infoLayout->addWidget(hwInfoGroup);

    QGroupBox *techGroup = new QGroupBox("Tecnologias Integradas no Core", pageInfo);
    QGridLayout *techLayout = new QGridLayout(techGroup);
    techLayout->setSpacing(10);
    techLayout->setContentsMargins(16, 24, 16, 16);

    QLabel *lblTechMergeKey = new QLabel("Mescla de Mídias:", techGroup);
    lblTechMergeKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    QLabel *lblTechMergeVal = new QLabel("FFmpeg Nativo com tecnologia Zero-Loss Stream Copy", techGroup);
    lblTechMergeVal->setStyleSheet("color: #e2e8f0; font-size: 13px;");

    QLabel *lblTechTimeKey = new QLabel("Tempo de Junção:", techGroup);
    lblTechTimeKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    QLabel *lblTechTimeVal = new QLabel("< 1 segundo por arquivo (sem recodificação redundante de áudio/vídeo)", techGroup);
    lblTechTimeVal->setStyleSheet("color: #10b981; font-weight: bold; font-size: 13px;");

    QLabel *lblTechExtKey = new QLabel("Engine Extrator:", techGroup);
    lblTechExtKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    QLabel *lblTechExtVal = new QLabel("yt-dlp nativo de alta compatibilidade com mais de 1000 plataformas", techGroup);
    lblTechExtVal->setStyleSheet("color: #e2e8f0; font-size: 13px;");

    techLayout->addWidget(lblTechMergeKey, 0, 0);
    techLayout->addWidget(lblTechMergeVal, 0, 1);
    techLayout->addWidget(lblTechTimeKey, 1, 0);
    techLayout->addWidget(lblTechTimeVal, 1, 1);
    techLayout->addWidget(lblTechExtKey, 2, 0);
    techLayout->addWidget(lblTechExtVal, 2, 1);
    techLayout->setColumnStretch(1, 1);
    infoLayout->addWidget(techGroup);

    infoLayout->addStretch();
    m_stackedWidget->addWidget(pageInfo);

    // ---> TELA 5: CENTRAL DE ATUALIZAÇÕES E COMPATIBILIDADE <---
    QWidget *pageUpdates = new QWidget(m_stackedWidget);
    QVBoxLayout *upPageLayout = new QVBoxLayout(pageUpdates);
    upPageLayout->setSpacing(16);
    upPageLayout->setContentsMargins(24, 20, 24, 20);

    QGroupBox *updateGroup = new QGroupBox("Central de Atualizações e Versão (GitHub Release Core)", pageUpdates);
    QVBoxLayout *upLayout = new QVBoxLayout(updateGroup);
    upLayout->setSpacing(12);
    upLayout->setContentsMargins(16, 24, 16, 16);

    QGridLayout *upGrid = new QGridLayout();
    upGrid->setSpacing(10);

    QLabel *lblServerKey = new QLabel("Repositório de Nuvem:", updateGroup);
    lblServerKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    QLabel *lblServerVal = new QLabel("GitHub Oficial (BadTonho/PrismDownloader)", updateGroup);
    lblServerVal->setStyleSheet("color: #38bdf8; font-weight: bold; font-size: 13px;");

    QLabel *lblUpStatusKey = new QLabel("Status de Versão:", updateGroup);
    lblUpStatusKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    m_updateStatusLabel = new QLabel("Versão " + NEOV_VERSION_TAG + " (Release) operacional. Aguardando verificação...", updateGroup);
    m_updateStatusLabel->setStyleSheet("color: #ffffff; font-weight: bold; font-size: 13px;");

    QLabel *lblYtdlpStatusKey = new QLabel("Motor yt-dlp:", updateGroup);
    lblYtdlpStatusKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    m_ytdlpStatusLabel = new QLabel("Localizando versão e origem do motor...", updateGroup);
    m_ytdlpStatusLabel->setWordWrap(true);
    m_ytdlpStatusLabel->setStyleSheet("color: #ffffff; font-weight: bold; font-size: 13px;");

    upGrid->addWidget(lblServerKey, 0, 0);
    upGrid->addWidget(lblServerVal, 0, 1);
    upGrid->addWidget(lblUpStatusKey, 1, 0);
    upGrid->addWidget(m_updateStatusLabel, 1, 1);
    upGrid->addWidget(lblYtdlpStatusKey, 2, 0);
    upGrid->addWidget(m_ytdlpStatusLabel, 2, 1);
    upGrid->setColumnStretch(1, 1);
    upLayout->addLayout(upGrid);

    m_checkUpdatesOnStartChk = new QCheckBox("Verificar novas atualizações automaticamente ao iniciar o aplicativo", updateGroup);
    m_checkUpdatesOnStartChk->setCursor(Qt::PointingHandCursor);
    
    m_autoDownloadUpdatesChk = new QCheckBox("Baixar, validar por assinatura e instalar automaticamente atualizações do aplicativo", updateGroup);
    m_autoDownloadUpdatesChk->setCursor(Qt::PointingHandCursor);
    m_autoDownloadUpdatesChk->setStyleSheet("color: #f59e0b; font-weight: bold;");

    QVBoxLayout *chkVertLayout = new QVBoxLayout();
    chkVertLayout->setSpacing(6);
    chkVertLayout->addWidget(m_checkUpdatesOnStartChk);
    chkVertLayout->addWidget(m_autoDownloadUpdatesChk);
    upLayout->addLayout(chkVertLayout);

    QHBoxLayout *upBtnsLayout = new QHBoxLayout();
    m_updateAppBtn = new QPushButton("BAIXAR E ATUALIZAR AGORA", updateGroup);
    m_updateAppBtn->setObjectName("startBtn");
    m_updateAppBtn->setCursor(Qt::PointingHandCursor);
    m_updateAppBtn->setMinimumHeight(40);
    m_updateAppBtn->setVisible(false);
    connect(m_updateAppBtn, &QPushButton::clicked, this, &MainWindow::requestAppUpdate);

    m_checkUpdateBtn = new QPushButton("VERIFICAR NO GITHUB AGORA", updateGroup);
    m_checkUpdateBtn->setObjectName("startBtn");
    m_checkUpdateBtn->setCursor(Qt::PointingHandCursor);
    m_checkUpdateBtn->setMinimumHeight(40);
    connect(m_checkUpdateBtn, &QPushButton::clicked, this, [this]() {
        checkForUpdates(false);
        checkYtDlpUpdates(false);
    });

    m_updateYtdlpBtn = new QPushButton("VERIFICAR YT-DLP NIGHTLY", updateGroup);
    m_updateYtdlpBtn->setObjectName("browseBtn");
    m_updateYtdlpBtn->setCursor(Qt::PointingHandCursor);
    m_updateYtdlpBtn->setMinimumHeight(40);
    connect(m_updateYtdlpBtn, &QPushButton::clicked, this, &MainWindow::updateYtdlpEngine);

    upBtnsLayout->addWidget(m_updateAppBtn, 3);
    upBtnsLayout->addWidget(m_checkUpdateBtn, 2);
    upBtnsLayout->addWidget(m_updateYtdlpBtn, 2);
    upLayout->addLayout(upBtnsLayout);

    m_updateProgressBar = new QProgressBar(updateGroup);
    m_updateProgressBar->setRange(0, 100);
    m_updateProgressBar->setValue(0);
    m_updateProgressBar->setTextVisible(true);
    m_updateProgressBar->setStyleSheet("QProgressBar { background-color: #171717; border: 1px solid #333333; border-radius: 6px; color: #ffffff; font-weight: bold; height: 24px; text-align: center; } QProgressBar::chunk { background-color: #10b981; border-radius: 5px; }");
    m_updateProgressBar->setVisible(false);
    upLayout->addWidget(m_updateProgressBar);

    upPageLayout->addWidget(updateGroup);
    upPageLayout->addStretch();
    m_stackedWidget->addWidget(pageUpdates);

    // Conectar navegação e botões principais
    connect(navGroup, &QButtonGroup::idClicked, this, &MainWindow::switchPage);
    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &MainWindow::onCancelClicked);
    connect(m_cancelAllBtn, &QPushButton::clicked, this, &MainWindow::onCancelAllClicked);
    connect(m_concurrencySpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onConcurrencyChanged);
    connect(m_browseDirBtn, &QPushButton::clicked, this, &MainWindow::onBrowseClicked);
    connect(m_openFolderBtn, &QPushButton::clicked, this, &MainWindow::onOpenFolderClicked);

}

void MainWindow::switchPage(int index)
{
    if (index == 0) {
        showDownloadsPage();
        return;
    }

    if (m_stackedWidget) {
        if (index >= 0 && index < m_stackedWidget->count()) {
            m_stackedWidget->setCurrentIndex(index);
        }
        if (index == 1) { // Se abriu a aba Biblioteca, atualizar a tabela
            refreshLibrary();
        }
    }
}

void MainWindow::showDownloadsPage()
{
    if (m_navDownloadBtn) {
        m_navDownloadBtn->setChecked(true);
    }
    if (m_stackedWidget) {
        m_stackedWidget->setCurrentIndex(0);
    }
}

void MainWindow::closePlaylistPreviewDialog()
{
    if (!m_playlistPreviewDialog) {
        return;
    }
    m_playlistPreviewDialog->hide();
    m_playlistPreviewDialog->deleteLater();
    m_playlistPreviewDialog = nullptr;
}

void MainWindow::startPlaylistPreview(const QUrl &url)
{
    if (m_playlistPreviewProcess) {
        return;
    }

    const QString program = MediaToolResolver::resolve(MediaTool::YtDlp);
    if (program.isEmpty() || !QFile::exists(program)) {
        QMessageBox::warning(this, "Motor indisponível",
                             MediaToolResolver::missingMessage(MediaTool::YtDlp));
        return;
    }

    auto *process = new QProcess(this);
    m_playlistPreviewProcess = process;
    m_playlistPreviewOutput.clear();
    m_playlistPreviewErrorOutput.clear();
    m_playlistPreviewTruncated = false;
    process->setProcessChannelMode(QProcess::SeparateChannels);

    const auto consumePlaylistOutput = [this](const QByteArray &chunk) {
        if (chunk.isEmpty()) {
            return;
        }
        const qsizetype remaining = kMaximumPlaylistOutputBytes - m_playlistPreviewOutput.size();
        if (remaining <= 0) {
            m_playlistPreviewTruncated = true;
            return;
        }
        if (chunk.size() > remaining) {
            m_playlistPreviewOutput.append(chunk.constData(), static_cast<int>(remaining));
            m_playlistPreviewTruncated = true;
        } else {
            m_playlistPreviewOutput.append(chunk);
        }
    };
    connect(process, &QProcess::readyReadStandardOutput, this,
            [process, consumePlaylistOutput]() {
        consumePlaylistOutput(process->readAllStandardOutput());
    });
    const auto consumePlaylistError = [this](const QByteArray &chunk) {
        if (chunk.isEmpty()) {
            return;
        }
        const qsizetype remaining = kMaximumPlaylistErrorBytes - m_playlistPreviewErrorOutput.size();
        if (remaining > 0) {
            m_playlistPreviewErrorOutput.append(
                chunk.constData(), static_cast<int>(qMin<qsizetype>(remaining, chunk.size())));
        }
    };
    connect(process, &QProcess::readyReadStandardError, this,
            [process, consumePlaylistError]() {
        consumePlaylistError(process->readAllStandardError());
    });

    connect(process, &QProcess::finished, this,
            [this, process, consumePlaylistOutput, consumePlaylistError](int exitCode, QProcess::ExitStatus) {
        if (m_playlistPreviewProcess != process) {
            process->deleteLater();
            return;
        }

        consumePlaylistOutput(process->readAllStandardOutput());
        consumePlaylistError(process->readAllStandardError());
        const bool truncated = m_playlistPreviewTruncated;
        const QList<PlaylistItem> items = parsePlaylistPreview(m_playlistPreviewOutput);
        const QString errorOutput = QString::fromUtf8(m_playlistPreviewErrorOutput).trimmed();
        m_playlistPreviewOutput.clear();
        m_playlistPreviewErrorOutput.clear();
        m_playlistPreviewTruncated = false;
        m_playlistPreviewProcess = nullptr;
        process->deleteLater();
        closePlaylistPreviewDialog();
        m_startBtn->setEnabled(true);

        if (items.isEmpty()) {
            const QString detail = errorOutput.isEmpty()
                ? "Nenhum vídeo foi encontrado nessa playlist."
                : errorOutput;
            QMessageBox::warning(this, "Playlist não identificada",
                                 "Não foi possível listar os vídeos da playlist.\n\n" + detail);
            logMessage("[Playlist] Não foi possível obter os itens da playlist.");
            return;
        }

        if (exitCode != 0) {
            logMessage(QString("[Playlist] yt-dlp retornou código %1, mas %2 item(ns) foram identificados.")
                           .arg(exitCode).arg(items.size()));
        } else {
            logMessage(QString("[Playlist] %1 item(ns) encontrado(s) para seleção.").arg(items.size()));
        }
        if (truncated) {
            logMessage(QString("[Playlist] Prévia limitada aos primeiros %1 itens para preservar desempenho.")
                           .arg(kMaximumPlaylistItems));
        }

        QList<PlaylistItem> selectedItems;
        if (!showPlaylistSelectionDialog(items, selectedItems)) {
            logMessage("[Playlist] Seleção de itens cancelada pelo usuário.");
            return;
        }
        continueDownload(selectedItems);
    });

    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || m_playlistPreviewProcess != process) {
            return;
        }
        m_playlistPreviewOutput.clear();
        m_playlistPreviewErrorOutput.clear();
        m_playlistPreviewTruncated = false;
        m_playlistPreviewProcess = nullptr;
        process->deleteLater();
        closePlaylistPreviewDialog();
        m_startBtn->setEnabled(true);
        QMessageBox::warning(this, "Falha ao consultar playlist",
                             "Não foi possível iniciar o yt-dlp para listar a playlist.");
    });

    m_startBtn->setEnabled(false);
    m_playlistPreviewDialog = new QProgressDialog(
        "Consultando os vídeos da playlist...\nIsso pode levar alguns segundos.",
        "Cancelar", 0, 0, this);
    m_playlistPreviewDialog->setWindowTitle("Consultando playlist");
    m_playlistPreviewDialog->setWindowModality(Qt::WindowModal);
    m_playlistPreviewDialog->setMinimumDuration(0);
    m_playlistPreviewDialog->setAutoClose(false);
    m_playlistPreviewDialog->setAutoReset(false);
    m_playlistPreviewDialog->show();
    connect(m_playlistPreviewDialog, &QProgressDialog::canceled, this, [this, process]() {
        if (m_playlistPreviewProcess != process) {
            return;
        }
        m_playlistPreviewProcess = nullptr;
        m_playlistPreviewOutput.clear();
        m_playlistPreviewErrorOutput.clear();
        m_playlistPreviewTruncated = false;
        process->kill();
        process->deleteLater();
        closePlaylistPreviewDialog();
        m_startBtn->setEnabled(true);
        logMessage("[Playlist] Consulta cancelada pelo usuário.");
    });

    logMessage("[Playlist] Consultando os vídeos disponíveis...");
    const QStringList arguments = {
        "--flat-playlist",
        "--print", "%(title)s\t%(webpage_url)s\t%(duration_string)s\t%(thumbnail)s",
        "--skip-download",
        "--quiet",
        "--no-warnings",
        "--no-color",
        "--ignore-errors",
        "--",
        url.toString(QUrl::FullyEncoded)
    };
    process->start(program, arguments);
}

bool MainWindow::showPlaylistSelectionDialog(const QList<PlaylistItem> &items,
                                             QList<PlaylistItem> &selectedItems)
{
    QDialog dialog(this);
    dialog.setObjectName("playlistSelectionDialog");
    dialog.setWindowTitle("Itens da playlist - Prism Studio Suite");
    dialog.resize(900, 650);
    dialog.setMinimumSize(760, 520);
    dialog.setStyleSheet(this->styleSheet() + R"(
        QDialog#playlistSelectionDialog { background-color: #151515; }
        QFrame#playlistHeader {
            background-color: #1d2924;
            border: 1px solid #275b45;
            border-radius: 10px;
        }
        QLabel#playlistKicker {
            color: #10b981;
            font-size: 11px;
            font-weight: bold;
            letter-spacing: 1px;
        }
        QLabel#playlistTitle { color: #ffffff; font-size: 20px; font-weight: bold; }
        QLabel#playlistSubtitle { color: #a3a3a3; font-size: 13px; }
        QLabel#playlistSelectionCount { color: #10b981; font-weight: bold; font-size: 13px; }
        QLineEdit#playlistSearch {
            background-color: #202020;
            border: 1px solid #3b4b43;
            border-radius: 7px;
            padding: 9px 12px;
            color: #ffffff;
        }
        QLineEdit#playlistSearch:focus { border-color: #10b981; background-color: #252525; }
        QListWidget#playlistItems {
            background-color: #1b1b1b;
            border: 1px solid #303030;
            border-radius: 8px;
            outline: none;
        }
        QListWidget#playlistItems::item {
            min-height: 30px;
            padding: 5px 10px;
            border-bottom: 1px solid #292929;
            color: #ededed;
        }
        QListWidget#playlistItems::item:hover { background-color: #25372f; }
        QListWidget#playlistItems::item:selected { background-color: #25372f; }
        QWidget#playlistItemRow { background-color: #1b1b1b; border-bottom: 1px solid #292929; }
        QCheckBox#playlistItemCheck { padding-left: 6px; }
        QPushButton#playlistItemTitle {
            background: transparent;
            border: none;
            color: #ededed;
            font-size: 13px;
            padding: 7px 8px;
            text-align: left;
        }
        QPushButton#playlistItemTitle:hover { color: #10b981; text-decoration: underline; }
        QPushButton#playlistSecondaryButton {
            background-color: #252525;
            color: #d4d4d4;
            border: 1px solid #424242;
            border-radius: 6px;
            padding: 7px 12px;
            font-weight: bold;
        }
        QPushButton#playlistSecondaryButton:hover { border-color: #10b981; color: #ffffff; }
        QPushButton#playlistConfirmButton {
            background-color: #10b981;
            color: #021810;
            border: none;
            border-radius: 6px;
            padding: 10px 16px;
            font-size: 13px;
            font-weight: bold;
        }
        QPushButton#playlistConfirmButton:hover { background-color: #059669; }
        QPushButton#playlistConfirmButton:disabled { background-color: #2c2c2c; color: #737373; }
    )");

    auto *layout = new QVBoxLayout(&dialog);
    layout->setSpacing(14);
    layout->setContentsMargins(24, 22, 24, 20);

    auto *header = new QFrame(&dialog);
    header->setObjectName("playlistHeader");
    auto *headerLayout = new QVBoxLayout(header);
    headerLayout->setSpacing(4);
    headerLayout->setContentsMargins(18, 14, 18, 14);

    auto *kicker = new QLabel("PLAYLIST ENCONTRADA", header);
    kicker->setObjectName("playlistKicker");
    auto *title = new QLabel("Escolha os vídeos que entrarão na fila", header);
    title->setObjectName("playlistTitle");
    auto *subtitle = new QLabel(
        QString("%1 vídeo(s) disponível(is). Clique em um nome para ver miniatura e duração.").arg(items.size()), header);
    subtitle->setObjectName("playlistSubtitle");
    headerLayout->addWidget(kicker);
    headerLayout->addWidget(title);
    headerLayout->addWidget(subtitle);
    layout->addWidget(header);

    auto *searchInput = new QLineEdit(&dialog);
    searchInput->setObjectName("playlistSearch");
    searchInput->setPlaceholderText("Filtrar por título...");
    searchInput->setClearButtonEnabled(true);
    layout->addWidget(searchInput);

    auto *toolbar = new QHBoxLayout();
    auto *selectionCount = new QLabel(&dialog);
    selectionCount->setObjectName("playlistSelectionCount");
    auto *selectAllButton = new QPushButton("Selecionar todos", &dialog);
    selectAllButton->setObjectName("playlistSecondaryButton");
    auto *clearButton = new QPushButton("Limpar seleção", &dialog);
    clearButton->setObjectName("playlistSecondaryButton");
    toolbar->addWidget(selectionCount);
    toolbar->addStretch();
    toolbar->addWidget(selectAllButton);
    toolbar->addWidget(clearButton);
    layout->addLayout(toolbar);

    auto *list = new QListWidget(&dialog);
    list->setObjectName("playlistItems");
    list->setAlternatingRowColors(false);
    list->setSelectionMode(QAbstractItemView::NoSelection);
    list->setSpacing(1);
    QList<QCheckBox *> itemChecks;
    for (int index = 0; index < items.size(); ++index) {
        const PlaylistItem item = items.at(index);
        auto *listItem = new QListWidgetItem(list);
        listItem->setToolTip(item.url.toString());
        listItem->setSizeHint(QSize(0, 44));

        auto *row = new QWidget(list);
        row->setObjectName("playlistItemRow");
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(4, 0, 8, 0);
        rowLayout->setSpacing(4);

        auto *check = new QCheckBox(row);
        check->setObjectName("playlistItemCheck");
        check->setChecked(true);
        auto *itemTitle = new QPushButton(QString("%1. %2").arg(index + 1).arg(item.title), row);
        itemTitle->setObjectName("playlistItemTitle");
        itemTitle->setCursor(Qt::PointingHandCursor);
        itemTitle->setToolTip("Ver informações deste vídeo");
        rowLayout->addWidget(check);
        rowLayout->addWidget(itemTitle, 1);
        list->setItemWidget(listItem, row);
        itemChecks.append(check);

        connect(itemTitle, &QPushButton::clicked, &dialog, [this, item]() {
            showPlaylistItemDetailsDialog(item);
        });
    }
    layout->addWidget(list, 1);

    auto *footer = new QHBoxLayout();
    auto *cancelButton = new QPushButton("Cancelar", &dialog);
    cancelButton->setObjectName("playlistSecondaryButton");
    auto *confirmButton = new QPushButton("ADICIONAR SELECIONADOS", &dialog);
    confirmButton->setObjectName("playlistConfirmButton");
    confirmButton->setMinimumHeight(42);
    footer->addStretch();
    footer->addWidget(cancelButton);
    footer->addWidget(confirmButton);
    layout->addLayout(footer);

    const auto updateSelectionState = [itemChecks, selectionCount, confirmButton, total = items.size()]() {
        int selectedCount = 0;
        for (QCheckBox *check : itemChecks) {
            if (check->isChecked()) {
                ++selectedCount;
            }
        }
        selectionCount->setText(QString("%1 de %2 selecionado(s)").arg(selectedCount).arg(total));
        confirmButton->setEnabled(selectedCount > 0);
    };

    connect(selectAllButton, &QPushButton::clicked, &dialog, [itemChecks, updateSelectionState]() {
        for (QCheckBox *check : itemChecks) {
            check->setChecked(true);
        }
        updateSelectionState();
    });
    connect(clearButton, &QPushButton::clicked, &dialog, [itemChecks, updateSelectionState]() {
        for (QCheckBox *check : itemChecks) {
            check->setChecked(false);
        }
        updateSelectionState();
    });
    for (QCheckBox *check : itemChecks) {
        connect(check, &QCheckBox::toggled, &dialog, updateSelectionState);
    }
    connect(searchInput, &QLineEdit::textChanged, &dialog, [list, items](const QString &filter) {
        for (int index = 0; index < list->count(); ++index) {
            list->item(index)->setHidden(!items.at(index).title.contains(filter, Qt::CaseInsensitive));
        }
    });
    connect(confirmButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    updateSelectionState();

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    for (int index = 0; index < itemChecks.size(); ++index) {
        if (itemChecks.at(index)->isChecked()) {
            selectedItems.append(items.at(index));
        }
    }
    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, "Nenhum item selecionado",
                                 "Selecione pelo menos um vídeo para adicionar à fila.");
        return false;
    }
    return true;
}

void MainWindow::showPlaylistItemDetailsDialog(const PlaylistItem &item)
{
    QDialog dialog(this);
    dialog.setObjectName("playlistDetailsDialog");
    dialog.setWindowTitle("Informações do vídeo - Prism Studio Suite");
    dialog.resize(580, 520);
    dialog.setMinimumSize(580, 500);
    dialog.setStyleSheet(this->styleSheet() + R"(
        QDialog#playlistDetailsDialog { background-color: #151515; }
        QLabel#playlistThumbnail {
            background-color: #202020;
            border: 1px solid #31483d;
            border-radius: 9px;
            color: #a3a3a3;
        }
        QLabel#playlistDetailsTitle { color: #ffffff; font-size: 18px; font-weight: bold; }
        QLabel#playlistDetailsMeta { color: #10b981; font-size: 14px; font-weight: bold; }
        QLabel#playlistDetailsUrl { color: #a3a3a3; font-size: 11px; }
        QPushButton#playlistDetailsClose {
            background-color: #10b981;
            color: #021810;
            border: none;
            border-radius: 6px;
            padding: 10px 18px;
            font-weight: bold;
        }
        QPushButton#playlistDetailsClose:hover { background-color: #059669; }
    )");

    auto *layout = new QVBoxLayout(&dialog);
    layout->setSpacing(14);
    layout->setContentsMargins(22, 20, 22, 20);

    auto *thumbnail = new QLabel("Carregando miniatura...", &dialog);
    thumbnail->setObjectName("playlistThumbnail");
    thumbnail->setAlignment(Qt::AlignCenter);
    thumbnail->setFixedSize(536, 300);
    layout->addWidget(thumbnail, 0, Qt::AlignHCenter);

    auto *title = new QLabel(item.title, &dialog);
    title->setObjectName("playlistDetailsTitle");
    title->setWordWrap(true);
    layout->addWidget(title);

    const QString duration = item.duration.isEmpty() || item.duration == "NA"
        ? "Duração não informada pela playlist"
        : item.duration;
    auto *durationLabel = new QLabel("Duração: " + duration, &dialog);
    durationLabel->setObjectName("playlistDetailsMeta");
    layout->addWidget(durationLabel);

    auto *urlLabel = new QLabel(item.url.toString(QUrl::FullyEncoded), &dialog);
    urlLabel->setObjectName("playlistDetailsUrl");
    urlLabel->setWordWrap(true);
    layout->addWidget(urlLabel);

    auto *closeButton = new QPushButton("FECHAR", &dialog);
    closeButton->setObjectName("playlistDetailsClose");
    closeButton->setMinimumHeight(40);
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(closeButton, 0, Qt::AlignRight);

    if (!item.thumbnailUrl.isValid() || item.thumbnailUrl.host().isEmpty()
        || (item.thumbnailUrl.scheme() != "http" && item.thumbnailUrl.scheme() != "https")) {
        thumbnail->setText("Miniatura não disponível para este vídeo.");
    } else if (m_thumbnailNetwork) {
        QNetworkRequest request(item.thumbnailUrl);
        request.setHeader(QNetworkRequest::UserAgentHeader,
                          "PrismDownloader/1.1 (playlist details)");
        QNetworkReply *reply = m_thumbnailNetwork->get(request);
        auto imageData = std::make_shared<QByteArray>();
        const QPointer<QLabel> thumbnailGuard = thumbnail;
        connect(reply, &QIODevice::readyRead, reply, [reply, imageData]() {
            constexpr qsizetype kMaximumThumbnailBytes = 5 * 1024 * 1024;
            const QByteArray chunk = reply->readAll();
            if (imageData->size() > kMaximumThumbnailBytes - chunk.size()) {
                reply->abort();
                return;
            }
            imageData->append(chunk);
        });
        connect(reply, &QNetworkReply::finished, reply,
                [reply, imageData, thumbnailGuard]() {
            QPixmap image;
            if (thumbnailGuard) {
                if (reply->error() == QNetworkReply::NoError && image.loadFromData(*imageData)) {
                    thumbnailGuard->setPixmap(image.scaled(thumbnailGuard->size(), Qt::KeepAspectRatio,
                                                           Qt::SmoothTransformation));
                } else {
                    thumbnailGuard->setText("Não foi possível carregar a miniatura.");
                }
            }
            reply->deleteLater();
        });
        connect(&dialog, &QDialog::finished, reply, &QNetworkReply::abort);
        QTimer::singleShot(10000, reply, [reply]() {
            if (reply->isRunning()) {
                reply->abort();
            }
        });
    } else {
        thumbnail->setText("Miniatura não disponível para este vídeo.");
    }

    dialog.exec();
}

void MainWindow::continueDownload(const QList<PlaylistItem> &items)
{
    if (items.isEmpty()) {
        return;
    }

    QString selectedQuality, timeRange, convertFormat, customOutputDir;
    bool doConvert = false;
    if (!showFormatSelectionDialog(selectedQuality, timeRange, doConvert, convertFormat, customOutputDir)) {
        logMessage("[Operação] Seleção de formato cancelada pelo usuário.");
        return;
    }
    if (!isValidTimeRange(timeRange)) {
        QMessageBox::warning(this, "Recorte inválido",
                             "Use o formato HH:MM:SS-HH:MM:SS, com início menor que o fim.");
        return;
    }
    if (customOutputDir.isEmpty()
        || (!QDir(customOutputDir).exists() && !QDir().mkpath(customOutputDir))) {
        QMessageBox::critical(this, "Pasta inválida",
                              "Não foi possível criar ou acessar a pasta de destino selecionada.");
        return;
    }

    m_currentDownloadDir = customOutputDir;
    const QString defaultOutputDir = m_outputDirInput->text().trimmed();
    QSettings settings("Tonho Studios", "PrismDownloader");
    settings.setValue("outputFolder", defaultOutputDir);
    settings.setValue("showNotifications", m_notifyCheckBox->isChecked());
    settings.setValue("selectedQuality", m_qualityCombo->currentIndex());
    settings.setValue("defaultTimeRange", timeRange);

    int addedCount = 0;
    QStringList rejectedItems;
    for (const auto &item : items) {
        DownloadRequest request;
        request.url = item.url;
        request.quality = selectedQuality;
        request.timeRange = timeRange;
        request.outputDirectory = customOutputDir;

        const EnqueueResult result = m_downloadManager->enqueueDownload(request);
        if (!result.accepted) {
            rejectedItems.append(item.title + ": " + result.error);
            logMessage("[Fila] Item recusado: " + result.error);
            continue;
        }

        UiDownloadJob uiJob;
        uiJob.request = request;
        uiJob.autoConvert = doConvert;
        uiJob.conversionFormat = convertFormat;
        m_downloadJobs.insert(result.id, uiJob);
        m_currentBatchJobs.insert(result.id);

        if (m_downloadsQueueTable) {
            m_downloadsQueueTable->insertRow(0);
            for (int column = 0; column < m_downloadsQueueTable->columnCount(); ++column) {
                auto *cell = new QTableWidgetItem;
                cell->setTextAlignment(column == 0 ? Qt::AlignLeft | Qt::AlignVCenter
                                                   : Qt::AlignCenter);
                m_downloadsQueueTable->setItem(0, column, cell);
            }
            m_downloadsQueueTable->item(0, 0)->setData(
                Qt::UserRole, QVariant::fromValue<qulonglong>(result.id));
            m_downloadRowItems.insert(result.id, m_downloadsQueueTable->item(0, 0));
            updateJobRow(result.id);
            m_downloadsQueueTable->selectRow(0);
        }

        ++addedCount;
        const QString itemLabel = item.title.isEmpty() ? item.url.toString() : item.title;
        logMessage(QString("[Fila] Download #%1 adicionado: %2")
                       .arg(result.id).arg(itemLabel));
        if (!timeRange.isEmpty()) {
            logMessage(QString("[Download #%1] Recorte programado: %2")
                           .arg(result.id).arg(timeRange));
        }
        if (doConvert) {
            logMessage(QString("[Download #%1] Conversão automática programada: %2")
                           .arg(result.id).arg(convertFormat));
        }
    }

    if (addedCount == 0) {
        QMessageBox::warning(this, "Nenhum item adicionado",
                             "Nenhum vídeo foi aceito pela fila de downloads.");
        return;
    }
    if (!rejectedItems.isEmpty()) {
        logMessage(QString("[Playlist] %1 item(ns) não foram adicionados.").arg(rejectedItems.size()));
    }

    m_urlInput->clear();
    onDownloadQueueStateChanged(m_downloadManager->activeCount(), m_downloadManager->pendingCount());
    showDownloadsPage();
}

void MainWindow::onConvertBrowseClicked()
{
    QString file = QFileDialog::getOpenFileName(this, "Selecione a Mídia para Converter", m_outputDirInput->text(),
                                                "Arquivos de Mídia (*.mp4 *.mkv *.avi *.mov *.webm *.flv *.mp3 *.m4a *.wav);;Todos os Arquivos (*.*)");
    if (!file.isEmpty()) {
        m_convertInput->setText(file);
        m_convertStatusLabel->setText("Status do Conversor: Arquivo selecionado! Pronto para converter.");
        logMessage("[Conversor] Arquivo de origem selecionado: " + file);
    }
}

void MainWindow::onStartConvertClicked()
{
    const QString inFile = m_convertInput->text().trimmed();
    if (inFile.isEmpty() || !QFile::exists(inFile)) {
        QMessageBox::warning(this, "Atenção", "Selecione um arquivo de mídia existente no computador para converter.");
        return;
    }

    const QString outFolder = m_outputDirInput->text().trimmed();
    if (outFolder.isEmpty() || (!QDir(outFolder).exists() && !QDir().mkpath(outFolder))) {
        QMessageBox::critical(this, "Erro", "Não foi possível criar ou acessar a pasta de destino da conversão.");
        return;
    }
    m_currentDownloadDir = QDir(outFolder).absolutePath();

    ConversionRequest request;
    request.inputFile = inFile;
    request.format = m_convertFormatCombo->currentText();
    request.outputDirectory = outFolder;
    request.gpuType = m_gpuDetector.getGPUType();
    if (m_gpuDetector.hasHardwareAcceleration()) {
        request.gpuCodec = QString::fromStdString(m_gpuDetector.getRecommendedCodec());
        request.gpuDevice = QString::fromStdString(m_gpuDetector.getHardwareDevice());
    }

    const ConversionEnqueueResult result = m_conversionManager->enqueueConversion(request);
    if (!result.accepted) {
        QMessageBox::critical(this, "Não foi possível converter", result.error);
        return;
    }

    m_manualConversionId = result.id;

    m_convertProgressBar->setRange(0, 0);
    m_convertStatusLabel->setText("Status: Aguardando conversão...");
    m_startConvertBtn->setEnabled(false);
    m_cancelConvertBtn->setEnabled(true);
}

void MainWindow::onCancelConvertClicked()
{
    if (m_manualConversionId != 0) {
        m_conversionManager->cancelConversion(m_manualConversionId);
    }
}

void MainWindow::refreshLibrary()
{
    if (!m_libraryTable || !m_stackedWidget || m_stackedWidget->currentIndex() != 1) return;
    stopLibraryThumbnailProcesses();
    m_libraryTable->setRowCount(0);
    if (m_libraryBlocks) {
        m_libraryBlocks->clear();
    }

    QString folder = m_currentDownloadDir.isEmpty() ? m_outputDirInput->text() : m_currentDownloadDir;
    QDir dir(folder);
    if (!dir.exists()) return;

    QStringList filters;
    filters << "*.mp4" << "*.mp3" << "*.mkv" << "*.webm" << "*.m4a" << "*.avi" << "*.flv" << "*.wav";
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks, QDir::Time);

    if (m_libraryViewStack && m_libraryViewStack->currentIndex() == 1) {
        refreshLibraryBlocks(fileList);
    } else {
        m_libraryTable->setRowCount(fileList.size());
        for (int i = 0; i < fileList.size(); ++i) {
            const QFileInfo &info = fileList.at(i);

            auto *itemTitle = new QTableWidgetItem(info.fileName());
            itemTitle->setFlags(itemTitle->flags() ^ Qt::ItemIsEditable);
            itemTitle->setData(Qt::UserRole, info.absoluteFilePath());

            auto *itemExt = new QTableWidgetItem(info.suffix().toUpper());
            itemExt->setFlags(itemExt->flags() ^ Qt::ItemIsEditable);
            itemExt->setTextAlignment(Qt::AlignCenter);

            const double sizeMB = static_cast<double>(info.size()) / (1024.0 * 1024.0);
            auto *itemSize = new QTableWidgetItem(QString("%1 MB").arg(sizeMB, 0, 'f', 1));
            itemSize->setFlags(itemSize->flags() ^ Qt::ItemIsEditable);
            itemSize->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

            m_libraryTable->setItem(i, 0, itemTitle);
            m_libraryTable->setItem(i, 1, itemExt);
            m_libraryTable->setItem(i, 2, itemSize);
        }
    }
    logMessage(QString("[Biblioteca] Lista de mídias atualizada: %1 arquivo(s) encontrado(s).").arg(fileList.size()));
}

void MainWindow::setLibraryViewMode(bool blocks)
{
    if (!m_libraryViewStack) {
        return;
    }
    m_libraryViewStack->setCurrentIndex(blocks ? 1 : 0);
    if (m_libraryListViewBtn) {
        m_libraryListViewBtn->setChecked(!blocks);
    }
    if (m_libraryBlocksViewBtn) {
        m_libraryBlocksViewBtn->setChecked(blocks);
    }
    refreshLibrary();
}

void MainWindow::refreshLibraryBlocks(const QFileInfoList &fileList)
{
    if (!m_libraryBlocks) {
        return;
    }

    for (const QFileInfo &info : fileList) {
        auto *item = new QListWidgetItem(m_libraryBlocks);
        item->setData(Qt::UserRole, info.absoluteFilePath());
        item->setSizeHint(QSize(208, 194));

        auto *card = new QWidget(m_libraryBlocks);
        card->setObjectName("libraryCard");
        card->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(5, 5, 5, 5);
        cardLayout->setSpacing(5);

        auto *thumbnail = new QLabel("Gerando miniatura...", card);
        thumbnail->setObjectName("libraryCardThumb");
        thumbnail->setAlignment(Qt::AlignCenter);
        thumbnail->setFixedSize(198, 112);
        thumbnail->setWordWrap(true);
        cardLayout->addWidget(thumbnail, 0, Qt::AlignHCenter);

        auto *title = new QLabel(info.fileName(), card);
        title->setObjectName("libraryCardTitle");
        title->setWordWrap(true);
        title->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        title->setMaximumHeight(38);
        title->setToolTip(info.fileName());
        cardLayout->addWidget(title);

        const double sizeMB = static_cast<double>(info.size()) / (1024.0 * 1024.0);
        auto *metadata = new QLabel(QString("%1  •  %2 MB")
                                         .arg(info.suffix().toUpper())
                                         .arg(sizeMB, 0, 'f', 1), card);
        metadata->setObjectName("libraryCardMeta");
        cardLayout->addWidget(metadata);

        m_libraryBlocks->setItemWidget(item, card);
        loadLibraryThumbnail(info, thumbnail);
    }
}

void MainWindow::loadLibraryThumbnail(const QFileInfo &fileInfo, QLabel *thumbnailLabel)
{
    if (!thumbnailLabel) {
        return;
    }

    const QString path = fileInfo.absoluteFilePath();
    const QString extension = fileInfo.suffix().toLower();
    const QStringList videoExtensions{
        QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("webm"),
        QStringLiteral("avi"), QStringLiteral("flv"), QStringLiteral("mov"),
        QStringLiteral("m4v"), QStringLiteral("wmv")};
    if (!videoExtensions.contains(extension)) {
        thumbnailLabel->setText(QString("ÁUDIO\n%1").arg(extension.toUpper()));
        return;
    }

    const auto cached = m_libraryThumbnailCache.constFind(path);
    if (cached != m_libraryThumbnailCache.cend()) {
        thumbnailLabel->setPixmap(cached.value().scaled(thumbnailLabel->size(),
                                                         Qt::KeepAspectRatio,
                                                         Qt::SmoothTransformation));
        return;
    }

    const QString ffmpeg = MediaToolResolver::resolve(MediaTool::Ffmpeg);
    if (ffmpeg.isEmpty() || !QFileInfo(ffmpeg).isFile()) {
        thumbnailLabel->setText("Miniatura indisponível\n(FFmpeg não encontrado)");
        return;
    }

    auto *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::SeparateChannels);
    m_libraryThumbnailProcesses.insert(process);
    auto output = std::make_shared<QByteArray>();
    const QPointer<QLabel> labelGuard = thumbnailLabel;

    connect(process, &QProcess::readyReadStandardOutput, process, [process, output]() {
        const QByteArray chunk = process->readAllStandardOutput();
        if (output->size() > kMaximumLibraryThumbnailBytes - chunk.size()) {
            process->kill();
            return;
        }
        output->append(chunk);
    });
    connect(process, &QProcess::readyReadStandardError, process, [process]() {
        process->readAllStandardError();
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this, process, output, labelGuard, path](int exitCode, QProcess::ExitStatus status) {
        m_libraryThumbnailProcesses.remove(process);
        if (labelGuard && status == QProcess::NormalExit && exitCode == 0) {
            QPixmap image;
            if (image.loadFromData(*output)) {
                if (m_libraryThumbnailCache.size() >= 64) {
                    m_libraryThumbnailCache.erase(m_libraryThumbnailCache.begin());
                }
                m_libraryThumbnailCache.insert(path, image);
                labelGuard->setPixmap(image.scaled(labelGuard->size(), Qt::KeepAspectRatio,
                                                   Qt::SmoothTransformation));
            } else {
                labelGuard->setText("Miniatura indisponível");
            }
        } else if (labelGuard) {
            labelGuard->setText("Miniatura indisponível");
        }
        process->deleteLater();
    });
    process->start(ffmpeg, {
        QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-ss"), QStringLiteral("1"), QStringLiteral("-i"), path,
        QStringLiteral("-frames:v"), QStringLiteral("1"),
        QStringLiteral("-vf"), QStringLiteral("scale=320:180:force_original_aspect_ratio=decrease"),
        QStringLiteral("-f"), QStringLiteral("image2pipe"),
        QStringLiteral("-vcodec"), QStringLiteral("mjpeg"), QStringLiteral("-q:v"), QStringLiteral("5"),
        QStringLiteral("pipe:1")});
}

void MainWindow::stopLibraryThumbnailProcesses()
{
    const QSet<QProcess *> processes = m_libraryThumbnailProcesses;
    m_libraryThumbnailProcesses.clear();
    for (QProcess *process : processes) {
        if (!process) {
            continue;
        }
        disconnect(process, nullptr, this, nullptr);
        process->kill();
        process->deleteLater();
    }
}

void MainWindow::onPlaySelectedMedia()
{
    if (m_libraryViewStack && m_libraryViewStack->currentIndex() == 1) {
        const QListWidgetItem *item = m_libraryBlocks ? m_libraryBlocks->currentItem() : nullptr;
        if (!item) {
            QMessageBox::information(this, "Biblioteca", "Selecione um bloco de mídia para reproduzir.");
            return;
        }
        openLibraryFile(item->data(Qt::UserRole).toString());
        return;
    }
    int row = m_libraryTable->currentRow();
    if (row < 0) {
        QMessageBox::information(this, "Biblioteca", "Selecione um arquivo de mídia da lista acima para reproduzir.");
        return;
    }
    onLibraryDoubleClicked(row, 0);
}

void MainWindow::onLibraryDoubleClicked(int row, int /*column*/)
{
    QTableWidgetItem *item = m_libraryTable->item(row, 0);
    if (!item) return;

    QString fileName = item->text();
    QString filePath = item->data(Qt::UserRole).toString();
    if (filePath.isEmpty()) {
        QDir dir(m_outputDirInput->text());
        filePath = dir.absoluteFilePath(fileName);
    }

    openLibraryFile(filePath);
}

void MainWindow::onLibraryBlockDoubleClicked(QListWidgetItem *item)
{
    if (item) {
        openLibraryFile(item->data(Qt::UserRole).toString());
    }
}

void MainWindow::openLibraryFile(const QString &filePath)
{
    if (QFile::exists(filePath)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
        logMessage("[Biblioteca] Abrindo arquivo no player padrão do sistema: "
                   + QFileInfo(filePath).fileName());
    } else {
        QMessageBox::warning(this, "Aviso", "O arquivo selecionado não foi encontrado fisicamente no disco:\n" + filePath);
        refreshLibrary();
    }
}

void MainWindow::onDownloadQueueDoubleClicked(int row, int /*column*/)
{
    if (!m_downloadsQueueTable) return;
    QTableWidgetItem *item = m_downloadsQueueTable->item(row, 0);
    if (!item) return;

    const QString fileName = item->text();
    const QString filePath = item->data(Qt::UserRole + 1).toString();

    if (!filePath.isEmpty() && QFile::exists(filePath)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
        logMessage("[Fila de Downloads] Abrindo vídeo no player padrão do sistema: " + fileName);
    } else {
        QMessageBox::information(this, "Aguarde", "Este item ainda não possui um arquivo final disponível.");
    }
}

// ==========================================
// DIÁLOGO MODAL DE SELEÇÃO DE FORMATO
// ==========================================
bool MainWindow::showFormatSelectionDialog(QString &outQuality, QString &outTimeRange, bool &outDoConvert, QString &outConvertFormat, QString &outCustomOutputDir)
{
    QDialog dlg(this);
    dlg.setWindowTitle("Selecione o formato da fonte - Prism Studio Suite");
    dlg.resize(780, 580);
    dlg.setStyleSheet(this->styleSheet() + "QDialog { background-color: #1a1a1a; }");

    QVBoxLayout *dlgLayout = new QVBoxLayout(&dlg);
    dlgLayout->setSpacing(16);
    dlgLayout->setContentsMargins(24, 24, 24, 24);

    QLabel *lblTitle = new QLabel("Selecione o formato da fonte e opções do download:", &dlg);
    lblTitle->setStyleSheet("font-weight: bold; font-size: 18px; color: #ffffff;");
    dlgLayout->addWidget(lblTitle);

    QTableWidget *table = new QTableWidget(4, 3, &dlg);
    QStringList headers;
    headers << "Título / Qualidade" << "Formato e Codec" << "Resolução / Modo";
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->setObjectName("libraryTable");
    table->setMinimumHeight(180);

    struct Prof { QString q; QString f; QString r; };
    Prof profs[4] = {
        {"4K / Melhor Disponível no Servidor", "MP4 / Container Original", "Ultra HD / Máxima"},
        {"1080p Full HD", "H.264 / Aceleração quando disponível", "1920x1080 (60/30 fps)"},
        {"720p HD", "H.264 / Aceleração quando disponível", "1280x720 (Balanceado)"},
        {"Áudio MP3 (Extração Direta)", "MP3 Estéreo Alta Fidelidade", "320 kbps (Apenas Áudio)"}
    };

    for (int i = 0; i < 4; ++i) {
        table->setItem(i, 0, new QTableWidgetItem(profs[i].q));
        table->setItem(i, 1, new QTableWidgetItem(profs[i].f));
        table->setItem(i, 2, new QTableWidgetItem(profs[i].r));
    }

    int defaultIdx = m_qualityCombo->currentIndex();
    if (defaultIdx >= 0 && defaultIdx < 4) {
        table->selectRow(defaultIdx);
    } else {
        table->selectRow(1);
    }

    dlgLayout->addWidget(table);

    // Opções de tempo, conversão e destino personalizado no modal
    QGridLayout *optLayout = new QGridLayout();
    optLayout->setSpacing(12);
    
    QLabel *lblTimeOpt = new QLabel("Recorte de Tempo (Opcional):", &dlg);
    lblTimeOpt->setStyleSheet("color: #a3a3a3; font-weight: bold;");
    QLineEdit *editTime = new QLineEdit(&dlg);
    editTime->setText(m_timeRangeInput->text());
    editTime->setPlaceholderText("Ex: 00:01:15-00:03:00 (Vazio = baixar completo)");

    QCheckBox *chkConv = new QCheckBox("Converter para outro formato após concluir o download", &dlg);
    chkConv->setStyleSheet("color: #38bdf8; font-weight: bold; font-size: 13px;");
    chkConv->setCursor(Qt::PointingHandCursor);
    
    QComboBox *comboConv = new QComboBox(&dlg);
    comboConv->addItem("MP4 (H.264 / Aceleração quando disponível)");
    comboConv->addItem("MP4 (HEVC / H.265 - Compressão de Alta Densidade)");
    comboConv->addItem("MKV (Matroska - Container Sem Perdas)");
    comboConv->addItem("MP3 (Áudio MP3 Alta Fidelidade - 320kbps)");
    comboConv->addItem("WAV (Áudio Sem Compressão / Estúdios)");
    comboConv->addItem("WEBM (Otimizado para Web e Redes Sociais)");
    comboConv->setEnabled(false);
    connect(chkConv, &QCheckBox::toggled, comboConv, &QComboBox::setEnabled);

    QLabel *lblFolderOpt = new QLabel("Salvar este download em:", &dlg);
    lblFolderOpt->setStyleSheet("color: #a3a3a3; font-weight: bold;");
    
    QHBoxLayout *folderOptLayout = new QHBoxLayout();
    QLineEdit *editCustomFolder = new QLineEdit(&dlg);
    editCustomFolder->setText(m_outputDirInput->text());
    
    QPushButton *btnChangeCustomFolder = new QPushButton("Mudar Pasta (Apenas Este)", &dlg);
    btnChangeCustomFolder->setObjectName("browseBtn");
    btnChangeCustomFolder->setCursor(Qt::PointingHandCursor);
    btnChangeCustomFolder->setMinimumHeight(32);
    connect(btnChangeCustomFolder, &QPushButton::clicked, &dlg, [this, editCustomFolder]() {
        QString dir = QFileDialog::getExistingDirectory(nullptr, "Escolha a Pasta Exclusiva Para Este Download", editCustomFolder->text());
        if (!dir.isEmpty()) {
            editCustomFolder->setText(dir);
        }
    });

    folderOptLayout->addWidget(editCustomFolder, 1);
    folderOptLayout->addWidget(btnChangeCustomFolder, 0);

    optLayout->addWidget(lblTimeOpt, 0, 0);
    optLayout->addWidget(editTime, 0, 1);
    optLayout->addWidget(chkConv, 1, 0);
    optLayout->addWidget(comboConv, 1, 1);
    optLayout->addWidget(lblFolderOpt, 2, 0);
    optLayout->addLayout(folderOptLayout, 2, 1);
    optLayout->setColumnStretch(1, 1);

    dlgLayout->addLayout(optLayout);
    dlgLayout->addStretch();

    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *btnOk = new QPushButton("ADICIONAR À FILA", &dlg);
    btnOk->setObjectName("startBtn");
    btnOk->setCursor(Qt::PointingHandCursor);
    btnOk->setMinimumHeight(42);
    btnOk->setFixedWidth(185);
    btnOk->setStyleSheet("font-size: 15px; font-weight: bold;");
    connect(btnOk, &QPushButton::clicked, &dlg, &QDialog::accept);

    QPushButton *btnCancel = new QPushButton("CANCELAR", &dlg);
    btnCancel->setObjectName("cancelBtn");
    btnCancel->setCursor(Qt::PointingHandCursor);
    btnCancel->setMinimumHeight(42);
    btnCancel->setFixedWidth(140);
    connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    const QString accelerationStatus = m_gpuDetector.hasHardwareAcceleration()
        ? "⚡ Aceleração disponível: "
            + QString::fromStdString(m_gpuDetector.getRecommendedCodec()).toUpper()
        : "ℹ️ Conversão será feita pela CPU";
    QLabel *lblAccel = new QLabel(accelerationStatus, &dlg);
    lblAccel->setStyleSheet(m_gpuDetector.hasHardwareAcceleration()
                                ? "color: #10b981; font-weight: bold; font-size: 13px;"
                                : "color: #f59e0b; font-weight: bold; font-size: 13px;");

    btnLayout->addWidget(btnOk);
    btnLayout->addWidget(btnCancel);
    btnLayout->addStretch();
    btnLayout->addWidget(lblAccel);
    dlgLayout->addLayout(btnLayout);

    if (dlg.exec() == QDialog::Accepted) {
        int r = table->currentRow();
        if (r >= 0 && r < 4) {
            outQuality = m_qualityCombo->itemText(r);
            m_qualityCombo->setCurrentIndex(r);
        } else {
            outQuality = m_qualityCombo->currentText();
        }
        outTimeRange = editTime->text().trimmed();
        m_timeRangeInput->setText(outTimeRange);
        
        outDoConvert = chkConv->isChecked();
        outConvertFormat = comboConv->currentText();
        outCustomOutputDir = editCustomFolder->text().trimmed();
        if (outCustomOutputDir.isEmpty()) {
            outCustomOutputDir = m_outputDirInput->text().trimmed();
        }
        return true;
    }
    return false;
}

void MainWindow::onBrowseClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Escolha a Pasta de Destino Padrão para os Downloads", m_outputDirInput->text());
    if (!dir.isEmpty()) {
        m_outputDirInput->setText(dir);
        m_currentDownloadDir.clear();
        QSettings settings("Tonho Studios", "PrismDownloader");
        settings.setValue("outputFolder", dir);
        logMessage("[System] Nova pasta de destino padrão salva: " + dir);
        refreshLibrary();
    }
}

void MainWindow::onOpenFolderClicked()
{
    QString dir = m_currentDownloadDir.isEmpty() ? m_outputDirInput->text() : m_currentDownloadDir;
    if (!dir.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
        logMessage("[System] Abrindo a pasta no gerenciador de arquivos: " + dir);
    }
}

void MainWindow::onStartClicked()
{
    const QString url = m_urlInput->text().trimmed();
    if (url.isEmpty()) {
        QMessageBox::warning(this, "Atenção", "Por favor, insira ou cole o link do vídeo na caixa de URL antes de prosseguir.");
        return;
    }
    const QUrl parsedUrl(url);
    if (!parsedUrl.isValid() || parsedUrl.host().isEmpty()
        || (parsedUrl.scheme() != "https" && parsedUrl.scheme() != "http")) {
        QMessageBox::warning(this, "URL inválida", "Informe uma URL HTTP ou HTTPS completa e válida.");
        return;
    }

    QUrlQuery query(parsedUrl);
    const QString path = parsedUrl.path().toLower();
    const bool looksLikePlaylist = query.hasQueryItem("list")
        || query.hasQueryItem("playlist")
        || path.contains("/playlist")
        || path.contains("/sets/");
    if (looksLikePlaylist) {
        startPlaylistPreview(parsedUrl);
        return;
    }

    QList<PlaylistItem> items;
    items.append({{}, parsedUrl, {}, {}});
    continueDownload(items);
}

void MainWindow::onCancelClicked()
{
    const DownloadId id = selectedDownloadId();
    if (id == 0) {
        return;
    }
    m_downloadManager->cancelDownload(id);
    m_conversionManager->cancelByDownloadId(id);
}

void MainWindow::onCancelAllClicked()
{
    logMessage("[Fila] Cancelamento de todos os downloads solicitado.");
    m_downloadManager->cancelAll();
    m_conversionManager->cancelAllAutomatic();
}

void MainWindow::onConcurrencyChanged(int value)
{
    m_downloadManager->setConcurrencyLimit(value);
    QSettings("Tonho Studios", "PrismDownloader").setValue("maxConcurrentDownloads", value);
}

int MainWindow::findDownloadRow(DownloadId id) const
{
    if (!m_downloadsQueueTable || id == 0) {
        return -1;
    }
    const QTableWidgetItem *item = m_downloadRowItems.value(id, nullptr);
    return item ? item->row() : -1;
}

DownloadId MainWindow::selectedDownloadId() const
{
    if (!m_downloadsQueueTable || m_downloadsQueueTable->currentRow() < 0) {
        return 0;
    }
    const QTableWidgetItem *item = m_downloadsQueueTable->item(m_downloadsQueueTable->currentRow(), 0);
    return item ? item->data(Qt::UserRole).toULongLong() : 0;
}

void MainWindow::updateJobRow(DownloadId id)
{
    const int row = findDownloadRow(id);
    auto iterator = m_downloadJobs.find(id);
    if (row < 0 || iterator == m_downloadJobs.end()) {
        return;
    }
    UiDownloadJob &job = iterator.value();
    const QFileInfo file(job.filePath);
    const QString title = file.exists() ? file.fileName() : job.request.url.toString();
    const QString format = file.exists() ? file.suffix().toUpper() : job.request.quality;
    const QString speedEta = (job.speed.isEmpty() ? "--" : job.speed)
        + " / " + (job.eta.isEmpty() ? "--:--" : job.eta);
    const QString size = file.exists()
        ? QString("%1 MB").arg(static_cast<double>(file.size()) / (1024.0 * 1024.0), 0, 'f', 1)
        : "Calculando...";

    m_downloadsQueueTable->item(row, 0)->setText(title);
    m_downloadsQueueTable->item(row, 0)->setData(Qt::UserRole + 1, job.filePath);
    m_downloadsQueueTable->item(row, 1)->setText(format);
    m_downloadsQueueTable->item(row, 2)->setText(QString::number(job.progress, 'f', 1) + "%");
    m_downloadsQueueTable->item(row, 3)->setText(speedEta);
    m_downloadsQueueTable->item(row, 4)->setText(size);
    m_downloadsQueueTable->item(row, 5)->setText(job.statusText);

    QColor color("#38bdf8");
    if (job.status == DownloadStatus::Completed) color = QColor("#10b981");
    if (job.status == DownloadStatus::Error) color = QColor("#ef4444");
    if (job.status == DownloadStatus::Cancelled) color = QColor("#f59e0b");
    m_downloadsQueueTable->item(row, 5)->setForeground(color);
}

void MainWindow::onQueueSelectionChanged()
{
    const DownloadId id = selectedDownloadId();
    const auto iterator = m_downloadJobs.constFind(id);
    m_cancelBtn->setEnabled(iterator != m_downloadJobs.cend() && !iterator->terminal);
    updateSelectedMonitor();
}

void MainWindow::onDownloadProgress(DownloadId id, double percent, const QString &speed, const QString &eta)
{
    auto iterator = m_downloadJobs.find(id);
    if (iterator == m_downloadJobs.end()) return;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    iterator->progress = qBound(0.0, percent, 100.0);
    iterator->speed = speed;
    iterator->eta = eta;
    if (iterator->lastUiRefreshMs != 0 && nowMs - iterator->lastUiRefreshMs < 100
        && iterator->progress < 100.0) {
        return;
    }
    iterator->lastUiRefreshMs = nowMs;
    updateJobRow(id);
    updateSelectedMonitor();
}

void MainWindow::onDownloadStatus(DownloadId id, DownloadStatus status, const QString &message)
{
    auto iterator = m_downloadJobs.find(id);
    if (iterator == m_downloadJobs.end()) return;
    UiDownloadJob &job = iterator.value();

    if (status == DownloadStatus::Completed && job.autoConvert && job.conversionId != 0 && !job.terminal) {
        job.status = DownloadStatus::ConvertingGPU;
        job.statusText = "Aguardando conversão";
    } else if (!job.terminal || status == DownloadStatus::Cancelled) {
        job.status = status;
        job.statusText = message;
        if (status == DownloadStatus::Completed) {
            job.progress = 100.0;
            job.terminal = true;
        } else if (status == DownloadStatus::Error || status == DownloadStatus::Cancelled) {
            job.terminal = true;
        }
    }

    updateJobRow(id);
    onQueueSelectionChanged();
    maybeShowQueueSummary();
}

void MainWindow::onDownloadCompleted(DownloadId id, const QString &filePath)
{
    auto iterator = m_downloadJobs.find(id);
    if (iterator == m_downloadJobs.end()) return;
    UiDownloadJob &job = iterator.value();
    job.filePath = filePath;
    job.progress = 100.0;
    m_currentDownloadDir = QFileInfo(filePath).absolutePath();

    if (job.autoConvert) {
        ConversionRequest request;
        request.ownerDownloadId = id;
        request.inputFile = filePath;
        request.format = job.conversionFormat;
        request.outputDirectory = job.request.outputDirectory;
        request.gpuType = m_gpuDetector.getGPUType();
        if (m_gpuDetector.hasHardwareAcceleration()) {
            request.gpuCodec = QString::fromStdString(m_gpuDetector.getRecommendedCodec());
            request.gpuDevice = QString::fromStdString(m_gpuDetector.getHardwareDevice());
        }
        const ConversionEnqueueResult result = m_conversionManager->enqueueConversion(request);
        if (result.accepted) {
            job.conversionId = result.id;
            job.status = DownloadStatus::ConvertingGPU;
            job.statusText = "Aguardando conversão";
        } else {
            job.status = DownloadStatus::Error;
            job.statusText = "Falha ao enfileirar conversão: " + result.error;
            job.terminal = true;
        }
    }
    updateJobRow(id);
    refreshLibrary();
}

void MainWindow::onDownloadQueueStateChanged(int active, int pending)
{
    m_cancelAllBtn->setEnabled(m_downloadManager->hasWork() || m_conversionManager->hasAutomaticWork());
    if (selectedDownloadId() == 0) {
        m_statusLabel->setText(QString("Fila: %1 ativo(s), %2 aguardando").arg(active).arg(pending));
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(0);
        m_speedLabel->setText("Velocidade: acompanhe um item selecionando sua linha");
        m_etaLabel->setText("Conversões aguardando: " + QString::number(m_conversionManager->pendingCount()));
    }
    maybeShowQueueSummary();
}

void MainWindow::updateSelectedMonitor()
{
    const auto iterator = m_downloadJobs.constFind(selectedDownloadId());
    if (iterator == m_downloadJobs.cend()) {
        onDownloadQueueStateChanged(m_downloadManager->activeCount(), m_downloadManager->pendingCount());
        return;
    }
    const UiDownloadJob &job = iterator.value();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(qRound(job.progress));
    m_statusLabel->setText("Status: " + job.statusText);
    m_speedLabel->setText("Velocidade: " + (job.speed.isEmpty() ? "--" : job.speed));
    m_etaLabel->setText("Tempo restante: " + (job.eta.isEmpty() ? "--:--" : job.eta));
}

void MainWindow::onConversionStatus(ConversionId id, DownloadId ownerDownloadId, const QString &message)
{
    if (ownerDownloadId == 0 && id == m_manualConversionId) {
        m_convertStatusLabel->setText("Status: " + message);
        m_convertProgressBar->setRange(0, 0);
        return;
    }
    auto iterator = m_downloadJobs.find(ownerDownloadId);
    if (iterator == m_downloadJobs.end()) return;
    iterator->status = DownloadStatus::ConvertingGPU;
    iterator->statusText = message;
    updateJobRow(ownerDownloadId);
    updateSelectedMonitor();
}

void MainWindow::onConversionProgress(ConversionId id, DownloadId ownerDownloadId, double percent)
{
    if (ownerDownloadId == 0 && id == m_manualConversionId) {
        m_convertProgressBar->setRange(0, 100);
        m_convertProgressBar->setValue(qRound(percent));
        m_convertStatusLabel->setText(QString("Status: Convertendo mídia (%1%)")
                                       .arg(QString::number(percent, 'f', 1)));
        return;
    }
    auto iterator = m_downloadJobs.find(ownerDownloadId);
    if (iterator == m_downloadJobs.end()) return;
    iterator->progress = qBound(0.0, percent, 100.0);
    iterator->status = DownloadStatus::ConvertingGPU;
    iterator->statusText = QString("Convertendo mídia (%1%)").arg(QString::number(percent, 'f', 1));
    updateJobRow(ownerDownloadId);
    updateSelectedMonitor();
}

void MainWindow::onConversionCompleted(ConversionId id, DownloadId ownerDownloadId, const QString &outputFile)
{
    if (ownerDownloadId == 0 && id == m_manualConversionId) {
        m_manualConversionId = 0;
        m_convertInput->setText(outputFile);
        m_convertProgressBar->setRange(0, 100);
        m_convertProgressBar->setValue(100);
        m_convertStatusLabel->setText("Status: Conversão finalizada com sucesso.");
        m_startConvertBtn->setEnabled(true);
        m_cancelConvertBtn->setEnabled(false);
        refreshLibrary();
        return;
    }
    auto iterator = m_downloadJobs.find(ownerDownloadId);
    if (iterator == m_downloadJobs.end()) return;
    iterator->filePath = outputFile;
    iterator->status = DownloadStatus::Completed;
    iterator->statusText = "Conversão concluída";
    iterator->progress = 100.0;
    iterator->terminal = true;
    updateJobRow(ownerDownloadId);
    updateSelectedMonitor();
    refreshLibrary();
    maybeShowQueueSummary();
}

void MainWindow::onConversionFailed(ConversionId id, DownloadId ownerDownloadId, const QString &message)
{
    if (ownerDownloadId == 0 && id == m_manualConversionId) {
        m_manualConversionId = 0;
        m_convertProgressBar->setRange(0, 100);
        m_convertProgressBar->setValue(0);
        m_convertStatusLabel->setText("Status: " + message);
        m_startConvertBtn->setEnabled(true);
        m_cancelConvertBtn->setEnabled(false);
        return;
    }
    auto iterator = m_downloadJobs.find(ownerDownloadId);
    if (iterator == m_downloadJobs.end()) return;
    iterator->status = DownloadStatus::Error;
    iterator->statusText = "Erro na conversão: " + message;
    iterator->terminal = true;
    updateJobRow(ownerDownloadId);
    updateSelectedMonitor();
    maybeShowQueueSummary();
}

void MainWindow::onConversionCancelled(ConversionId id, DownloadId ownerDownloadId)
{
    if (ownerDownloadId == 0 && id == m_manualConversionId) {
        m_manualConversionId = 0;
        m_convertProgressBar->setRange(0, 100);
        m_convertProgressBar->setValue(0);
        m_convertStatusLabel->setText("Status: Conversão cancelada.");
        m_startConvertBtn->setEnabled(true);
        m_cancelConvertBtn->setEnabled(false);
        return;
    }
    auto iterator = m_downloadJobs.find(ownerDownloadId);
    if (iterator == m_downloadJobs.end()) return;
    iterator->status = DownloadStatus::Cancelled;
    iterator->statusText = "Conversão cancelada";
    iterator->terminal = true;
    updateJobRow(ownerDownloadId);
    updateSelectedMonitor();
    maybeShowQueueSummary();
}

void MainWindow::maybeShowQueueSummary()
{
    tryStartPendingAppUpdate();
    if (m_closing || m_currentBatchJobs.isEmpty() || m_downloadManager->hasWork()
        || m_conversionManager->hasAutomaticWork()) {
        return;
    }
    int successes = 0;
    int errors = 0;
    int cancellations = 0;
    for (DownloadId id : std::as_const(m_currentBatchJobs)) {
        const auto iterator = m_downloadJobs.constFind(id);
        if (iterator == m_downloadJobs.cend() || !iterator->terminal) return;
        if (iterator->status == DownloadStatus::Completed) ++successes;
        else if (iterator->status == DownloadStatus::Cancelled) ++cancellations;
        else ++errors;
    }
    const QString summary = QString("Fila concluída: %1 sucesso(s), %2 erro(s), %3 cancelamento(s).")
                                .arg(successes).arg(errors).arg(cancellations);
    logMessage("[Fila] " + summary);
    m_currentBatchJobs.clear();
    if (m_notifyCheckBox->isChecked()) {
        QMessageBox::information(this, "Resumo da fila", summary);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_installingAppUpdate) {
        QMessageBox::information(this, "Atualização em andamento",
                                 "A atualização autenticada está sendo instalada. Aguarde a conclusão.");
        event->ignore();
        return;
    }
    if (!m_downloadManager->hasWork() && !m_conversionManager->hasWork()) {
        event->accept();
        return;
    }
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, "Trabalho em andamento",
        "Há downloads ou conversões pendentes. Deseja cancelar toda a sessão e fechar?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        event->ignore();
        return;
    }
    m_closing = true;
    m_downloadManager->cancelAll();
    m_conversionManager->cancelAllAutomatic();
    if (m_manualConversionId != 0) {
        m_conversionManager->cancelConversion(m_manualConversionId);
    }
    event->accept();
}

void MainWindow::flushLogBuffer()
{
    if (!m_logEdit || m_pendingLogLines.isEmpty()) {
        if (m_logFlushTimer && m_pendingLogLines.isEmpty()) {
            m_logFlushTimer->stop();
        }
        return;
    }
    m_logEdit->appendPlainText(m_pendingLogLines.join(QLatin1Char('\n')));
    m_pendingLogLines.clear();
    if (m_logFlushTimer) {
        m_logFlushTimer->stop();
    }
}

void MainWindow::logMessage(const QString &msg)
{
    m_allLogs.append(msg);
    if (m_allLogs.size() > kMaximumLogEntries) {
        m_allLogs.erase(m_allLogs.begin(), m_allLogs.begin() + (m_allLogs.size() - kMaximumLogEntries));
    }
    if (m_logEdit && shouldShowLogLine(msg)) {
        m_pendingLogLines.append(msg);
        if (m_logFlushTimer && !m_logFlushTimer->isActive()) {
            m_logFlushTimer->start();
        }
    }
}

bool MainWindow::shouldShowLogLine(const QString &line) const
{
    if (m_logFilterMode == 0) return true; // Todos
    if (m_logFilterMode == 1) {
        // Apenas Processos (yt-dlp, FFmpeg, junção, conversão)
        return line.contains("[Processo", Qt::CaseInsensitive) ||
               line.contains("[Download #", Qt::CaseInsensitive) ||
               line.contains("[Conversão #", Qt::CaseInsensitive) ||
               line.contains("[Conversor]", Qt::CaseInsensitive) ||
               line.contains("[Recorte]", Qt::CaseInsensitive) ||
               line.contains("[Auto-Conversao]", Qt::CaseInsensitive) ||
               line.contains("[Status]", Qt::CaseInsensitive) ||
               line.contains("[Muxing]", Qt::CaseInsensitive) ||
               line.contains("[Output]", Qt::CaseInsensitive) ||
               line.contains("[Sucesso]", Qt::CaseInsensitive);
    }
    if (m_logFilterMode == 2) {
        // Apenas Erros e Alertas
        return line.contains("[Erro", Qt::CaseInsensitive) ||
               line.contains("Erro", Qt::CaseInsensitive) ||
               line.contains("Falha", Qt::CaseInsensitive) ||
               line.contains("[Alerta", Qt::CaseInsensitive) ||
               line.contains("Warning", Qt::CaseInsensitive) ||
               line.contains("Error", Qt::CaseInsensitive) ||
               line.contains("404", Qt::CaseInsensitive) ||
               line.contains("403", Qt::CaseInsensitive) ||
               line.contains("Unavailable", Qt::CaseInsensitive);
    }
    if (m_logFilterMode == 3) {
        // Apenas Gerais (Sistema, GPU, Biblioteca, Updater, Destino)
        return line.contains("[System]", Qt::CaseInsensitive) ||
               line.contains("[Biblioteca]", Qt::CaseInsensitive) ||
               line.contains("[Updater]", Qt::CaseInsensitive) ||
               line.contains("[Destino", Qt::CaseInsensitive) ||
               line.contains("Placa gráfica", Qt::CaseInsensitive) ||
               line.contains("===");
    }
    return true;
}

void MainWindow::updateLogFilter(int mode)
{
    m_logFilterMode = mode;
    QString activeStyle = "background-color: #10b981; color: #000000; font-weight: bold; padding: 6px 12px; border-radius: 4px; border: 1px solid #10b981;";
    QString inactiveStyle = "background-color: #262626; color: #dedede; font-weight: bold; padding: 6px 12px; border-radius: 4px; border: 1px solid #333333;";
    QString clearStyle = "background-color: #ef4444; color: #ffffff; font-weight: bold; padding: 6px 12px; border-radius: 4px; border: 1px solid #dc2626;";
    
    if (m_filterAllBtn) m_filterAllBtn->setStyleSheet(mode == 0 ? activeStyle : inactiveStyle);
    if (m_filterProcessesBtn) m_filterProcessesBtn->setStyleSheet(mode == 1 ? activeStyle : inactiveStyle);
    if (m_filterErrorsBtn) m_filterErrorsBtn->setStyleSheet(mode == 2 ? activeStyle : inactiveStyle);
    if (m_filterGeneralBtn) m_filterGeneralBtn->setStyleSheet(mode == 3 ? activeStyle : inactiveStyle);
    if (m_clearLogsBtn) m_clearLogsBtn->setStyleSheet(clearStyle);
    
    refreshLogDisplay();
}

void MainWindow::refreshLogDisplay()
{
    if (!m_logEdit) return;
    m_pendingLogLines.clear();
    if (m_logFlushTimer) {
        m_logFlushTimer->stop();
    }
    m_logEdit->clear();
    for (const QString &line : m_allLogs) {
        if (shouldShowLogLine(line)) {
            m_logEdit->appendPlainText(line);
        }
    }
}

void MainWindow::setupStyles()
{
    QString qss = R"(
        QMainWindow {
            background-color: #121212;
            color: #dedede;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 13px;
        }
        QWidget#downloadPage {
            background-color: #121212;
        }
        QWidget {
            color: #dedede;
        }
        QFrame#sidebar {
            background-color: #181818;
            border-right: 1px solid #262626;
        }
        QStackedWidget#mainArea {
            background-color: #121212;
        }
        QPushButton#navBtn {
            background-color: transparent;
            color: #909090;
            border: none;
            border-left: 3px solid transparent;
            padding: 13px 18px;
            text-align: left;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton#navBtn:hover {
            background-color: #222222;
            color: #ffffff;
        }
        QPushButton#navBtn:checked {
            background-color: #1f2a24;
            color: #10b981;
            border-left: 3px solid #10b981;
        }
        QPushButton#updateSideBtn {
            background-color: #1b2922;
            color: #10b981;
            font-weight: bold;
            font-size: 13px;
            border: 1px solid #10b981;
            border-radius: 5px;
            padding: 9px;
            margin: 0 14px 6px 14px;
        }
        QPushButton#updateSideBtn:hover {
            background-color: #10b981;
            color: #021810;
        }
        QPushButton#updateSideBtn:checked {
            background-color: #10b981;
            color: #021810;
        }
        QPushButton#openFolderSideBtn {
            background-color: #1c2e3a;
            color: #38bdf8;
            font-weight: bold;
            font-size: 13px;
            border: 1px solid #38bdf8;
            border-radius: 5px;
            padding: 9px;
            margin: 0 14px;
        }
        QPushButton#openFolderSideBtn:hover {
            background-color: #38bdf8;
            color: #061824;
        }
        QGroupBox {
            background-color: #1a1a1a;
            border: 1px solid #282828;
            border-radius: 8px;
            margin-top: 14px;
            font-weight: bold;
            color: #ffffff;
            font-size: 13px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 0 8px;
            color: #10b981;
        }
        QLineEdit, QComboBox {
            background-color: #202020;
            border: 1px solid #333333;
            border-radius: 6px;
            padding: 8px 12px;
            color: #ffffff;
            font-size: 13px;
        }
        QLineEdit:focus, QComboBox:focus {
            border: 1px solid #10b981;
            background-color: #262626;
        }
        QComboBox::drop-down {
            border: none;
        }
        QComboBox QAbstractItemView {
            background-color: #222222;
            color: white;
            selection-background-color: #10b981;
            selection-color: black;
        }
        QCheckBox {
            font-size: 13px;
            color: #a3a3a3;
            padding-top: 4px;
        }
        QCheckBox::indicator {
            width: 17px;
            height: 17px;
            border: 1px solid #3e3e3e;
            border-radius: 4px;
            background-color: #222222;
        }
        QCheckBox::indicator:hover {
            border: 1px solid #10b981;
        }
        QCheckBox::indicator:checked {
            background-color: #10b981;
            border: 1px solid #ffffff;
        }
        QLabel {
            font-size: 13px;
        }
        QPushButton#startBtn {
            background-color: #10b981;
            color: #021810;
            font-weight: bold;
            font-size: 14px;
            border-radius: 6px;
            border: none;
            padding: 10px;
        }
        QPushButton#startBtn:hover {
            background-color: #059669;
            color: #ffffff;
        }
        QPushButton#startBtn:disabled {
            background-color: #242424;
            color: #666666;
        }
        QPushButton#cancelBtn {
            background-color: #dc2626;
            color: #ffffff;
            font-weight: bold;
            font-size: 13px;
            border-radius: 6px;
            border: none;
            padding: 10px;
        }
        QPushButton#cancelBtn:hover {
            background-color: #b91c1c;
        }
        QPushButton#cancelBtn:disabled {
            background-color: #242424;
            color: #666666;
        }
        QPushButton#browseBtn {
            background-color: #263530;
            color: #10b981;
            font-weight: bold;
            border: 1px solid #10b981;
            border-radius: 6px;
            padding: 6px 14px;
        }
        QPushButton#browseBtn:hover {
            background-color: #354a43;
            color: #ffffff;
        }
        QProgressBar {
            background-color: #202020;
            border: 1px solid #333333;
            border-radius: 6px;
            text-align: center;
            font-weight: bold;
            color: #ffffff;
        }
        QProgressBar::chunk {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #10b981, stop:1 #047857);
            border-radius: 5px;
        }
        QTableWidget#libraryTable {
            background-color: #1a1a1a;
            border: 1px solid #282828;
            border-radius: 6px;
            color: #ffffff;
            gridline-color: #282828;
            font-size: 13px;
            selection-background-color: #10b981;
            selection-color: #021810;
        }
        QTableWidget#libraryTable QHeaderView::section {
            background-color: #242424;
            color: #10b981;
            font-weight: bold;
            border: none;
            border-bottom: 2px solid #10b981;
            padding: 8px 10px;
            font-size: 13px;
        }
        QTableWidget#libraryTable::item {
            padding: 8px 10px;
            border-bottom: 1px solid #222222;
        }
        QTableWidget#libraryTable::item:selected {
            background-color: #10b981;
            color: #021810;
            font-weight: bold;
        }
        QPushButton#libraryViewBtn {
            background-color: #263530;
            color: #a7f3d0;
            border: 1px solid #315344;
            border-radius: 6px;
            padding: 6px 12px;
            font-weight: bold;
        }
        QPushButton#libraryViewBtn:hover {
            background-color: #354a43;
            color: #ffffff;
        }
        QPushButton#libraryViewBtn:checked {
            background-color: #10b981;
            color: #021810;
            border-color: #10b981;
        }
        QListWidget#libraryBlocks {
            background-color: #1a1a1a;
            border: 1px solid #282828;
            border-radius: 6px;
            outline: none;
            padding: 8px;
        }
        QListWidget#libraryBlocks::item {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 8px;
            padding: 2px;
        }
        QListWidget#libraryBlocks::item:selected {
            background-color: #263e34;
            border: 1px solid #10b981;
        }
        QWidget#libraryCard {
            background-color: #202522;
            border: 1px solid #303a34;
            border-radius: 7px;
        }
        QLabel#libraryCardThumb {
            background-color: #111513;
            border: 1px solid #35443b;
            border-radius: 5px;
            color: #94a3b8;
            font-size: 11px;
        }
        QLabel#libraryCardTitle {
            color: #f8fafc;
            font-size: 12px;
            font-weight: bold;
        }
        QLabel#libraryCardMeta {
            color: #10b981;
            font-size: 11px;
            font-weight: bold;
        }
        QPlainTextEdit#logArea {
            background-color: #0a0e0b;
            border: 1px solid #1a241c;
            border-radius: 6px;
            color: #10b981;
            font-family: 'Consolas', 'Courier New', monospace;
            font-size: 12px;
            padding: 12px;
        }
    )";

    setStyleSheet(qss);
}

void MainWindow::checkForUpdates(bool silent)
{
    if (!m_appUpdateService || m_appUpdateService->isBusy()) {
        return;
    }
    m_appUpdateCheckSilent = silent;
    m_appUpdatePending = false;
    if (m_updateAppBtn) {
        m_updateAppBtn->setVisible(false);
        m_updateAppBtn->setEnabled(false);
    }
    if (m_updateStatusLabel) {
        m_updateStatusLabel->setText("Consultando a release assinada no GitHub...");
    }
    if (m_sidebarUpdateNotification) {
        m_sidebarUpdateNotification->setText("🔄 Checando...");
        m_sidebarUpdateNotification->setStyleSheet("color: #38bdf8; font-size: 12px; font-weight: bold; margin-bottom: 2px;");
    }
    if (m_checkUpdateBtn) {
        m_checkUpdateBtn->setEnabled(false);
        m_checkUpdateBtn->setText("VERIFICANDO RELEASE...");
    }
    logMessage("[Updater] Consultando release e manifesto assinado no GitHub.");
    m_appUpdateService->checkLatestRelease();
}

void MainWindow::requestAppUpdate()
{
    if (!m_appUpdateService || !m_appUpdateService->hasLatestRelease() || m_installingAppUpdate) {
        return;
    }
    m_appUpdatePending = true;
    if (m_updateAppBtn) {
        m_updateAppBtn->setEnabled(false);
        m_updateAppBtn->setText("ATUALIZAÇÃO AGENDADA...");
    }
    tryStartPendingAppUpdate();
}

bool MainWindow::canInstallAppUpdate() const
{
    return !m_closing && !m_installingAppUpdate && !m_downloadManager->hasWork()
        && !m_conversionManager->hasWork() && !m_playlistPreviewProcess;
}

void MainWindow::tryStartPendingAppUpdate()
{
    if (!m_appUpdatePending || !m_appUpdateService || !m_appUpdateService->hasLatestRelease()
        || m_appUpdateService->isBusy() || m_installingAppUpdate) {
        return;
    }
    if (!canInstallAppUpdate()) {
        if (m_updateStatusLabel) {
            m_updateStatusLabel->setText("Atualização autenticada aguardando downloads, conversões ou prévia terminarem.");
        }
        return;
    }

    m_appUpdatePending = false;
    if (m_updateStatusLabel) {
        m_updateStatusLabel->setText("Baixando pacote autenticado; validando SHA-256...");
    }
    if (m_updateAppBtn) {
        m_updateAppBtn->setEnabled(false);
        m_updateAppBtn->setText("BAIXANDO ATUALIZAÇÃO...");
    }
    logMessage("[Updater] A fila está ociosa; iniciando download do pacote autenticado.");
    m_appUpdateService->downloadLatestRelease();
}

bool MainWindow::isInstalledWindowsCopy() const
{
#ifdef Q_OS_WIN
    return QFileInfo(QDir(QCoreApplication::applicationDirPath()).filePath("unins000.exe")).isFile();
#else
    return false;
#endif
}

void MainWindow::installVerifiedAppPackage(const QString &version, const QString &packagePath)
{
    if (!canInstallAppUpdate()) {
        m_appUpdatePending = true;
        if (m_updateStatusLabel) {
            m_updateStatusLabel->setText("Pacote v" + version + " validado; instalação aguardando a fila ficar ociosa.");
        }
        return;
    }

#ifdef Q_OS_WIN
    m_installingAppUpdate = true;
    if (isInstalledWindowsCopy()) {
        const bool started = QProcess::startDetached(packagePath, {
            QStringLiteral("/VERYSILENT"), QStringLiteral("/SUPPRESSMSGBOXES"),
            QStringLiteral("/NORESTART"), QStringLiteral("/CLOSEAPPLICATIONS")});
        if (!started) {
            m_installingAppUpdate = false;
            QFile::remove(packagePath);
            if (m_updateAppBtn) m_updateAppBtn->setEnabled(true);
            QMessageBox::warning(this, "Atualização preservada",
                                 "Não foi possível iniciar o instalador validado.");
            return;
        }
        logMessage("[Updater] Instalador Windows validado iniciado; encerrando a versão atual.");
        m_closing = true;
        QCoreApplication::quit();
        return;
    }

    const QString helperSource = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("PrismPortableUpdateHelper.exe"));
    const QString helperDirectory = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QStringLiteral("PrismDownloader/update-helper/%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    const QString helperCopy = QDir(helperDirectory).filePath(QStringLiteral("PrismPortableUpdateHelper.exe"));
    if (!QFileInfo(helperSource).isFile() || !QDir().mkpath(helperDirectory)
        || !QFile::copy(helperSource, helperCopy)) {
        m_installingAppUpdate = false;
        QFile::remove(packagePath);
        if (m_updateAppBtn) m_updateAppBtn->setEnabled(true);
        QMessageBox::warning(this, "Atualização preservada",
                             "O helper obrigatório para atualizar a cópia Portable não está disponível.");
        return;
    }
    const bool started = QProcess::startDetached(helperCopy, {
        QStringLiteral("--parent-pid"), QString::number(QCoreApplication::applicationPid()),
        QStringLiteral("--archive"), packagePath,
        QStringLiteral("--target"), QCoreApplication::applicationDirPath()});
    if (!started) {
        m_installingAppUpdate = false;
        QFile::remove(packagePath);
        QFile::remove(helperCopy);
        if (m_updateAppBtn) m_updateAppBtn->setEnabled(true);
        QMessageBox::warning(this, "Atualização preservada",
                             "Não foi possível iniciar o helper da atualização Portable.");
        return;
    }
    logMessage("[Updater] Helper Portable iniciado com pacote validado; encerrando a versão atual.");
    m_closing = true;
    QCoreApplication::quit();
#else
    const QString pkexec = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
    const QString aptGet = QStandardPaths::findExecutable(QStringLiteral("apt-get"));
    if (pkexec.isEmpty() || aptGet.isEmpty()) {
        m_installingAppUpdate = false;
        QFile::remove(packagePath);
        if (m_updateAppBtn) m_updateAppBtn->setEnabled(true);
        QMessageBox::warning(this, "AtualizaÃ§Ã£o indisponÃ­vel",
                             "pkexec e apt-get sÃ£o necessÃ¡rios para instalar atualizaÃ§Ãµes no Linux.");
        return;
    }
    m_installingAppUpdate = true;
    auto *process = new QProcess(this);
    m_appUpdateInstallProcess = process;
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this, process, version, packagePath](int exitCode, QProcess::ExitStatus status) {
        if (m_appUpdateInstallProcess != process) {
            process->deleteLater();
            return;
        }
        m_appUpdateInstallProcess = nullptr;
        m_installingAppUpdate = false;
        process->deleteLater();
        if (status != QProcess::NormalExit || exitCode != 0) {
            QFile::remove(packagePath);
            if (m_updateAppBtn) m_updateAppBtn->setEnabled(true);
            QMessageBox::warning(this, "Atualização preservada",
                                 "O APT não instalou o pacote validado. A versão atual continua em execução.");
            return;
        }
        QFile::remove(packagePath);
        if (!QProcess::startDetached(QCoreApplication::applicationFilePath())) {
            if (m_updateAppBtn) m_updateAppBtn->setEnabled(true);
            QMessageBox::warning(this, "Atualização instalada",
                                 "O pacote foi instalado, mas reinicie o Prism Downloader manualmente.");
            return;
        }
        logMessage("[Updater] Pacote Linux v" + version + " instalado; reiniciando o aplicativo.");
        m_closing = true;
        QCoreApplication::quit();
    });
    connect(process, &QProcess::errorOccurred, this,
            [this, process, packagePath](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || m_appUpdateInstallProcess != process) {
            return;
        }
        m_appUpdateInstallProcess = nullptr;
        m_installingAppUpdate = false;
        process->deleteLater();
        QFile::remove(packagePath);
        if (m_updateAppBtn) m_updateAppBtn->setEnabled(true);
        QMessageBox::warning(this, "Atualização preservada",
                             "Não foi possível iniciar o pkexec para instalar o pacote validado.");
    });
    process->start(pkexec, {
        aptGet, QStringLiteral("install"),
        QStringLiteral("--yes"), packagePath});
    if (m_updateStatusLabel) {
        m_updateStatusLabel->setText("Aguardando autorização para instalar a atualização v" + version + ".");
    }
#endif
}

void MainWindow::checkYtDlpUpdates(bool silent)
{
    if (!m_ytdlpUpdateService || m_ytdlpUpdateService->isBusy()) {
        return;
    }
    m_ytdlpCheckSilent = silent;
    if (m_ytdlpStatusLabel) {
        m_ytdlpStatusLabel->setText(ytdlpCurrentDescription()
                                    + " — consultando a Nightly oficial...");
    }
    if (m_updateYtdlpBtn) {
        m_updateYtdlpBtn->setEnabled(false);
        m_updateYtdlpBtn->setText("VERIFICANDO YT-DLP...");
    }
    logMessage("[Motor Extrator] Consultando a release Nightly oficial do yt-dlp...");
    m_ytdlpUpdateService->checkLatestRelease();
}

void MainWindow::updateYtdlpEngine()
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
        && !MediaToolResolver::isVersionNewer(m_ytdlpUpdateService->latestVersion(), current.version)) {
        QMessageBox::information(this, "yt-dlp", ytdlpCurrentDescription()
                                 + " já é igual ou mais recente que a Nightly publicada.");
        return;
    }
    if (m_downloadManager->hasWork() || m_conversionManager->hasWork() || m_playlistPreviewProcess) {
        QMessageBox::warning(this, "Atualização adiada",
                             "Conclua ou cancele downloads, conversões e prévias antes de atualizar o yt-dlp.");
        return;
    }

    const QMessageBox::StandardButton response = QMessageBox::question(
        this,
        "Atualizar yt-dlp Nightly",
        QString("A versão Nightly %1 será baixada da release oficial, validada por SHA-256 e salva apenas na pasta de dados do seu usuário.\n\nDeseja continuar?")
            .arg(m_ytdlpUpdateService->latestVersion()),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Yes);
    if (response != QMessageBox::Yes) {
        return;
    }

    if (m_updateYtdlpBtn) {
        m_updateYtdlpBtn->setEnabled(false);
        m_updateYtdlpBtn->setText("ATUALIZANDO YT-DLP...");
    }
    if (m_updateProgressBar) {
        m_updateProgressBar->setVisible(true);
        m_updateProgressBar->setRange(0, 0);
    }
    logMessage("[Motor Extrator] Atualização Nightly confirmada pelo usuário; baixando checksum e binário.");
    m_ytdlpUpdateService->installLatestRelease();
}
