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

inline std::string audioLanguagePrefix(const std::string &language)
{
    std::string prefix;
    for (unsigned char character : language) {
        if (character == '-' || character == '_') {
            break;
        }
        if (!std::isalnum(character)) {
            return {};
        }
        prefix.push_back(static_cast<char>(std::tolower(character)));
    }
    return prefix;
}

inline std::string audioSelectorForLanguage(const std::string &language)
{
    const std::string prefix = audioLanguagePrefix(language);
    return prefix.empty() ? "ba" : "ba[language^=" + prefix + "]";
}

inline std::string audioOnlySelectorForLanguage(const std::string &language)
{
    const std::string preferred = audioSelectorForLanguage(language);
    return preferred == "ba" ? preferred : preferred + "/ba";
}

inline std::string formatSelectorForQualityAndLanguage(const std::string &quality,
                                                       const std::string &language)
{
    const std::string baseSelector = formatSelectorForQuality(quality);
    const std::string prefix = audioLanguagePrefix(language);
    if (prefix.empty()) {
        return baseSelector;
    }

    const std::string audioSelector = "+ba";
    const std::size_t audioPosition = baseSelector.find(audioSelector);
    if (audioPosition == std::string::npos) {
        return baseSelector;
    }

    std::string preferredSelector = baseSelector;
    const std::size_t fallbackPosition = preferredSelector.find('/');
    if (fallbackPosition != std::string::npos) {
        preferredSelector.resize(fallbackPosition);
    }
    const std::size_t preferredAudioPosition = preferredSelector.find(audioSelector);
    if (preferredAudioPosition == std::string::npos) {
        return baseSelector;
    }
    preferredSelector.insert(preferredAudioPosition + audioSelector.size(),
                             "[language^=" + prefix + "]");
    return preferredSelector + "/" + baseSelector;
}

}

#endif // DOWNLOADPROFILE_H
