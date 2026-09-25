#include "FormatSelectionDialog.h"

#include <QApplication>
#include <QComboBox>
#include <QTableWidget>

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

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);

    MediaMetadata metadata;
    metadata.title = QStringLiteral("Faixas múltiplas");
    metadata.audioTracks = {
        {QStringLiteral("audio-en"), QStringLiteral("en-US"), QStringLiteral("English"),
         QStringLiteral("m4a"), QStringLiteral("aac"), 128.0, 10, 0, 160000, true},
        {QStringLiteral("audio-pt"), QStringLiteral("pt-BR"), QStringLiteral("Portuguese (Brazil)"),
         QStringLiteral("webm"), QStringLiteral("opus"), 160.0, 5, 0, 200000, true}
    };
    metadata.preferredAudioTrackIndex = 0;

    MediaFormatOption videoOption;
    videoOption.qualityLabel = QStringLiteral("1080p / Full HD");
    videoOption.formatSelector = QStringLiteral("video-only+audio-en");
    videoOption.videoFormatId = QStringLiteral("video-only");
    videoOption.formatCodec = QStringLiteral("MP4/H.264 + M4A/AAC");
    videoOption.resolutionMode = QStringLiteral("1920x1080 • 30 fps");
    videoOption.canSelectAudio = true;

    MediaFormatOption audioOption;
    audioOption.qualityLabel = QStringLiteral("Áudio MP3 (320 kbps)");
    audioOption.formatSelector = QStringLiteral("audio-en");
    audioOption.formatCodec = QStringLiteral("MP3 • origem M4A/AAC");
    audioOption.resolutionMode = QStringLiteral("Áudio • 128 kbps");
    audioOption.isAudio = true;

    MediaFormatOption muxedOption;
    muxedOption.qualityLabel = QStringLiteral("720p / HD");
    muxedOption.videoFormatId = QStringLiteral("muxed720");
    muxedOption.formatSelector = QStringLiteral("muxed720");
    muxedOption.formatCodec = QStringLiteral("MP4/H.264 + AAC");
    muxedOption.resolutionMode = QStringLiteral("1280x720 • 30 fps");
    muxedOption.canSelectAudio = false;

    metadata.options = {videoOption, audioOption, muxedOption};
    FormatSelectionDialog dialog(metadata, 1, 0, {}, {}, false, {}, {}, nullptr, nullptr);

    auto *audioCombo = dialog.findChild<QComboBox *>(QStringLiteral("audioTrackCombo"));
    auto *formatTable = dialog.findChild<QTableWidget *>(QStringLiteral("libraryTable"));
    if (!check(audioCombo != nullptr, "audio track selector is present")
        || !check(formatTable != nullptr, "format table is present")
        || !check(audioCombo->count() == 2, "all audio tracks are shown")) {
        return 1;
    }

    audioCombo->setCurrentIndex(1);
    formatTable->selectRow(0);
    FormatSelectionResult selection = dialog.result();
    if (!check(selection.audioLanguage == QStringLiteral("pt-BR"),
               "selected language is returned")
        || !check(selection.formatSelector == QStringLiteral("video-only+audio-pt"),
                  "video download combines the selected audio format")) {
        return 1;
    }

    formatTable->selectRow(1);
    selection = dialog.result();
    if (!check(selection.formatSelector == QStringLiteral("audio-pt"),
               "audio-only download uses the selected audio format")) {
        return 1;
    }

    formatTable->selectRow(2);
    selection = dialog.result();
    if (!check(!audioCombo->isEnabled(), "muxed format disables audio replacement")
        || !check(selection.formatSelector == QStringLiteral("muxed720"),
                  "muxed format keeps its embedded audio")) {
        return 1;
    }
    return 0;
}
