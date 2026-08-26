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
            {"ext":"mp4", "vcodec":"avc1.640028", "acodec":"none",
             "width":1080, "height":1080, "fps":25, "tbr":1200, "filesize":1500000},
            {"ext":"m4a", "vcodec":"none", "acodec":"mp4a.40.2",
             "abr":128, "filesize":160000}
        ]
    })json";

    const MediaMetadata metadata = MediaMetadataParser::parse(json);
    bool success = true;
    success = check(metadata.error.isEmpty(), "valid metadata has no error")
        && check(metadata.title == QStringLiteral("Teste de metadados"), "title is parsed")
        && check(metadata.uploader == QStringLiteral("Canal Oficial"), "uploader is parsed")
        && check(metadata.thumbnailUrl == QStringLiteral("https://example.com/thumb.jpg"), "thumbnail is parsed")
        && check(metadata.durationText == QStringLiteral("00:00:10"), "duration is formatted")
        && check(metadata.options.size() == 2, "dynamic format options are produced")
        && check(metadata.options.at(0).actualHeight == 1080, "best video height is real")
        && check(metadata.options.at(0).qualityLabel == QStringLiteral("1080p / Full HD"), "quality label matches height")
        && check(metadata.options.at(1).isAudio, "audio option is detected")
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
