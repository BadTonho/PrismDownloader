#ifndef DOWNLOADPROFILE_H
#define DOWNLOADPROFILE_H

#include <algorithm>
#include <cctype>
#include <string>

namespace DownloadProfile {

inline std::string formatSelectorForQuality(const std::string &quality)
{
    std::string normalized = quality;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });

    if (normalized.find("1080P") != std::string::npos) {
        return "bv*[height<=1080]+ba/b[height<=1080]";
    }
    if (normalized.find("720P") != std::string::npos) {
        return "bv*[height<=720]+ba/b[height<=720]";
    }
    if (normalized.find("4K") != std::string::npos) {
        return "bv*[height<=2160]+ba/b[height<=2160]";
    }
    return "bv*+ba/b";
}

}

#endif // DOWNLOADPROFILE_H
