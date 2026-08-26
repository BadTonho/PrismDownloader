#include "MediaMetadata.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>

namespace {

struct ParsedMediaFormat {
    QString ext;
    QString videoCodec;
    QString audioCodec;
    int width{0};
    int height{0};
    double fps{0.0};
    double tbr{0.0};
    double vbr{0.0};
    double abr{0.0};
    qint64 size{0};
};

bool hasVideo(const ParsedMediaFormat &format)
{
    return !format.videoCodec.isEmpty() && format.videoCodec != QStringLiteral("none")
        && format.height > 0;
}

bool hasAudio(const ParsedMediaFormat &format)
{
    return !format.audioCodec.isEmpty() && format.audioCodec != QStringLiteral("none");
}

QString codecLabel(const QString &codec, bool audio)
{
    const QString normalized = codec.toLower();
    if (normalized.startsWith(QStringLiteral("avc1")) || normalized.contains(QStringLiteral("h264"))) {
        return QStringLiteral("H.264");
    }
    if (normalized.startsWith(QStringLiteral("hev1")) || normalized.startsWith(QStringLiteral("hvc1"))
        || normalized.contains(QStringLiteral("hevc"))) {
        return QStringLiteral("HEVC");
    }
    if (normalized.startsWith(QStringLiteral("av01")) || normalized.contains(QStringLiteral("av1"))) {
        return QStringLiteral("AV1");
    }
    if (normalized.startsWith(QStringLiteral("vp09")) || normalized.startsWith(QStringLiteral("vp9"))) {
        return QStringLiteral("VP9");
    }
    if (normalized.startsWith(QStringLiteral("vp08")) || normalized.startsWith(QStringLiteral("vp8"))) {
        return QStringLiteral("VP8");
    }
    if (audio && normalized.startsWith(QStringLiteral("mp4a"))) {
        return QStringLiteral("AAC");
    }
    if (audio && normalized.contains(QStringLiteral("opus"))) {
        return QStringLiteral("Opus");
    }
    if (audio && normalized.contains(QStringLiteral("vorbis"))) {
        return QStringLiteral("Vorbis");
    }
    const int separator = codec.indexOf(QLatin1Char('.'));
    return separator > 0 ? codec.left(separator).toUpper() : codec.toUpper();
}

qint64 formatSizeFromJson(const QJsonObject &object)
{
    const qint64 exact = static_cast<qint64>(object.value(QStringLiteral("filesize")).toDouble(-1.0));
    if (exact > 0) {
        return exact;
    }
    return static_cast<qint64>(object.value(QStringLiteral("filesize_approx")).toDouble(-1.0));
}

double formatBitrateKbps(const ParsedMediaFormat &format)
{
    if (format.tbr > 0.0) {
        return format.tbr;
    }
    return format.vbr + format.abr;
}

double formatBytesPerSecond(const ParsedMediaFormat &format, double durationSeconds)
{
    if (format.size > 0 && durationSeconds > 0.0) {
        return static_cast<double>(format.size) / durationSeconds;
    }
    const double bitrateKbps = formatBitrateKbps(format);
    return bitrateKbps > 0.0 ? bitrateKbps * 1000.0 / 8.0 : 0.0;
}

int bestVideoFormatForHeight(const QList<ParsedMediaFormat> &formats, int targetHeight)
{
    int best = -1;
    for (int index = 0; index < formats.size(); ++index) {
        const ParsedMediaFormat &candidate = formats.at(index);
        if (!hasVideo(candidate) || candidate.height != targetHeight) {
            continue;
        }
        if (best < 0) {
            best = index;
            continue;
        }
        const ParsedMediaFormat &current = formats.at(best);
        if (candidate.fps > current.fps
            || (candidate.fps == current.fps && formatBitrateKbps(candidate) > formatBitrateKbps(current))) {
            best = index;
        }
    }
    return best;
}

int bestAudioFormat(const QList<ParsedMediaFormat> &formats)
{
    int best = -1;
    for (int index = 0; index < formats.size(); ++index) {
        const ParsedMediaFormat &candidate = formats.at(index);
        if (!hasAudio(candidate)) {
            continue;
        }
        if (best < 0) {
            best = index;
            continue;
        }
        const ParsedMediaFormat &current = formats.at(best);
        const bool candidateAudioOnly = !hasVideo(candidate);
        const bool currentAudioOnly = !hasVideo(current);
        if ((candidateAudioOnly && !currentAudioOnly)
            || (candidateAudioOnly == currentAudioOnly
                && formatBitrateKbps(candidate) > formatBitrateKbps(current))) {
            best = index;
        }
    }
    return best;
}

MediaFormatOption makeVideoFormatOptionForHeight(const QList<ParsedMediaFormat> &formats,
                                                int height, double durationSeconds)
{
    MediaFormatOption option;
    const int videoIndex = bestVideoFormatForHeight(formats, height);
    if (videoIndex < 0) {
        option.available = false;
        return option;
    }

    const ParsedMediaFormat &video = formats.at(videoIndex);
    const int audioIndex = hasAudio(video) ? videoIndex : bestAudioFormat(formats);
    const ParsedMediaFormat *audio = audioIndex >= 0 ? &formats.at(audioIndex) : nullptr;
    option.available = true;
    option.isAudio = false;
    option.actualHeight = video.height;
    option.fps = video.fps;
    option.qualityLabel = MediaMetadataParser::actualQualityLabel(video.height);
    option.formatCodec = QStringLiteral("%1/%2")
        .arg(video.ext.toUpper(), codecLabel(video.videoCodec, false));
    double bytesPerSecond = formatBytesPerSecond(video, durationSeconds);
    if (audio && audioIndex != videoIndex) {
        option.formatCodec += QStringLiteral(" + %1/%2")
            .arg(audio->ext.toUpper(), codecLabel(audio->audioCodec, true));
        bytesPerSecond += formatBytesPerSecond(*audio, durationSeconds);
    } else if (audio && hasAudio(video)) {
        option.formatCodec += QStringLiteral(" + %1")
            .arg(codecLabel(video.audioCodec, true));
    }
    option.estimatedBytesPerSecond = bytesPerSecond;
    option.estimatedBytes = durationSeconds > 0.0
        ? qRound64(bytesPerSecond * durationSeconds)
        : video.size + (audio && audioIndex != videoIndex ? audio->size : 0);
    option.resolutionMode = QStringLiteral("%1x%2 • %3 fps")
        .arg(video.width).arg(video.height)
        .arg(video.fps > 0.0 ? QString::number(video.fps, 'f', 0) : QStringLiteral("?"));
    return option;
}

MediaFormatOption makeAudioFormatOption(const QList<ParsedMediaFormat> &formats, double durationSeconds)
{
    MediaFormatOption option;
    const int audioIndex = bestAudioFormat(formats);
    if (audioIndex < 0) {
        option.available = false;
        option.formatCodec = QStringLiteral("Áudio não disponível neste vídeo");
        option.resolutionMode = QStringLiteral("Somente vídeo");
        return option;
    }

    const ParsedMediaFormat &audio = formats.at(audioIndex);
    option.available = true;
    option.isAudio = true;
    option.actualHeight = 0;
    option.qualityLabel = QStringLiteral("Áudio MP3 (320 kbps)");
    option.formatCodec = QStringLiteral("MP3 • origem %1/%2")
        .arg(audio.ext.toUpper(), codecLabel(audio.audioCodec, true));
    option.estimatedBytesPerSecond = formatBytesPerSecond(audio, durationSeconds);
    option.estimatedBytes = durationSeconds > 0.0
        ? qRound64(option.estimatedBytesPerSecond * durationSeconds)
        : audio.size;
    const double bitrate = formatBitrateKbps(audio);
    option.resolutionMode = bitrate > 0.0
        ? QStringLiteral("Áudio • %1 kbps").arg(qRound(bitrate))
        : QStringLiteral("Áudio");
    return option;
}

}

namespace MediaMetadataParser {

QString actualQualityLabel(int height)
{
    if (height >= 4320) {
        return QStringLiteral("4320p / 8K Ultra HD");
    }
    if (height >= 2160) {
        return QStringLiteral("2160p / 4K Ultra HD");
    }
    if (height >= 1440) {
        return QStringLiteral("1440p / 2K QHD");
    }
    if (height >= 1080) {
        return QStringLiteral("1080p / Full HD");
    }
    if (height >= 720) {
        return QStringLiteral("720p / HD");
    }
    if (height >= 480) {
        return QStringLiteral("480p / SD");
    }
    if (height >= 360) {
        return QStringLiteral("360p / Baixa Definição");
    }
    if (height >= 240) {
        return QStringLiteral("240p / Econômico");
    }
    if (height >= 144) {
        return QStringLiteral("144p / Mínimo");
    }
    return height > 0 ? QStringLiteral("%1p / Fonte").arg(height)
                      : QStringLiteral("Qualidade não informada");
}

QString readableBytes(qint64 bytes)
{
    if (bytes <= 0) {
        return QStringLiteral("—");
    }
    static const QStringList units{QStringLiteral("B"), QStringLiteral("KB"),
                                   QStringLiteral("MB"), QStringLiteral("GB")};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < units.size() - 1) {
        value /= 1024.0;
        ++unit;
    }
    return QStringLiteral("≈ %1 %2").arg(value, 0, 'f', unit == 0 ? 0 : 1).arg(units.at(unit));
}

double selectedDurationSeconds(const QString &timeRange, double fullDuration)
{
    if (timeRange.isEmpty()) {
        return fullDuration;
    }
    static const QRegularExpression pattern(
        QStringLiteral("^(\\d{1,3}):([0-5]\\d):([0-5]\\d)-(\\d{1,3}):([0-5]\\d):([0-5]\\d)$"));
    const QRegularExpressionMatch match = pattern.match(timeRange);
    if (!match.hasMatch()) {
        return fullDuration;
    }
    const auto toSeconds = [&match](int hourIndex, int minuteIndex, int secondIndex) {
        return match.captured(hourIndex).toLongLong() * 3600
            + match.captured(minuteIndex).toLongLong() * 60
            + match.captured(secondIndex).toLongLong();
    };
    const qint64 start = toSeconds(1, 2, 3);
    const qint64 end = toSeconds(4, 5, 6);
    return end > start ? static_cast<double>(end - start) : fullDuration;
}

MediaMetadata parse(const QByteArray &output)
{
    MediaMetadata metadata;
    QByteArray json = output.trimmed();
    const qsizetype objectStart = json.indexOf('{');
    const qsizetype objectEnd = json.lastIndexOf('}');
    if (objectStart >= 0 && objectEnd > objectStart) {
        json = json.mid(objectStart, objectEnd - objectStart + 1);
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        metadata.error = QStringLiteral("yt-dlp não retornou metadados JSON válidos.");
        return metadata;
    }

    const QJsonObject root = document.object();
    metadata.title = root.value(QStringLiteral("title")).toString();
    metadata.uploader = root.value(QStringLiteral("uploader")).toString();
    if (metadata.uploader.isEmpty()) {
        metadata.uploader = root.value(QStringLiteral("channel")).toString();
    }
    if (metadata.uploader.isEmpty()) {
        metadata.uploader = root.value(QStringLiteral("uploader_id")).toString();
    }
    metadata.thumbnailUrl = root.value(QStringLiteral("thumbnail")).toString().trimmed();
    QSet<QString> candidateSet;
    if (!metadata.thumbnailUrl.isEmpty()) {
        metadata.thumbnailCandidates.append(metadata.thumbnailUrl);
        candidateSet.insert(metadata.thumbnailUrl);
    }

    const QJsonArray thumbs = root.value(QStringLiteral("thumbnails")).toArray();
    for (int i = thumbs.size() - 1; i >= 0; --i) {
        const QJsonObject thumbObj = thumbs.at(i).toObject();
        const QString tUrl = thumbObj.value(QStringLiteral("url")).toString().trimmed();
        if (!tUrl.isEmpty() && !candidateSet.contains(tUrl)) {
            metadata.thumbnailCandidates.append(tUrl);
            candidateSet.insert(tUrl);
        }
    }

    static const QRegularExpression ytPattern(QStringLiteral(R"(i\.ytimg\.com/vi(?:_webp)?/([^/?#]+))"));
    for (const QString &url : metadata.thumbnailCandidates) {
        const QRegularExpressionMatch m = ytPattern.match(url);
        if (m.hasMatch()) {
            const QString videoId = m.captured(1);
            const QStringList ytDefaults = {
                QStringLiteral("https://i.ytimg.com/vi/%1/hqdefault.jpg").arg(videoId),
                QStringLiteral("https://i.ytimg.com/vi/%1/mqdefault.jpg").arg(videoId),
                QStringLiteral("https://i.ytimg.com/vi/%1/sddefault.jpg").arg(videoId),
                QStringLiteral("https://i.ytimg.com/vi/%1/default.jpg").arg(videoId)
            };
            for (const QString &defUrl : ytDefaults) {
                if (!candidateSet.contains(defUrl)) {
                    metadata.thumbnailCandidates.append(defUrl);
                    candidateSet.insert(defUrl);
                }
            }
            break;
        }
    }

    metadata.durationSeconds = root.value(QStringLiteral("duration")).toDouble(0.0);
    metadata.durationText = root.value(QStringLiteral("duration_string")).toString();
    if (metadata.durationText.isEmpty() && metadata.durationSeconds > 0.0) {
        const qint64 totalSeconds = qRound64(metadata.durationSeconds);
        metadata.durationText = QStringLiteral("%1:%2:%3")
            .arg(totalSeconds / 3600, 2, 10, QLatin1Char('0'))
            .arg((totalSeconds / 60) % 60, 2, 10, QLatin1Char('0'))
            .arg(totalSeconds % 60, 2, 10, QLatin1Char('0'));
    }

    QList<ParsedMediaFormat> formats;
    const QJsonArray formatArray = root.value(QStringLiteral("formats")).toArray();
    for (const QJsonValue &value : formatArray) {
        const QJsonObject object = value.toObject();
        ParsedMediaFormat format;
        format.ext = object.value(QStringLiteral("ext")).toString();
        format.videoCodec = object.value(QStringLiteral("vcodec")).toString();
        format.audioCodec = object.value(QStringLiteral("acodec")).toString();
        format.width = object.value(QStringLiteral("width")).toInt();
        format.height = object.value(QStringLiteral("height")).toInt();
        format.fps = object.value(QStringLiteral("fps")).toDouble();
        format.tbr = object.value(QStringLiteral("tbr")).toDouble();
        format.vbr = object.value(QStringLiteral("vbr")).toDouble();
        format.abr = object.value(QStringLiteral("abr")).toDouble();
        format.size = formatSizeFromJson(object);
        if (hasVideo(format) || hasAudio(format)) {
            formats.append(format);
        }
    }

    if (formats.isEmpty()) {
        metadata.error = QStringLiteral("A mídia não possui formatos compatíveis para análise.");
        return metadata;
    }

    QList<int> distinctHeights;
    for (const ParsedMediaFormat &fmt : formats) {
        if (hasVideo(fmt) && fmt.height > 0 && !distinctHeights.contains(fmt.height)) {
            distinctHeights.append(fmt.height);
        }
    }
    std::sort(distinctHeights.begin(), distinctHeights.end(), std::greater<int>());

    for (int height : distinctHeights) {
        MediaFormatOption opt = makeVideoFormatOptionForHeight(formats, height, metadata.durationSeconds);
        if (opt.available) {
            metadata.options.append(opt);
        }
    }

    MediaFormatOption audioOpt = makeAudioFormatOption(formats, metadata.durationSeconds);
    if (audioOpt.available) {
        metadata.options.append(audioOpt);
    }

    return metadata;
}

}
