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

    if (normalized.find("4K") != std::string::npos || normalized.find("2160P") != std::string::npos) {
        return "bv*[height<=2160]+ba/b[height<=2160]";
    }
    if (normalized.find("8K") != std::string::npos || normalized.find("4320P") != std::string::npos) {
        return "bv*[height<=4320]+ba/b[height<=4320]";
    }
    if (normalized.find("1440P") != std::string::npos || normalized.find("2K") != std::string::npos || normalized.find("QHD") != std::string::npos) {
        return "bv*[height<=1440]+ba/b[height<=1440]";
    }
    if (normalized.find("1080P") != std::string::npos) {
        return "bv*[height<=1080]+ba/b[height<=1080]";
    }
    if (normalized.find("720P") != std::string::npos) {
        return "bv*[height<=720]+ba/b[height<=720]";
    }
    if (normalized.find("480P") != std::string::npos) {
        return "bv*[height<=480]+ba/b[height<=480]";
    }
    if (normalized.find("360P") != std::string::npos) {
        return "bv*[height<=360]+ba/b[height<=360]";
    }
    if (normalized.find("240P") != std::string::npos) {
        return "bv*[height<=240]+ba/b[height<=240]";
    }
    if (normalized.find("144P") != std::string::npos) {
        return "bv*[height<=144]+ba/b[height<=144]";
    }
    return "bv*+ba/b";
}

}

#endif // DOWNLOADPROFILE_H
