#ifndef MEDIAITEM_H
#define MEDIAITEM_H

#include <string>
#include <algorithm>

enum class DownloadStatus {
    Queued,
    Downloading,
    Muxing,
    ConvertingGPU,
    Completed,
    Error,
    Cancelled
};

struct MediaItem {
    std::string url;
    std::string title;
    std::string quality;        // ex: "1080p", "4K", "MP3"
    std::string speed;          // ex: "12.5 MB/s"
    std::string eta;            // ex: "01:30"
    double progress = 0.0;      // 0.0 a 100.0
    DownloadStatus status = DownloadStatus::Queued;

    bool isAudioOnly() const {
        std::string upper = quality;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        return upper.find("MP3") != std::string::npos || 
               upper.find("FLAC") != std::string::npos || 
               upper.find("AUDIO") != std::string::npos;
    }
};

#endif // MEDIAITEM_H
