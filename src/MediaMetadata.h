#ifndef MEDIA_METADATA_H
#define MEDIA_METADATA_H

#include <QByteArray>
#include <QList>
#include <QString>

struct MediaFormatOption {
    QString formatCodec;
    QString resolutionMode;
    int actualHeight{0};
    qint64 estimatedBytes{0};
    double estimatedBytesPerSecond{0.0};
    bool available{false};
};

struct MediaMetadata {
    QString title;
    QString durationText;
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
