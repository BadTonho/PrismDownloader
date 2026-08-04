#include "DownloadProfile.h"
#include "MediaItem.h"

#include <cassert>

int main()
{
    assert(DownloadProfile::formatSelectorForQuality("4K / Melhor Disponivel") == "bv*[height<=2160]+ba/b[height<=2160]");
    assert(DownloadProfile::formatSelectorForQuality("1080p Full HD") == "bv*[height<=1080]+ba/b[height<=1080]");
    assert(DownloadProfile::formatSelectorForQuality("720p HD") == "bv*[height<=720]+ba/b[height<=720]");
    assert(DownloadProfile::formatSelectorForQuality("Perfil desconhecido") == "bv*+ba/b");

    MediaItem audioItem;
    audioItem.quality = "Audio MP3";
    assert(audioItem.isAudioOnly());

    MediaItem videoItem;
    videoItem.quality = "1080p";
    assert(!videoItem.isAudioOnly());
    return 0;
}
