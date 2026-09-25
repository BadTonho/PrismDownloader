#include "MediaMetadata.h"

#include <iostream>

namespace {

bool check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

}

int main()
{
    const QByteArray json = R"json({
        "title": "Teste de metadados",
        "uploader": "Canal Oficial",
        "thumbnail": "https://example.com/thumb.jpg",
        "duration": 10,
        "formats": [
            {"format_id":"video1080", "ext":"mp4", "vcodec":"avc1.640028", "acodec":"none",
             "width":1080, "height":1080, "fps":25, "tbr":1200, "filesize":1500000},
            {"format_id":"audio128", "ext":"m4a", "vcodec":"none", "acodec":"mp4a.40.2",
             "abr":128, "filesize":160000, "language":"pt-BR", "format_note":"Portuguese (Brazil)",
             "language_preference":5, "source_preference":0},
            {"format_id":"audio160", "ext":"m4a", "vcodec":"none", "acodec":"mp4a.40.2",
             "abr":160, "filesize":200000, "language":"en-US", "format_note":"English (US)",
             "language_preference":10, "source_preference":0},
            {"format_id":"audio256", "ext":"webm", "vcodec":"none", "acodec":"opus",
             "abr":256, "filesize":320000, "language":"pt-BR", "format_note":"Portuguese (Brazil)",
             "language_preference":5, "source_preference":1},
            {"format_id":"audio-commentary", "ext":"m4a", "vcodec":"none", "acodec":"mp4a.40.2",
             "abr":320, "filesize":400000, "format_note":"Commentary"}
        ]
    })json";

    const MediaMetadata metadata = MediaMetadataParser::parse(json);
    bool success = true;
    success = check(metadata.error.isEmpty(), "valid metadata has no error")
        && check(metadata.title == QStringLiteral("Teste de metadados"), "title is parsed")
        && check(metadata.uploader == QStringLiteral("Canal Oficial"), "uploader is parsed")
        && check(metadata.thumbnailUrl == QStringLiteral("https://example.com/thumb.jpg"), "thumbnail is parsed")
        && check(metadata.thumbnailCandidates.contains(QStringLiteral("https://example.com/thumb.jpg")), "thumbnail candidates contains url")
        && check(metadata.durationText == QStringLiteral("00:00:10"), "duration is formatted")
        && check(metadata.options.size() == 2, "dynamic format options are produced")
        && check(metadata.audioTracks.size() == 4, "all distinct audio formats are preserved")
        && check(metadata.preferredAudioTrackIndex == 1,
                 "language preference wins over a higher bitrate")
        && check(metadata.options.at(0).actualHeight == 1080, "best video height is real")
        && check(metadata.options.at(0).qualityLabel == QStringLiteral("1080p / Full HD"), "quality label matches height")
        && check(metadata.options.at(0).videoFormatId == QStringLiteral("video1080"),
                 "video option preserves its video-only format ID")
        && check(metadata.options.at(0).formatSelector == QStringLiteral("video1080+audio160"),
                 "video option combines the source-preferred audio format")
        && check(metadata.options.at(1).isAudio, "audio option is detected")
        && check(metadata.options.at(1).formatSelector == QStringLiteral("audio160"),
                 "audio option preserves the source-preferred format ID")
        && check(metadata.audioTracks.at(2).language == QStringLiteral("pt-BR")
                 && metadata.audioTracks.at(2).formatId == QStringLiteral("audio256"),
                 "same-language streams with distinct format IDs remain selectable")
        && check(metadata.audioTracks.at(3).language.isEmpty()
                 && metadata.audioTracks.at(3).formatNote == QStringLiteral("Commentary"),
                 "audio tracks remain available when language metadata is missing")
        && check(MediaMetadataParser::actualQualityLabel(1080) == QStringLiteral("1080p / Full HD"),
                 "quality label reflects real height")
        && check(MediaMetadataParser::selectedDurationSeconds(
                      QStringLiteral("00:00:02-00:00:07"), 10.0) == 5.0,
                  "time range duration is calculated")
        && success;

    const MediaMetadata invalid = MediaMetadataParser::parse(QByteArray("not-json"));
    success = check(!invalid.error.isEmpty(), "invalid metadata reports an error") && success;
    return success ? 0 : 1;
}
