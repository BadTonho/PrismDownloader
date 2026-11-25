#include "TuiBackend.h"

#include "AppSettings.h"
#include "ConversionManager.h"
#include "DownloadManager.h"
#include "GPUDetector.h"
#include "MediaToolResolver.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QObject>
#include <QStandardPaths>
#include <QUrl>

namespace {

std::string timestamped(const std::string &prefix, const std::string &message)
{
    const QString timeStr = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    return "[" + timeStr.toStdString() + "] " + prefix + " " + message;
}

} // namespace

TuiBackend::TuiBackend(TuiState &state, std::string appVersion)
    : m_state(state),
      m_appVersion(std::move(appVersion))
{
}

TuiBackend::~TuiBackend()
{
    stop();
}

void TuiBackend::setStateChangedCallback(std::function<void()> callback)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_onStateChanged = std::move(callback);
}

void TuiBackend::start(int argc, char **argv)
{
    m_stopped.store(false);
    m_thread = std::thread([this, argc, argv]() {
        runLoop(argc, argv);
    });

    std::unique_lock<std::mutex> lock(m_readyMutex);
    m_readyCv.wait(lock, [this]() { return m_ready; });
}

void TuiBackend::stop()
{
    if (m_stopped.exchange(true)) {
        return;
    }

    if (m_dispatcher) {
        QMetaObject::invokeMethod(m_dispatcher, [this]() {
            if (m_downloadManager) {
                m_downloadManager->cancelAll();
            }
            if (m_conversionManager && m_activeConversionId != 0) {
                m_conversionManager->cancelConversion(m_activeConversionId);
            }
            if (m_qtApp) {
                m_qtApp->quit();
            }
        }, Qt::QueuedConnection);
    }

    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void TuiBackend::notifyUi()
{
    std::function<void()> cb;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        cb = m_onStateChanged;
    }
    if (cb) {
        cb();
    }
}

void TuiBackend::runLoop(int argc, char **argv)
{
    int localArgc = argc;
    QCoreApplication app(localArgc, argv);
    app.setOrganizationName(QStringLiteral("TonhoStudios"));
    app.setApplicationName(QStringLiteral("PrismDownloader"));
    app.setApplicationVersion(QString::fromUtf8(m_appVersion.c_str()));

    QObject dispatcher;
    DownloadManager downloadManager(&dispatcher);
    ConversionManager conversionManager(&dispatcher);
    GPUDetector gpuDetector;

    m_qtApp = &app;
    m_dispatcher = &dispatcher;
    m_downloadManager = &downloadManager;
    m_conversionManager = &conversionManager;
    m_gpuDetector = &gpuDetector;

    const AppSettings settings = AppSettings::load();
    QString folder = settings.outputFolder;
    if (folder.isEmpty() || !QDir(folder).exists()) {
        folder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (folder.isEmpty()) {
            folder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        }
    }

    const int concurrency = settings.maxConcurrentDownloads > 0 ? settings.maxConcurrentDownloads : 2;
    downloadManager.setConcurrencyLimit(concurrency);
    m_state.setConcurrencyLimit(concurrency);
    m_state.setOutputFolder(folder.toUtf8().toStdString());
    m_state.setLibraryItems(TuiState::scanLibraryDirectory(folder.toUtf8().toStdString()));

    QObject::connect(&downloadManager, &DownloadManager::jobProgress, &dispatcher,
                     [this](DownloadId id, double percent, const QString &speed, const QString &eta) {
                         m_state.updateQueueProgress(
                             id, percent, speed.toUtf8().toStdString(), eta.toUtf8().toStdString());
                         notifyUi();
                     });

    QObject::connect(&downloadManager, &DownloadManager::jobStatus, &dispatcher,
                     [this](DownloadId id, DownloadStatus status, const QString &message) {
                         m_state.updateQueueStatus(id, status, message.toUtf8().toStdString());
                         m_state.appendLog(timestamped(
                             "[Download #" + std::to_string(id) + "]",
                             TuiState::statusLabel(status) + (message.isEmpty() ? "" : ": " + message.toUtf8().toStdString())));
                         notifyUi();
                     });

    QObject::connect(&downloadManager, &DownloadManager::jobCompleted, &dispatcher,
                     [this](DownloadId id, const QString &filePath) {
                         m_state.markQueueCompleted(id, filePath.toUtf8().toStdString());
                         m_state.appendLog(timestamped(
                             "[Download #" + std::to_string(id) + "]",
                             "Concluido: " + filePath.toUtf8().toStdString()));
                         const auto snap = m_state.snapshot();
                         m_state.setLibraryItems(TuiState::scanLibraryDirectory(snap.outputFolder));
                         m_state.setBanner("Download #" + std::to_string(id) + " concluido.", false);
                         notifyUi();
                     });

    QObject::connect(&downloadManager, &DownloadManager::jobLog, &dispatcher,
                     [this](DownloadId id, const QString &message) {
                         m_state.appendLog(timestamped(
                             "[yt-dlp #" + std::to_string(id) + "]",
                             message.toUtf8().toStdString()));
                         notifyUi();
                     });

    QObject::connect(&downloadManager, &DownloadManager::queueStateChanged, &dispatcher,
                     [this](int active, int pending) {
                         m_state.setQueueCounts(active, pending);
                         notifyUi();
                     });

    QObject::connect(&conversionManager, &ConversionManager::conversionProgress, &dispatcher,
                     [this](ConversionId, DownloadId, double percent) {
                         m_state.updateConverterProgress(percent);
                         notifyUi();
                     });

    QObject::connect(&conversionManager, &ConversionManager::conversionStatus, &dispatcher,
                     [this](ConversionId, DownloadId, const QString &message) {
                         m_state.updateConverterStatus(message.toUtf8().toStdString(), true);
                         m_state.appendLog(timestamped("[Conversor]", message.toUtf8().toStdString()));
                         notifyUi();
                     });

    QObject::connect(&conversionManager, &ConversionManager::conversionCompleted, &dispatcher,
                     [this](ConversionId, DownloadId, const QString &outputFile) {
                         m_activeConversionId = 0;
                         TuiConverterState conv;
                         conv.running = false;
                         conv.progress = 100.0;
                         conv.statusText = "Conversao concluida: " + outputFile.toUtf8().toStdString();
                         conv.lastOutputPath = outputFile.toUtf8().toStdString();
                         m_state.setConverterState(conv);
                         m_state.appendLog(timestamped("[Conversor]", conv.statusText));
                         const auto snap = m_state.snapshot();
                         m_state.setLibraryItems(TuiState::scanLibraryDirectory(snap.outputFolder));
                         m_state.setBanner("Conversao concluida com sucesso.", false);
                         notifyUi();
                     });

    QObject::connect(&conversionManager, &ConversionManager::conversionFailed, &dispatcher,
                     [this](ConversionId, DownloadId, const QString &message) {
                         m_activeConversionId = 0;
                         m_state.updateConverterStatus("Erro: " + message.toUtf8().toStdString(), false);
                         m_state.appendLog(timestamped("[Conversor]", "Erro: " + message.toUtf8().toStdString()));
                         m_state.setBanner("Falha na conversao: " + message.toUtf8().toStdString(), true);
                         notifyUi();
                     });

    QObject::connect(&conversionManager, &ConversionManager::conversionCancelled, &dispatcher,
                     [this](ConversionId, DownloadId) {
                         m_activeConversionId = 0;
                         m_state.updateConverterStatus("Conversao cancelada.", false);
                         m_state.appendLog(timestamped("[Conversor]", "Conversao cancelada pelo usuario."));
                         m_state.setBanner("Conversao cancelada.", false);
                         notifyUi();
                     });

    QObject::connect(&conversionManager, &ConversionManager::conversionLog, &dispatcher,
                     [this](ConversionId, DownloadId, const QString &message) {
                         m_state.appendLog(timestamped("[FFmpeg]", message.toUtf8().toStdString()));
                         notifyUi();
                     });

    m_state.appendLog(timestamped("[Sistema]", "Prism CLI v" + m_appVersion + " iniciado."));

    {
        std::lock_guard<std::mutex> lock(m_readyMutex);
        m_ready = true;
    }
    m_readyCv.notify_all();

    QMetaObject::invokeMethod(&dispatcher, [this]() {
        probeSystemOnBackendThread();
    }, Qt::QueuedConnection);

    app.exec();

    m_downloadManager = nullptr;
    m_conversionManager = nullptr;
    m_gpuDetector = nullptr;
    m_dispatcher = nullptr;
    m_qtApp = nullptr;
}

void TuiBackend::probeSystemOnBackendThread()
{
    if (!m_gpuDetector) {
        return;
    }
    m_gpuDetector->detect(false);

    const MediaToolInfo ytdlp = MediaToolResolver::resolveInfo(MediaTool::YtDlp);
    const MediaToolInfo ffmpeg = MediaToolResolver::resolveInfo(MediaTool::Ffmpeg);

    TuiSystemInfo info;
    info.appVersion = m_appVersion;
    info.gpuName = m_gpuDetector->getGPUName();
    info.gpuCodec = m_gpuDetector->getRecommendedCodec();
    info.hardwareAcceleration = m_gpuDetector->hasHardwareAcceleration();
    info.ytdlpPath = ytdlp.path.toUtf8().toStdString();
    info.ytdlpSource = MediaToolResolver::sourceLabel(ytdlp.source).toUtf8().toStdString();
    info.ytdlpVersion = ytdlp.isAvailable()
        ? (ytdlp.version.isEmpty() ? "Detectado" : ytdlp.version.toUtf8().toStdString())
        : "Nao encontrado";
    info.ffmpegPath = ffmpeg.path.toUtf8().toStdString();
    info.ffmpegSource = ffmpeg.isAvailable()
        ? MediaToolResolver::sourceLabel(ffmpeg.source).toUtf8().toStdString()
        : "Nao encontrado";

    m_state.setSystemInfo(info);
    m_state.appendLog(timestamped("[GPUDetector]", info.gpuName + " (" + info.gpuCodec + ")"));
    notifyUi();
}

void TuiBackend::enqueueDownload(const std::string &url,
                                 const std::string &quality,
                                 const std::string &outputDir)
{
    if (!m_dispatcher) {
        return;
    }
    QMetaObject::invokeMethod(m_dispatcher, [this, url, quality, outputDir]() {
        if (!m_downloadManager) {
            return;
        }
        const QString trimmedUrl = QString::fromUtf8(url.c_str()).trimmed();
        const QUrl parsed = QUrl::fromUserInput(trimmedUrl);
        if (trimmedUrl.isEmpty() || !parsed.isValid() ||
            (parsed.scheme() != QStringLiteral("http") && parsed.scheme() != QStringLiteral("https"))) {
            m_state.setBanner("URL invalida. Informe um link http:// ou https://.", true);
            notifyUi();
            return;
        }

        const QString targetDir = QString::fromUtf8(outputDir.c_str()).trimmed();
        if (targetDir.isEmpty() || !QDir(targetDir).exists()) {
            m_state.setBanner("Pasta de destino inexistente: " + outputDir, true);
            notifyUi();
            return;
        }

        DownloadRequest req;
        req.url = parsed;
        req.quality = QString::fromUtf8(quality.c_str());
        req.outputDirectory = targetDir;

        const EnqueueResult res = m_downloadManager->enqueueDownload(req);
        if (!res.accepted) {
            const std::string err = res.error.toUtf8().toStdString();
            m_state.setBanner("Nao foi possivel enfileirar: " + err, true);
            m_state.appendLog(timestamped("[Fila]", "Recusado: " + err));
            notifyUi();
            return;
        }

        TuiQueueItem item;
        item.id = res.id;
        item.url = trimmedUrl.toUtf8().toStdString();
        item.title = trimmedUrl.toUtf8().toStdString();
        item.quality = quality;
        item.status = DownloadStatus::Queued;
        item.statusText = TuiState::statusLabel(DownloadStatus::Queued);
        m_state.addQueueItem(item);
        m_state.setBanner("Item #" + std::to_string(res.id) + " adicionado a fila (" + quality + ").", false);
        m_state.appendLog(timestamped("[Fila]", "Adicionado #" + std::to_string(res.id) + " (" + quality + ")"));
        notifyUi();
    }, Qt::QueuedConnection);
}

void TuiBackend::cancelDownload(std::uint64_t id)
{
    if (!m_dispatcher || id == 0) {
        return;
    }
    QMetaObject::invokeMethod(m_dispatcher, [this, id]() {
        if (!m_downloadManager) {
            return;
        }
        if (m_downloadManager->cancelDownload(id)) {
            m_state.setBanner("Cancelamento solicitado para o item #" + std::to_string(id) + ".", false);
        } else {
            m_state.setBanner("Item #" + std::to_string(id) + " nao esta ativo ou pendente.", true);
        }
        notifyUi();
    }, Qt::QueuedConnection);
}

void TuiBackend::cancelAllDownloads()
{
    if (!m_dispatcher) {
        return;
    }
    QMetaObject::invokeMethod(m_dispatcher, [this]() {
        if (!m_downloadManager) {
            return;
        }
        m_downloadManager->cancelAll();
        m_state.setBanner("Cancelamento de toda a fila solicitado.", false);
        notifyUi();
    }, Qt::QueuedConnection);
}

void TuiBackend::setConcurrency(int limit)
{
    if (!m_dispatcher) {
        return;
    }
    QMetaObject::invokeMethod(m_dispatcher, [this, limit]() {
        if (!m_downloadManager) {
            return;
        }
        const int clamped = std::clamp(limit, 1, 5);
        m_downloadManager->setConcurrencyLimit(clamped);
        m_state.setConcurrencyLimit(clamped);
        AppSettings settings = AppSettings::load();
        settings.maxConcurrentDownloads = clamped;
        settings.save();
        m_state.setBanner("Downloads simultaneos ajustados para " + std::to_string(clamped) + ".", false);
        notifyUi();
    }, Qt::QueuedConnection);
}

void TuiBackend::setOutputFolder(const std::string &folder)
{
    if (!m_dispatcher) {
        return;
    }
    QMetaObject::invokeMethod(m_dispatcher, [this, folder]() {
        const QString qFolder = QString::fromUtf8(folder.c_str()).trimmed();
        if (qFolder.isEmpty() || !QDir(qFolder).exists()) {
            m_state.setBanner("Diretorio invalido ou inexistente.", true);
            notifyUi();
            return;
        }
        m_state.setOutputFolder(qFolder.toUtf8().toStdString());
        m_state.setLibraryItems(TuiState::scanLibraryDirectory(qFolder.toUtf8().toStdString()));
        AppSettings settings = AppSettings::load();
        settings.outputFolder = qFolder;
        settings.save();
        m_state.setBanner("Pasta de destino atualizada.", false);
        notifyUi();
    }, Qt::QueuedConnection);
}

void TuiBackend::refreshLibrary(const std::string &folder)
{
    if (!m_dispatcher) {
        return;
    }
    QMetaObject::invokeMethod(m_dispatcher, [this, folder]() {
        auto items = TuiState::scanLibraryDirectory(folder);
        const std::size_t count = items.size();
        m_state.setLibraryItems(std::move(items));
        m_state.setBanner("Biblioteca atualizada: " + std::to_string(count) + " arquivo(s).", false);
        notifyUi();
    }, Qt::QueuedConnection);
}

void TuiBackend::startConversion(const std::string &inputFile,
                                 const std::string &format,
                                 const std::string &outputDir)
{
    if (!m_dispatcher) {
        return;
    }
    QMetaObject::invokeMethod(m_dispatcher, [this, inputFile, format, outputDir]() {
        if (!m_conversionManager || !m_gpuDetector) {
            return;
        }
        const QString qInput = QString::fromUtf8(inputFile.c_str()).trimmed();
        const QFileInfo inputInfo(qInput);
        if (qInput.isEmpty() || !inputInfo.exists() || !inputInfo.isFile()) {
            m_state.setBanner("Arquivo de origem nao encontrado para conversao.", true);
            notifyUi();
            return;
        }

        ConversionRequest req;
        req.inputFile = inputInfo.absoluteFilePath();
        req.format = QString::fromUtf8(format.c_str());
        req.outputDirectory = outputDir.empty()
            ? inputInfo.absolutePath()
            : QString::fromUtf8(outputDir.c_str());
        req.gpuType = m_gpuDetector->getGPUType();
        req.gpuCodec = QString::fromUtf8(m_gpuDetector->getRecommendedCodec().c_str());
        req.gpuDevice = QString::fromUtf8(m_gpuDetector->getHardwareDevice().c_str());

        const ConversionEnqueueResult res = m_conversionManager->enqueueConversion(req);
        if (!res.accepted) {
            m_state.setBanner("Falha ao iniciar conversao: " + res.error.toUtf8().toStdString(), true);
            notifyUi();
            return;
        }

        m_activeConversionId = res.id;
        TuiConverterState conv;
        conv.activeId = res.id;
        conv.running = true;
        conv.progress = 0.0;
        conv.statusText = "Convertendo " + inputInfo.fileName().toUtf8().toStdString() + " -> " + format;
        m_state.setConverterState(conv);
        m_state.setBanner("Conversao iniciada (" + format + ").", false);
        notifyUi();
    }, Qt::QueuedConnection);
}

void TuiBackend::cancelConversion()
{
    if (!m_dispatcher) {
        return;
    }
    QMetaObject::invokeMethod(m_dispatcher, [this]() {
        if (!m_conversionManager || m_activeConversionId == 0) {
            m_state.setBanner("Nenhuma conversao ativa no momento.", false);
            notifyUi();
            return;
        }
        m_conversionManager->cancelConversion(m_activeConversionId);
        notifyUi();
    }, Qt::QueuedConnection);
}

void TuiBackend::refreshSystemInfo()
{
    if (!m_dispatcher) {
        return;
    }
    QMetaObject::invokeMethod(m_dispatcher, [this]() {
        MediaToolResolver::clearVersionCache();
        probeSystemOnBackendThread();
        m_state.setBanner("Diagnostico de hardware e ferramentas atualizado.", false);
        notifyUi();
    }, Qt::QueuedConnection);
}
