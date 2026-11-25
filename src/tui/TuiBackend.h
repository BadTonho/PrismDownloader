#ifndef PRISM_TUI_BACKEND_H
#define PRISM_TUI_BACKEND_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "TuiState.h"

class QCoreApplication;
class QObject;
class DownloadManager;
class ConversionManager;
class GPUDetector;

class TuiBackend final {
public:
    TuiBackend(TuiState &state, std::string appVersion);
    ~TuiBackend();

    TuiBackend(const TuiBackend &) = delete;
    TuiBackend &operator=(const TuiBackend &) = delete;

    void setStateChangedCallback(std::function<void()> callback);
    void start(int argc, char **argv);
    void stop();

    void enqueueDownload(const std::string &url,
                         const std::string &quality,
                         const std::string &outputDir);
    void cancelDownload(std::uint64_t id);
    void cancelAllDownloads();
    void setConcurrency(int limit);
    void setOutputFolder(const std::string &folder);
    void refreshLibrary(const std::string &folder);
    void startConversion(const std::string &inputFile,
                         const std::string &format,
                         const std::string &outputDir);
    void cancelConversion();
    void refreshSystemInfo();

private:
    void runLoop(int argc, char **argv);
    void notifyUi();
    void probeSystemOnBackendThread();

    TuiState &m_state;
    std::string m_appVersion;

    std::mutex m_callbackMutex;
    std::function<void()> m_onStateChanged;

    std::thread m_thread;
    std::mutex m_readyMutex;
    std::condition_variable m_readyCv;
    bool m_ready{false};
    std::atomic<bool> m_stopped{false};

    QCoreApplication *m_qtApp{nullptr};
    QObject *m_dispatcher{nullptr};
    DownloadManager *m_downloadManager{nullptr};
    ConversionManager *m_conversionManager{nullptr};
    GPUDetector *m_gpuDetector{nullptr};
    std::uint64_t m_activeConversionId{0};
};

#endif // PRISM_TUI_BACKEND_H
