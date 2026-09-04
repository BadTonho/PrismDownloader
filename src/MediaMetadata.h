#ifndef MEDIA_METADATA_H
#define MEDIA_METADATA_H

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

struct MediaFormatOption {
    QString qualityLabel;
    QString formatCodec;
    // Exact yt-dlp format selector represented by this option. Keeping the
    // selected stream IDs prevents the download from choosing a different
    // stream than the one used for the estimate shown in the dialog.
    QString formatSelector;
    QString resolutionMode;
    int actualHeight{0};
    double fps{0.0};
    qint64 estimatedBytes{0};
    double estimatedBytesPerSecond{0.0};
    bool isAudio{false};
    bool available{true};
};

struct MediaMetadata {
    QString title;
    QString uploader;
    QString durationText;
    QString thumbnailUrl;
    QStringList thumbnailCandidates;
    double durationSeconds{0.0};
    QList<MediaFormatOption> options;
    QString error;
};

namespace MediaMetadataParser {

MediaMetadata parse(const QByteArray &output);
QString readableBytes(qint64 bytes);
QString actualQualityLabel(int height);
double selectedDurationSeconds(const QString &timeRange, double fullDuration);

}

#endif // MEDIA_METADATA_H
