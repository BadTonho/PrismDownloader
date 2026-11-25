#include "tui/TuiState.h"

#include <QFile>
#include <QTemporaryDir>

#include <iostream>
#include <string>

namespace {

int g_failures = 0;

void expect(bool condition, const std::string &message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++g_failures;
    }
}

void testQueueLifecycle()
{
    TuiState state;
    state.setConcurrencyLimit(99);
    expect(state.snapshot().concurrencyLimit == 5, "concurrency should clamp to 5");
    state.setConcurrencyLimit(0);
    expect(state.snapshot().concurrencyLimit == 1, "concurrency should clamp to 1");

    TuiQueueItem item;
    item.id = 42;
    item.url = "https://example.com/video";
    item.title = item.url;
    item.quality = "1080p Full HD";
    state.addQueueItem(item);
    state.setQueueCounts(1, 2);

    state.updateQueueProgress(42, 65.5, "12.4 MB/s", "00:15");
    state.updateQueueStatus(42, DownloadStatus::Downloading, "Baixando");

    TuiSnapshot snap = state.snapshot();
    expect(snap.queue.size() == 1, "queue should have 1 item");
    expect(snap.activeDownloads == 1 && snap.pendingDownloads == 2, "counts should match");
    expect(snap.queue[0].progress > 65.0 && snap.queue[0].speed == "12.4 MB/s", "progress/speed updated");

    state.markQueueCompleted(42, "/tmp/demo_video.mp4");
    snap = state.snapshot();
    expect(snap.queue[0].status == DownloadStatus::Completed, "status should be Completed");
    expect(snap.queue[0].progress == 100.0, "completed progress should be 100");
    expect(snap.queue[0].title == "demo_video.mp4", "completed title should use file basename");
}

void testLogsAndFiltering()
{
    TuiState state;
    state.appendLog("[10:00:00] [Sistema] Iniciado");
    state.appendLog("[10:00:01] [Download #1] Baixando 50%");
    state.appendLog("[10:00:02] [Conversor] Erro: codec indisponivel");

    const auto snap = state.snapshot();
    expect(TuiState::filterLogs(snap.logs, 0).size() == 3, "filter 0 returns all logs");
    expect(TuiState::filterLogs(snap.logs, 1).size() == 2, "filter 1 returns process logs");
    expect(TuiState::filterLogs(snap.logs, 2).size() == 1, "filter 2 returns error logs");

    state.clearLogs();
    expect(state.snapshot().logs.empty(), "clearLogs empties log buffer");
}

void testLibraryScanningAndFormatting()
{
    QTemporaryDir tempDir;
    expect(tempDir.isValid(), "temp dir should be valid");

    {
        QFile videoFile(tempDir.filePath(QStringLiteral("clip.mp4")));
        expect(videoFile.open(QIODevice::WriteOnly), "create clip.mp4");
        videoFile.write(QByteArray(2048, 'a'));
    }
    {
        QFile ignoredFile(tempDir.filePath(QStringLiteral("notes.txt")));
        expect(ignoredFile.open(QIODevice::WriteOnly), "create notes.txt");
        ignoredFile.write("ignore me");
    }

    const auto items = TuiState::scanLibraryDirectory(tempDir.path().toUtf8().toStdString());
    expect(items.size() == 1, "library scan should only include media files");
    if (!items.empty()) {
        expect(items[0].name == "clip.mp4", "scanned file name should match");
        expect(items[0].extension == "MP4", "extension should be uppercase MP4");
        expect(items[0].sizeBytes == 2048, "file size should be 2048 bytes");
    }

    expect(TuiState::formatFileSize(512) == "512 B", "bytes formatting");
    expect(TuiState::formatFileSize(2048) == "2.0 KB", "KB formatting");
    expect(TuiState::formatFileSize(5 * 1024 * 1024) == "5.0 MB", "MB formatting");
    expect(!TuiState::qualityPresets().empty(), "quality presets non-empty");
    expect(!TuiState::converterFormats().empty(), "converter formats non-empty");
}

} // namespace

int main()
{
    testQueueLifecycle();
    testLogsAndFiltering();
    testLibraryScanningAndFormatting();

    if (g_failures > 0) {
        std::cerr << g_failures << " test(s) failed.\n";
        return 1;
    }
    std::cout << "All PrismTuiStateTests passed.\n";
    return 0;
}
