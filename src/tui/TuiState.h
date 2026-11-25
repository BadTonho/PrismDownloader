#ifndef PRISM_TUI_STATE_H
#define PRISM_TUI_STATE_H

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include "MediaItem.h"

struct TuiQueueItem {
    std::uint64_t id{0};
    std::string url;
    std::string title;
    std::string quality;
    double progress{0.0};
    std::string speed;
    std::string eta;
    DownloadStatus status{DownloadStatus::Queued};
    std::string statusText;
    std::string outputPath;
};

struct TuiLibraryItem {
    std::string name;
    std::string fullPath;
    std::string extension;
    std::uint64_t sizeBytes{0};
};

struct TuiConverterState {
    std::uint64_t activeId{0};
    bool running{false};
    double progress{0.0};
    std::string statusText{"Aguardando selecao de arquivo."};
    std::string lastOutputPath;
};

struct TuiSystemInfo {
    std::string appVersion;
    std::string gpuName{"Sondando hardware..."};
    std::string gpuCodec{"---"};
    bool hardwareAcceleration{false};
    std::string ytdlpVersion{"Verificando..."};
    std::string ytdlpSource{"---"};
    std::string ytdlpPath;
    std::string ffmpegSource{"---"};
    std::string ffmpegPath;
};

struct TuiSnapshot {
    std::vector<TuiQueueItem> queue;
    int activeDownloads{0};
    int pendingDownloads{0};
    int concurrencyLimit{2};
    std::string outputFolder;
    std::vector<TuiLibraryItem> library;
    TuiConverterState converter;
    TuiSystemInfo system;
    std::vector<std::string> logs;
    std::string statusBanner;
    bool statusIsError{false};
};

class TuiState final {
public:
    static constexpr std::size_t kMaxLogs = 500;

    static std::string statusLabel(DownloadStatus status);
    static std::string formatFileSize(std::uint64_t bytes);
    static std::vector<std::string> qualityPresets();
    static std::vector<std::string> converterFormats();
    static std::vector<std::string> filterLogs(const std::vector<std::string> &logs, int filterMode);
    static std::vector<TuiLibraryItem> scanLibraryDirectory(const std::string &directoryPath);

    TuiSnapshot snapshot() const;

    void setOutputFolder(const std::string &folder);
    void setConcurrencyLimit(int limit);
    void setBanner(const std::string &message, bool isError = false);

    void addQueueItem(const TuiQueueItem &item);
    void updateQueueProgress(std::uint64_t id, double percent, const std::string &speed, const std::string &eta);
    void updateQueueStatus(std::uint64_t id, DownloadStatus status, const std::string &message);
    void markQueueCompleted(std::uint64_t id, const std::string &outputPath);
    void setQueueCounts(int active, int pending);

    void setLibraryItems(std::vector<TuiLibraryItem> items);

    void setConverterState(const TuiConverterState &state);
    void updateConverterProgress(double percent);
    void updateConverterStatus(const std::string &text, bool running);

    void setSystemInfo(const TuiSystemInfo &info);
    void appendLog(const std::string &line);
    void clearLogs();

private:
    mutable std::mutex m_mutex;
    std::vector<TuiQueueItem> m_queue;
    int m_activeDownloads{0};
    int m_pendingDownloads{0};
    int m_concurrencyLimit{2};
    std::string m_outputFolder;
    std::vector<TuiLibraryItem> m_library;
    TuiConverterState m_converter;
    TuiSystemInfo m_system;
    std::deque<std::string> m_logs;
    std::string m_statusBanner{"Pronto. Cole uma URL e pressione Enter para adicionar a fila."};
    bool m_statusIsError{false};
};

#endif // PRISM_TUI_STATE_H
