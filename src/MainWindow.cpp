#include "MainWindow.h"
#include "DownloadQueueWorkflow.h"
#include "FormatSelectionDialog.h"
#include "LibraryView.h"
#include "LogHighlighter.h"
#include "MainWindowUiBuilder.h"
#include "MainWindowUpdateCoordinator.h"
#include "PlaylistItemDetailsDialog.h"
#include "PlaylistSelectionDialog.h"
#include "PrismStyleSheet.h"
#include "PrismVersion.h"
#include "YtDlpMetadataService.h"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QColor>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMetaObject>
#include <QMessageBox>
#include <QMimeData>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QPointer>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStandardPaths>
#include <QSyntaxHighlighter>
#include <QTableWidgetItem>
#include <QTextCharFormat>
#include <QThread>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>
#include <utility>

static const QString NEOV_VERSION_TAG = QStringLiteral(PRISM_VERSION_TAG);
static const QString NEOV_VERSION_NUMBER = QStringLiteral(PRISM_VERSION_NUMBER);

namespace {
constexpr int kMaximumLogEntries = 5000;
constexpr int kMaximumPlaylistItems = 500;

enum class LogLevel {
    Info,
    Warning,
    Error
};

LogLevel logLevelForMessage(const QString &message)
{
    if (message.contains(QStringLiteral("[ERROR]"), Qt::CaseInsensitive)) {
        return LogLevel::Error;
    }
    if (message.contains(QStringLiteral("[WARN]"), Qt::CaseInsensitive)) {
        return LogLevel::Warning;
    }
    const QString lower = message.toLower();
    static const QStringList errorMarkers{
        QStringLiteral("erro"), QStringLiteral("error"), QStringLiteral("falha"),
        QStringLiteral("failed"), QStringLiteral("falhou"), QStringLiteral("falhar"),
        QStringLiteral("fatal"), QStringLiteral("inválid"),
        QStringLiteral("invalid"), QStringLiteral("não foi possível"),
        QStringLiteral("não encontrado"), QStringLiteral("ausente"),
        QStringLiteral("crash"), QStringLiteral("encerrou com código")};
    for (const QString &marker : errorMarkers) {
        if (lower.contains(marker)) {
            return LogLevel::Error;
        }
    }

    static const QStringList warningMarkers{
        QStringLiteral("alerta"), QStringLiteral("warning"), QStringLiteral("fallback"),
        QStringLiteral("aguarda confirmação"), QStringLiteral("não contém a chave"),
        QStringLiteral("desativada"), QStringLiteral("indisponível"),
        QStringLiteral("não localizado"), QStringLiteral("nenhum encoder")};
    for (const QString &marker : warningMarkers) {
        if (lower.contains(marker)) {
            return LogLevel::Warning;
        }
    }
    return LogLevel::Info;
}

QString logLevelLabel(LogLevel level)
{
    switch (level) {
    case LogLevel::Error:
        return QStringLiteral("ERROR");
    case LogLevel::Warning:
        return QStringLiteral("WARN");
    case LogLevel::Info:
        return QStringLiteral("INFO");
    }
    return QStringLiteral("INFO");
}

QString normalizedLogLine(const QString &message)
{
    const QString clean = message.trimmed();
    return QStringLiteral("[%1] [%2] %3")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")),
             logLevelLabel(logLevelForMessage(clean)), clean);
}

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

}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_settings(AppSettings::load()),
      m_downloadManager(new DownloadManager(this)),
      m_conversionManager(new ConversionManager(this))
{
    setWindowTitle("Prism Downloader - Studio Suite");
    resize(980, 620);

    setupUI();
    setAcceptDrops(true);
    setupStyles();
    m_downloadQueueWorkflow = std::make_unique<DownloadQueueWorkflow>(m_downloadManager);

    m_logFlushTimer = new QTimer(this);
    m_logFlushTimer->setInterval(75);
    connect(m_logFlushTimer, &QTimer::timeout, this, &MainWindow::flushLogBuffer);
    initializeLogFile();
    logMessage(QStringLiteral("[System] Sessão iniciada | versão %1 | arquivo de log: %2")
                   .arg(NEOV_VERSION_TAG,
                        m_logFilePath.isEmpty()
                            ? QStringLiteral("somente na memória")
                            : QDir::toNativeSeparators(m_logFilePath)));

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

    m_metadataService = new YtDlpMetadataService(this);
    connect(m_metadataService, &YtDlpMetadataService::busyChanged, this,
            [this](bool busy) {
        if (m_startBtn) {
            m_startBtn->setEnabled(!busy);
        }
    });
    connect(m_metadataService, &YtDlpMetadataService::logMessage,
            this, &MainWindow::logMessage);
    connect(m_metadataService, &YtDlpMetadataService::metadataReady, this,
            [this](const QList<PlaylistItem> &items, const MediaMetadata &metadata) {
        if (!metadata.error.isEmpty()) {
            logMessage(QStringLiteral("[Metadados] Análise indisponível: %1")
                           .arg(metadata.error));
        } else {
            logMessage(QStringLiteral(
                "[Metadados] Fonte analisada: %1 | duração: %2 | %3 opção(ões) identificada(s).")
                           .arg(metadata.title.isEmpty() ? QStringLiteral("sem título") : metadata.title,
                                metadata.durationText.isEmpty() ? QStringLiteral("desconhecida")
                                                                : metadata.durationText)
                           .arg(metadata.options.size()));
        }
        continueDownloadWithMetadata(items, metadata);
    });

    m_playlistPreviewService = new PlaylistPreviewService(this, this);
    connect(m_playlistPreviewService, &PlaylistPreviewService::busyChanged, this,
            [this](bool busy) {
        if (m_startBtn) {
            const bool metadataBusy = m_metadataService && m_metadataService->isRunning();
            m_startBtn->setEnabled(!busy && !metadataBusy);
        }
    });
    connect(m_playlistPreviewService, &PlaylistPreviewService::logMessage,
            this, &MainWindow::logMessage);
    connect(m_playlistPreviewService, &PlaylistPreviewService::previewError, this,
            [this](const QString &title, const QString &message) {
        QMessageBox::warning(this, title, message);
    });
    connect(m_playlistPreviewService, &PlaylistPreviewService::previewReady, this,
            &MainWindow::handlePlaylistPreviewReady);

    m_updateCoordinator = std::make_unique<MainWindowUpdateCoordinator>(
        this, NEOV_VERSION_TAG, NEOV_VERSION_NUMBER);
    startGpuProbe();

    connect(m_downloadManager, &DownloadManager::jobProgress, this, &MainWindow::onDownloadProgress);
    connect(m_downloadManager, &DownloadManager::jobStatus, this, &MainWindow::onDownloadStatus);
    connect(m_downloadManager, &DownloadManager::jobCompleted, this, &MainWindow::onDownloadCompleted);
    connect(m_downloadManager, &DownloadManager::queueStateChanged, this, &MainWindow::onDownloadQueueStateChanged);
    connect(m_downloadManager, &DownloadManager::queueIdle, this, &MainWindow::maybeShowQueueSummary);
    connect(m_downloadManager, &DownloadManager::queueIdle, this, [this]() {
        if (m_updateCoordinator) {
            m_updateCoordinator->tryStartPendingAppUpdate();
        }
    });
    connect(m_downloadManager, &DownloadManager::jobLog, this, [this](DownloadId id, const QString &message) {
        logMessage(QString("[Download #%1] %2").arg(id).arg(message));
    });

    connect(m_conversionManager, &ConversionManager::conversionStatus, this, &MainWindow::onConversionStatus);
    connect(m_conversionManager, &ConversionManager::conversionProgress, this, &MainWindow::onConversionProgress);
    connect(m_conversionManager, &ConversionManager::conversionCompleted, this, &MainWindow::onConversionCompleted);
    connect(m_conversionManager, &ConversionManager::conversionFailed, this, &MainWindow::onConversionFailed);
    connect(m_conversionManager, &ConversionManager::conversionCancelled, this, &MainWindow::onConversionCancelled);
    connect(m_conversionManager, &ConversionManager::queueIdle, this, &MainWindow::maybeShowQueueSummary);
    connect(m_conversionManager, &ConversionManager::queueIdle, this, [this]() {
        if (m_updateCoordinator) {
            m_updateCoordinator->tryStartPendingAppUpdate();
        }
    });
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
    const bool checkOnStart = m_settings.checkUpdatesOnStart;
    const int concurrency = m_settings.maxConcurrentDownloads;
    if (m_concurrencySpin) m_concurrencySpin->setValue(concurrency);
    m_downloadManager->setConcurrencyLimit(concurrency);
    if (m_checkUpdatesOnStartChk) m_checkUpdatesOnStartChk->setChecked(checkOnStart);
    if (m_autoDownloadUpdatesChk) {
        m_autoDownloadUpdatesChk->setChecked(m_settings.autoDownloadUpdates);
        connect(m_autoDownloadUpdatesChk, &QCheckBox::toggled, this, [this](bool enabled) {
            logMessage(enabled
                ? "[Updater] Download e instalação automáticos foram ativados pelo usuário."
                : "[Updater] Download e instalação automáticos foram desativados pelo usuário.");
            if (enabled) {
                if (m_updateCoordinator) {
                    m_updateCoordinator->tryStartPendingAppUpdate();
                }
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
    m_settings.outputFolder = m_outputDirInput->text().trimmed();
    m_settings.showNotifications = m_notifyCheckBox->isChecked();
    m_settings.selectedQualityIndex = m_qualityCombo->currentIndex();
    m_settings.defaultTimeRange = m_timeRangeInput->text().trimmed();
    if (m_checkUpdatesOnStartChk) {
        m_settings.checkUpdatesOnStart = m_checkUpdatesOnStartChk->isChecked();
    }
    if (m_concurrencySpin) {
        m_settings.maxConcurrentDownloads = m_concurrencySpin->value();
    }
    if (m_autoDownloadUpdatesChk) {
        m_settings.autoDownloadUpdates = m_autoDownloadUpdatesChk->isChecked();
    }
    m_settings.save();
    if (m_logFile.isOpen()) {
        m_logFile.flush();
        m_logFile.close();
    }
    if (m_gpuProbeThread) {
        m_gpuProbeThread->disconnect();
        m_gpuProbeThread->wait();
        delete m_gpuProbeThread;
        m_gpuProbeThread = nullptr;
    }
    delete m_gpuProbeResult;
    m_gpuProbeResult = nullptr;
}

void MainWindow::setupUI()
{
    MainWindowUiBuilder::build(this, NEOV_VERSION_TAG, NEOV_VERSION_NUMBER, m_settings);
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
    if (m_playlistPreviewService) {
        m_playlistPreviewService->closeDialog();
    }
}

void MainWindow::startMetadataLookup(const QList<PlaylistItem> &items)
{
    if (!m_metadataService) {
        return;
    }
    m_metadataService->start(items, this);
}

void MainWindow::startPlaylistPreview(const QUrl &url)
{
    if (m_playlistPreviewService) {
        m_playlistPreviewService->start(url);
    }
}

void MainWindow::handlePlaylistPreviewReady(const QList<PlaylistItem> &items,
                                            int exitCode,
                                            bool truncated,
                                            const QString &errorOutput)
{
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
}

bool MainWindow::showPlaylistSelectionDialog(const QList<PlaylistItem> &items,
                                             QList<PlaylistItem> &selectedItems)
{
    PlaylistSelectionDialog dialog(items, styleSheet(), this);
    connect(&dialog, &PlaylistSelectionDialog::itemDetailsRequested,
            this, &MainWindow::showPlaylistItemDetailsDialog);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    selectedItems = dialog.selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, "Nenhum item selecionado",
                                 "Selecione pelo menos um vídeo para adicionar à fila.");
        return false;
    }
    return true;
}

void MainWindow::showPlaylistItemDetailsDialog(const PlaylistItem &item)
{
    PlaylistItemDetailsDialog dialog(item, styleSheet(), m_thumbnailNetwork, this);
    dialog.exec();
}

void MainWindow::continueDownload(const QList<PlaylistItem> &items)
{
    if (items.isEmpty()) {
        return;
    }

    startMetadataLookup(items);
}

void MainWindow::continueDownloadWithMetadata(const QList<PlaylistItem> &items,
                                              const MediaMetadata &metadata)
{
    if (items.isEmpty()) {
        return;
    }

    QString selectedQuality, timeRange, convertFormat, customOutputDir;
    bool doConvert = false;
    if (!showFormatSelectionDialog(metadata, items.size(), selectedQuality, timeRange,
                                   doConvert, convertFormat, customOutputDir)) {
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
    m_settings.outputFolder = defaultOutputDir;
    m_settings.showNotifications = m_notifyCheckBox->isChecked();
    m_settings.selectedQualityIndex = m_qualityCombo->currentIndex();
    m_settings.defaultTimeRange = timeRange;
    m_settings.save();

    if (!m_downloadQueueWorkflow) {
        QMessageBox::critical(this, "Fila indisponível",
                              "O gerenciador de downloads não está disponível.");
        return;
    }

    const DownloadBatchOptions batchOptions{selectedQuality, timeRange, customOutputDir};
    const DownloadBatchResult batch = m_downloadQueueWorkflow->enqueue(items, batchOptions);
    for (const QString &rejected : batch.rejected) {
        logMessage("[Fila] Item recusado: " + rejected);
    }

    for (const EnqueuedDownload &accepted : batch.accepted) {
        UiDownloadJob uiJob;
        uiJob.request = accepted.request;
        uiJob.autoConvert = doConvert;
        uiJob.conversionFormat = convertFormat;
        m_downloadJobs.insert(accepted.id, uiJob);
        m_currentBatchJobs.insert(accepted.id);

        if (m_downloadsQueueTable) {
            m_downloadsQueueTable->insertRow(0);
            for (int column = 0; column < m_downloadsQueueTable->columnCount(); ++column) {
                auto *cell = new QTableWidgetItem;
                cell->setTextAlignment(column == 0 ? Qt::AlignLeft | Qt::AlignVCenter
                                                   : Qt::AlignCenter);
                m_downloadsQueueTable->setItem(0, column, cell);
            }
            m_downloadsQueueTable->item(0, 0)->setData(
                Qt::UserRole, QVariant::fromValue<qulonglong>(accepted.id));
            m_downloadRowItems.insert(accepted.id, m_downloadsQueueTable->item(0, 0));
            updateJobRow(accepted.id);
            m_downloadsQueueTable->selectRow(0);
        }

        logMessage(QString("[Fila] Download #%1 adicionado: %2")
                       .arg(accepted.id).arg(accepted.itemLabel));
        if (!timeRange.isEmpty()) {
            logMessage(QString("[Download #%1] Recorte programado: %2")
                           .arg(accepted.id).arg(timeRange));
        }
        if (doConvert) {
            logMessage(QString("[Download #%1] Conversão automática programada: %2")
                           .arg(accepted.id).arg(convertFormat));
        }
    }

    const int addedCount = batch.accepted.size();
    const QStringList rejectedItems = batch.rejected;

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
    if (!m_libraryView || !m_stackedWidget || m_stackedWidget->currentIndex() != 1) {
        return;
    }
    const QString folder = m_currentDownloadDir.isEmpty()
        ? m_outputDirInput->text() : m_currentDownloadDir;
    m_libraryView->refresh(folder);
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

void MainWindow::showQueueContextMenu(const QPoint &pos)
{
    if (!m_downloadsQueueTable) {
        return;
    }
    const int row = m_downloadsQueueTable->rowAt(pos.y());
    if (row < 0) {
        return;
    }
    const QTableWidgetItem *item = m_downloadsQueueTable->item(row, 0);
    if (!item) {
        return;
    }
    const DownloadId id = item->data(Qt::UserRole).toULongLong();
    const auto jobIt = m_downloadJobs.constFind(id);
    if (jobIt == m_downloadJobs.cend()) {
        return;
    }
    const UiDownloadJob &job = jobIt.value();

    QMenu menu(this);
    menu.setStyleSheet(styleSheet());

    if (job.status == DownloadStatus::Completed && !job.filePath.isEmpty() && QFile::exists(job.filePath)) {
        const QString path = job.filePath;
        QAction *openFileAction = menu.addAction(QStringLiteral("🎬 Reproduzir Mídia"));
        connect(openFileAction, &QAction::triggered, this, [this, path]() {
            openLibraryFile(path);
        });
    }

    const QString targetPath = (!job.filePath.isEmpty() && QFile::exists(job.filePath))
        ? job.filePath : job.request.outputDirectory;
    QAction *openFolderAction = menu.addAction(QStringLiteral("📂 Abrir Pasta de Destino"));
    connect(openFolderAction, &QAction::triggered, this, [targetPath]() {
        if (!targetPath.isEmpty()) {
            const QFileInfo fi(targetPath);
            QDesktopServices::openUrl(QUrl::fromLocalFile(fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath()));
        }
    });

    menu.addSeparator();

    const QString urlString = job.request.url.toString();
    QAction *copyUrlAction = menu.addAction(QStringLiteral("📋 Copiar URL"));
    connect(copyUrlAction, &QAction::triggered, this, [urlString]() {
        QGuiApplication::clipboard()->setText(urlString);
    });

    if (job.status == DownloadStatus::Error || job.status == DownloadStatus::Cancelled) {
        const DownloadRequest request = job.request;
        const bool autoConvert = job.autoConvert;
        const QString conversionFormat = job.conversionFormat;
        QAction *retryAction = menu.addAction(QStringLiteral("🔄 Tentar Novamente"));
        connect(retryAction, &QAction::triggered, this, [this, request, autoConvert, conversionFormat]() {
            const EnqueueResult result = m_downloadManager->enqueueDownload(request);
            if (result.accepted) {
                UiDownloadJob newJob;
                newJob.request = request;
                newJob.autoConvert = autoConvert;
                newJob.conversionFormat = conversionFormat;
                newJob.statusText = QStringLiteral("Aguardando");
                m_downloadJobs.insert(result.id, newJob);
                logMessage(QStringLiteral("[Fila de Downloads] Reiniciando download: %1").arg(request.url.toString()));
            }
        });
    }

    if (!job.terminal) {
        QAction *cancelAction = menu.addAction(QStringLiteral("⏹ Cancelar Download"));
        connect(cancelAction, &QAction::triggered, this, [this, id]() {
            m_downloadManager->cancelDownload(id);
            m_conversionManager->cancelByDownloadId(id);
        });
    }

    menu.exec(m_downloadsQueueTable->viewport()->mapToGlobal(pos));
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls() || event->mimeData()->hasText()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QString droppedText;
    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty()) {
            droppedText = urls.first().toString();
        }
    }
    if (droppedText.isEmpty() && event->mimeData()->hasText()) {
        droppedText = event->mimeData()->text().trimmed();
    }
    if (!droppedText.isEmpty()) {
        event->acceptProposedAction();
        showDownloadsPage();
        if (m_urlInput) {
            m_urlInput->setText(droppedText);
            m_urlInput->setFocus();
        }
        logMessage(QStringLiteral("[Interface] Link inserido via Arrastar e Soltar: %1").arg(droppedText));
    }
}

// ==========================================
// DIÁLOGO MODAL DE SELEÇÃO DE FORMATO
// ==========================================
bool MainWindow::showFormatSelectionDialog(const MediaMetadata &metadata, int itemCount,
                                           QString &outQuality, QString &outTimeRange,
                                           bool &outDoConvert, QString &outConvertFormat,
                                           QString &outCustomOutputDir)
{
    FormatSelectionDialog dialog(
        metadata, itemCount, m_qualityCombo->currentIndex(), m_timeRangeInput->text(),
        m_outputDirInput->text(), m_gpuDetector.hasHardwareAcceleration(),
        QString::fromStdString(m_gpuDetector.getRecommendedCodec()), styleSheet(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    const FormatSelectionResult selection = dialog.result();
    if (selection.qualityIndex >= 0 && selection.qualityIndex < m_qualityCombo->count()) {
        m_qualityCombo->setCurrentIndex(selection.qualityIndex);
        outQuality = m_qualityCombo->currentText();
    } else {
        outQuality = m_qualityCombo->currentText();
    }
    outTimeRange = selection.timeRange;
    m_timeRangeInput->setText(outTimeRange);
    outDoConvert = selection.doConvert;
    outConvertFormat = selection.convertFormat;
    outCustomOutputDir = selection.customOutputDir;
    if (outCustomOutputDir.isEmpty()) {
        outCustomOutputDir = m_outputDirInput->text().trimmed();
    }
    return true;
}

void MainWindow::onBrowseClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Escolha a Pasta de Destino Padrão para os Downloads", m_outputDirInput->text());
    if (!dir.isEmpty()) {
        m_outputDirInput->setText(dir);
        m_currentDownloadDir.clear();
        m_settings.outputFolder = dir;
        m_settings.save();
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
    m_settings.maxConcurrentDownloads = value;
    m_settings.save();
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
    if (m_updateCoordinator) {
        m_updateCoordinator->tryStartPendingAppUpdate();
    }
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
    if (m_updateCoordinator && m_updateCoordinator->isInstallingAppUpdate()) {
        QMessageBox::information(this, "Atualização em andamento",
                                 "A atualização autenticada está sendo instalada. Aguarde a conclusão.");
        event->ignore();
        return;
    }
    const bool playlistPreviewBusy = m_playlistPreviewService && m_playlistPreviewService->isBusy();
    if (!m_downloadManager->hasWork() && !m_conversionManager->hasWork() && !playlistPreviewBusy) {
        event->accept();
        return;
    }
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, "Trabalho em andamento",
        "Há downloads, conversões ou consultas de playlist pendentes. Deseja cancelar toda a sessão e fechar?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        event->ignore();
        return;
    }
    m_closing = true;
    m_downloadManager->cancelAll();
    m_conversionManager->cancelAllAutomatic();
    if (playlistPreviewBusy) {
        m_playlistPreviewService->cancel();
    }
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

void MainWindow::initializeLogFile()
{
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (appDataPath.isEmpty()) {
        return;
    }

    const QDir logDirectory(QDir(appDataPath).filePath(QStringLiteral("logs")));
    if (!QDir().mkpath(logDirectory.absolutePath())) {
        return;
    }

    m_logFilePath = logDirectory.filePath(
        QStringLiteral("prism-%1.log").arg(QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"))));
    m_logFile.setFileName(m_logFilePath);
    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_logFilePath.clear();
    }
}

void MainWindow::logMessage(const QString &msg)
{
    const QStringList messages = msg.split(QRegularExpression(QStringLiteral("[\\r\\n]+")),
                                           Qt::SkipEmptyParts);
    for (const QString &message : messages) {
        const QString line = normalizedLogLine(message);
        m_allLogs.append(line);
        if (m_logFile.isOpen()) {
            m_logFile.write(line.toUtf8());
            m_logFile.putChar('\n');
        }
        if (m_logEdit && shouldShowLogLine(line)) {
            m_pendingLogLines.append(line);
        }
    }
    if (m_allLogs.size() > kMaximumLogEntries) {
        m_allLogs.erase(m_allLogs.begin(), m_allLogs.begin() + (m_allLogs.size() - kMaximumLogEntries));
    }
    if (m_logFile.isOpen() && !messages.isEmpty()) {
        m_logFile.flush();
    }
    if (m_logEdit && !m_pendingLogLines.isEmpty()) {
        if (m_logFlushTimer && !m_logFlushTimer->isActive()) {
            m_logFlushTimer->start();
        }
    }
    updateLogSummary();
}

bool MainWindow::shouldShowLogLine(const QString &line) const
{
    if (m_logSearchInput && !m_logSearchInput->text().trimmed().isEmpty()
        && !line.contains(m_logSearchInput->text().trimmed(), Qt::CaseInsensitive)) {
        return false;
    }
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
        return logLevelForMessage(line) != LogLevel::Info;
    }
    if (m_logFilterMode == 3) {
        // Apenas Gerais (Sistema, GPU, Biblioteca, Updater, Destino)
        return line.contains("[System]", Qt::CaseInsensitive) ||
               line.contains("[GPUDetector]", Qt::CaseInsensitive) ||
               line.contains("[Biblioteca]", Qt::CaseInsensitive) ||
               line.contains("[Updater]", Qt::CaseInsensitive) ||
               line.contains("[Motor Extrator]", Qt::CaseInsensitive) ||
               line.contains("[Destino", Qt::CaseInsensitive) ||
               line.contains("[Playlist]", Qt::CaseInsensitive) ||
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
    updateLogSummary();
}

void MainWindow::updateLogSummary()
{
    if (!m_logSummaryLabel) {
        return;
    }

    int visible = 0;
    int errors = 0;
    int warnings = 0;
    for (const QString &line : m_allLogs) {
        const LogLevel level = logLevelForMessage(line);
        if (level == LogLevel::Error) {
            ++errors;
        } else if (level == LogLevel::Warning) {
            ++warnings;
        }
        if (shouldShowLogLine(line)) {
            ++visible;
        }
    }
    m_logSummaryLabel->setText(QStringLiteral("Total: %1 | Visíveis: %2 | Erros: %3 | Alertas: %4")
                                   .arg(m_allLogs.size()).arg(visible).arg(errors).arg(warnings));
    if (!m_logFilePath.isEmpty()) {
        m_logSummaryLabel->setToolTip(
            QStringLiteral("Arquivo de log da sessão:\n%1")
                .arg(QDir::toNativeSeparators(m_logFilePath)));
    }
}

void MainWindow::setupStyles()
{
    setStyleSheet(PrismStyleSheet::mainWindow());
}

void MainWindow::checkForUpdates(bool silent)
{
    if (m_updateCoordinator) {
        m_updateCoordinator->checkForUpdates(silent);
    }
}

void MainWindow::requestAppUpdate()
{
    if (m_updateCoordinator) {
        m_updateCoordinator->requestAppUpdate();
    }
}

void MainWindow::checkYtDlpUpdates(bool silent)
{
    if (m_updateCoordinator) {
        m_updateCoordinator->checkYtDlpUpdates(silent);
    }
}

void MainWindow::updateYtdlpEngine()
{
    if (m_updateCoordinator) {
        m_updateCoordinator->updateYtdlpEngine();
    }
}
