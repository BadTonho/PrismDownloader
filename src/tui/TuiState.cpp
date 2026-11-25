#include "TuiState.h"

#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace {

std::string toLowerCopy(const std::string &input)
{
    std::string out = input;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return out;
}

bool containsIgnoreCase(const std::string &haystack, const std::string &needle)
{
    return toLowerCopy(haystack).find(toLowerCopy(needle)) != std::string::npos;
}

} // namespace

std::string TuiState::statusLabel(DownloadStatus status)
{
    switch (status) {
    case DownloadStatus::Queued:
        return "Na fila";
    case DownloadStatus::Downloading:
        return "Baixando";
    case DownloadStatus::Muxing:
        return "Mesclando";
    case DownloadStatus::ConvertingGPU:
        return "Convertendo";
    case DownloadStatus::Cancelling:
        return "Cancelando";
    case DownloadStatus::Completed:
        return "Concluido";
    case DownloadStatus::Error:
        return "Erro";
    case DownloadStatus::Cancelled:
        return "Cancelado";
    }
    return "Desconhecido";
}

std::string TuiState::formatFileSize(std::uint64_t bytes)
{
    char buffer[64];
    constexpr double kKb = 1024.0;
    constexpr double kMb = 1024.0 * 1024.0;
    constexpr double kGb = 1024.0 * 1024.0 * 1024.0;

    if (static_cast<double>(bytes) >= kGb) {
        std::snprintf(buffer, sizeof(buffer), "%.2f GB", static_cast<double>(bytes) / kGb);
    } else if (static_cast<double>(bytes) >= kMb) {
        std::snprintf(buffer, sizeof(buffer), "%.1f MB", static_cast<double>(bytes) / kMb);
    } else if (static_cast<double>(bytes) >= kKb) {
        std::snprintf(buffer, sizeof(buffer), "%.1f KB", static_cast<double>(bytes) / kKb);
    } else {
        std::snprintf(buffer, sizeof(buffer), "%llu B", static_cast<unsigned long long>(bytes));
    }
    return std::string(buffer);
}

std::vector<std::string> TuiState::qualityPresets()
{
    return {
        "Melhor Disponivel (4K / 1080p)",
        "1080p Full HD",
        "720p HD",
        "480p SD",
        "MP3 (Apenas Audio)"
    };
}

std::vector<std::string> TuiState::converterFormats()
{
    return {
        "MP4 (H.264)",
        "MP4 (HEVC / H.265)",
        "MKV (Matroska)",
        "MP3 (320kbps)",
        "WAV (Sem Compressao)",
        "WEBM (Web)"
    };
}

std::vector<std::string> TuiState::filterLogs(const std::vector<std::string> &logs, int filterMode)
{
    if (filterMode == 0) {
        return logs;
    }

    std::vector<std::string> result;
    result.reserve(logs.size());
    for (const std::string &line : logs) {
        if (filterMode == 1) {
            if (containsIgnoreCase(line, "[download") ||
                containsIgnoreCase(line, "[convers") ||
                containsIgnoreCase(line, "[yt-dlp") ||
                containsIgnoreCase(line, "[ffmpeg")) {
                result.push_back(line);
            }
        } else if (filterMode == 2) {
            if (containsIgnoreCase(line, "erro") ||
                containsIgnoreCase(line, "error") ||
                containsIgnoreCase(line, "falha") ||
                containsIgnoreCase(line, "aviso") ||
                containsIgnoreCase(line, "warning")) {
                result.push_back(line);
            }
        }
    }
    return result;
}

std::vector<TuiLibraryItem> TuiState::scanLibraryDirectory(const std::string &directoryPath)
{
    std::vector<TuiLibraryItem> items;
    const QDir directory(QString::fromUtf8(directoryPath.c_str()));
    if (!directory.exists()) {
        return items;
    }

    const QStringList filters{
        QStringLiteral("*.mp4"), QStringLiteral("*.mp3"), QStringLiteral("*.mkv"),
        QStringLiteral("*.webm"), QStringLiteral("*.m4a"), QStringLiteral("*.avi"),
        QStringLiteral("*.flv"), QStringLiteral("*.wav")};
    const QFileInfoList entries = directory.entryInfoList(
        filters, QDir::Files | QDir::NoSymLinks, QDir::Time);

    items.reserve(static_cast<std::size_t>(entries.size()));
    for (const QFileInfo &info : entries) {
        TuiLibraryItem item;
        item.name = info.fileName().toUtf8().toStdString();
        item.fullPath = info.absoluteFilePath().toUtf8().toStdString();
        item.extension = info.suffix().toUpper().toUtf8().toStdString();
        item.sizeBytes = static_cast<std::uint64_t>(std::max<qint64>(0, info.size()));
        items.push_back(std::move(item));
    }
    return items;
}

TuiSnapshot TuiState::snapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    TuiSnapshot snap;
    snap.queue = m_queue;
    snap.activeDownloads = m_activeDownloads;
    snap.pendingDownloads = m_pendingDownloads;
    snap.concurrencyLimit = m_concurrencyLimit;
    snap.outputFolder = m_outputFolder;
    snap.library = m_library;
    snap.converter = m_converter;
    snap.system = m_system;
    snap.logs.assign(m_logs.begin(), m_logs.end());
    snap.statusBanner = m_statusBanner;
    snap.statusIsError = m_statusIsError;
    return snap;
}

void TuiState::setOutputFolder(const std::string &folder)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_outputFolder = folder;
}

void TuiState::setConcurrencyLimit(int limit)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_concurrencyLimit = std::clamp(limit, 1, 5);
}

void TuiState::setBanner(const std::string &message, bool isError)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_statusBanner = message;
    m_statusIsError = isError;
}

void TuiState::addQueueItem(const TuiQueueItem &item)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_queue.push_back(item);
}

void TuiState::updateQueueProgress(std::uint64_t id,
                                   double percent,
                                   const std::string &speed,
                                   const std::string &eta)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (TuiQueueItem &item : m_queue) {
        if (item.id == id) {
            item.progress = std::clamp(percent, 0.0, 100.0);
            item.speed = speed;
            item.eta = eta;
            break;
        }
    }
}

void TuiState::updateQueueStatus(std::uint64_t id,
                                 DownloadStatus status,
                                 const std::string &message)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (TuiQueueItem &item : m_queue) {
        if (item.id == id) {
            item.status = status;
            item.statusText = message.empty() ? statusLabel(status) : message;
            if (status == DownloadStatus::Completed) {
                item.progress = 100.0;
            }
            break;
        }
    }
}

void TuiState::markQueueCompleted(std::uint64_t id, const std::string &outputPath)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (TuiQueueItem &item : m_queue) {
        if (item.id == id) {
            item.status = DownloadStatus::Completed;
            item.progress = 100.0;
            item.statusText = statusLabel(DownloadStatus::Completed);
            item.outputPath = outputPath;
            if (!outputPath.empty()) {
                const QFileInfo info(QString::fromUtf8(outputPath.c_str()));
                if (!info.fileName().isEmpty()) {
                    item.title = info.fileName().toUtf8().toStdString();
                }
            }
            break;
        }
    }
}

void TuiState::setQueueCounts(int active, int pending)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_activeDownloads = std::max(0, active);
    m_pendingDownloads = std::max(0, pending);
}

void TuiState::setLibraryItems(std::vector<TuiLibraryItem> items)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_library = std::move(items);
}

void TuiState::setConverterState(const TuiConverterState &state)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_converter = state;
}

void TuiState::updateConverterProgress(double percent)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_converter.progress = std::clamp(percent, 0.0, 100.0);
}

void TuiState::updateConverterStatus(const std::string &text, bool running)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_converter.statusText = text;
    m_converter.running = running;
}

void TuiState::setSystemInfo(const TuiSystemInfo &info)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_system = info;
}

void TuiState::appendLog(const std::string &line)
{
    if (line.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logs.push_back(line);
    while (m_logs.size() > kMaxLogs) {
        m_logs.pop_front();
    }
}

void TuiState::clearLogs()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logs.clear();
}
