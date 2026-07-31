#ifndef MEDIAITEM_H
#define MEDIAITEM_H

#include <QString>
#include <QMetaType>

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
    QString url;
    QString title;
    QString quality;        // ex: "1080p", "4K", "MP3"
    QString speed;          // ex: "12.5 MB/s"
    QString eta;            // ex: "01:30"
    double progress = 0.0;  // 0.0 a 100.0
    DownloadStatus status = DownloadStatus::Queued;

    bool isAudioOnly() const {
        return quality.contains("MP3", Qt::CaseInsensitive) || 
               quality.contains("FLAC", Qt::CaseInsensitive) || 
               quality.contains("Audio", Qt::CaseInsensitive);
    }
};

Q_DECLARE_METATYPE(MediaItem)

#endif // MEDIAITEM_H
