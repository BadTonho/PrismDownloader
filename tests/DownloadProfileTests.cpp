#include "DownloadProfile.h"
#include "MediaItem.h"

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
    if (!check(DownloadProfile::formatSelectorForQuality("4K / Melhor Disponivel") == "bv*[height<=2160]+ba/b[height<=2160]", "4K selector")
        || !check(DownloadProfile::formatSelectorForQuality("1080p Full HD") == "bv*[height<=1080]+ba/b[height<=1080]", "1080p selector")
        || !check(DownloadProfile::formatSelectorForQuality("720p HD") == "bv*[height<=720]+ba/b[height<=720]", "720p selector")
        || !check(DownloadProfile::formatSelectorForQuality("Perfil desconhecido") == "bv*+ba/b", "fallback selector")) {
        return 1;
    }

    MediaItem audioItem;
    audioItem.quality = "Audio MP3";
    if (!check(audioItem.isAudioOnly(), "audio profile detection")) return 1;

    MediaItem videoItem;
    videoItem.quality = "1080p";
    if (!check(!videoItem.isAudioOnly(), "video profile detection")) return 1;
    return 0;
}
