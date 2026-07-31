#ifndef DOWNLOADENGINE_H
#define DOWNLOADENGINE_H

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include "MediaItem.h"
#include "GPUDetector.h"

class DownloadEngine {
public:
    DownloadEngine();
    ~DownloadEngine();

    void initialize();
    void startDownload(const std::string &url, const std::string &quality = "1080p", const std::string &timeRange = "");
    void cancelCurrent();

    bool isDownloading() const;
    GPUDetector* gpuDetector();

    // Callbacks C++ nativos para eventos em tempo real
    void setProgressCallback(std::function<void(double, const std::string&, const std::string&)> cb);
    void setStatusCallback(std::function<void(DownloadStatus, const std::string&)> cb);

private:
    GPUDetector m_gpuDetector;
    MediaItem m_currentItem;
    std::atomic<bool> m_isRunning{false};
    std::thread m_workerThread;

    std::function<void(double, const std::string&, const std::string&)> m_onProgress;
    std::function<void(DownloadStatus, const std::string&)> m_onStatus;

    void parseYtDlpOutput(const std::string &line);
    void workerLoop(const std::string &command);
};

#endif // DOWNLOADENGINE_H
