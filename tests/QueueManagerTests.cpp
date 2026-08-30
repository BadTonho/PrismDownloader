#include "ConversionManager.h"
#include "DownloadManager.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QSet>
#include <QTemporaryDir>
#include <QThread>

#include <functional>
#include <iostream>

namespace {

bool waitUntil(const std::function<bool()> &condition, int timeoutMs = 6000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(5);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return condition();
}

bool check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

DownloadRequest requestFor(const QString &outputDirectory, int number)
{
    DownloadRequest request;
    request.url = QUrl(QString("https://example.test/video/%1").arg(number));
    request.quality = "1080p Full HD";
    request.outputDirectory = outputDirectory;
    return request;
}

bool testDownloadScheduling(const QString &toolPath)
{
    QTemporaryDir output;
    if (!check(output.isValid(), "temporary download directory")) return false;

    DownloadManager manager(nullptr, toolPath);
    manager.setConcurrencyLimit(2);
    QSet<DownloadId> active;
    QHash<DownloadId, int> progressEvents;
    int maximumActive = 0;
    int completions = 0;
    QObject::connect(&manager, &DownloadManager::jobStatus,
                     [&active, &maximumActive](DownloadId id, DownloadStatus status, const QString &) {
        if (status == DownloadStatus::Downloading) active.insert(id);
        if (status == DownloadStatus::Completed || status == DownloadStatus::Error
            || status == DownloadStatus::Cancelled) active.remove(id);
        maximumActive = qMax(maximumActive, active.size());
    });
    QObject::connect(&manager, &DownloadManager::jobCompleted,
                     [&completions](DownloadId, const QString &) { ++completions; });
    QObject::connect(&manager, &DownloadManager::jobProgress,
                     [&progressEvents](DownloadId id, double, const QString &, const QString &) {
        ++progressEvents[id];
    });

    for (int number = 1; number <= 5; ++number) {
        if (!check(manager.enqueueDownload(requestFor(output.path(), number)).accepted,
                   "enqueue five downloads")) return false;
    }
    if (!check(waitUntil([&manager]() { return manager.activeCount() == 2 && manager.pendingCount() == 3; }),
               "limit 2 starts exactly two and queues three")) return false;
    if (!check(waitUntil([&manager]() { return !manager.hasWork(); }), "five downloads complete")) return false;
    if (!check(completions == 5, "all downloads emitted completion")
        || !check(maximumActive == 2, "download concurrency never exceeded two")
        || !check(progressEvents.size() == 5, "progress remained identified for every download")) {
        return false;
    }
    for (auto iterator = progressEvents.cbegin(); iterator != progressEvents.cend(); ++iterator) {
        if (!check(iterator.value() >= 2, "each download received only its identified progress events")) return false;
    }
    return true;
}

bool testReportedOutputPathVariants(const QString &toolPath)
{
    const QStringList variants{"relative", "json", "stale"};
    for (const QString &variant : variants) {
        QTemporaryDir output;
        if (!check(output.isValid(), "temporary output-identity directory")) return false;

        DownloadManager manager(nullptr, toolPath);
        QString completedPath;
        int errors = 0;
        QObject::connect(&manager, &DownloadManager::jobCompleted,
                         [&completedPath](DownloadId, const QString &path) {
            completedPath = path;
        });
        QObject::connect(&manager, &DownloadManager::jobStatus,
                         [&errors](DownloadId, DownloadStatus status, const QString &) {
            if (status == DownloadStatus::Error) ++errors;
        });

        DownloadRequest request;
        request.url = QUrl(QString("https://example.test/video/%1").arg(variant));
        request.quality = "1080p Full HD";
        request.outputDirectory = output.path();
        if (!check(manager.enqueueDownload(request).accepted,
                   "enqueue output-identity download")) return false;
        if (!check(waitUntil([&manager]() { return !manager.hasWork(); }),
                   "output-identity download completes")) return false;

        const QString expectedPath = output.filePath("Fake [" + variant + "].mp4");
        if (!check(errors == 0, "output-identity path is not reported as an error")
            || !check(completedPath == QFileInfo(expectedPath).absoluteFilePath(),
                      "reported output path resolves to the file on disk")
            || !check(QFile::exists(completedPath), "resolved output file exists")) {
            return false;
        }
    }
    return true;
}

bool testDuplicateCancellationAndLimit(const QString &toolPath)
{
    QTemporaryDir output;
    if (!check(output.isValid(), "temporary cancellation directory")) return false;

    DownloadManager manager(nullptr, toolPath);
    manager.setConcurrencyLimit(1);
    QStringList starts;
    QObject::connect(&manager, &DownloadManager::jobStatus,
                     [&starts](DownloadId id, DownloadStatus status, const QString &) {
        if (status == DownloadStatus::Downloading) starts.append(QString::number(id));
    });

    const EnqueueResult first = manager.enqueueDownload(requestFor(output.path(), 10));
    const EnqueueResult duplicate = manager.enqueueDownload(requestFor(output.path(), 10));
    const EnqueueResult second = manager.enqueueDownload(requestFor(output.path(), 11));
    const EnqueueResult third = manager.enqueueDownload(requestFor(output.path(), 12));
    if (!check(first.accepted && second.accepted && third.accepted, "unique downloads accepted")) return false;
    if (!check(!duplicate.accepted && duplicate.error.contains("já está ativa"), "active duplicate rejected clearly")) return false;
    if (!check(waitUntil([&manager]() { return manager.activeCount() == 1 && manager.pendingCount() == 2; }),
               "single concurrency established")) return false;
    if (!check(manager.cancelDownload(first.id), "selected active download cancelled")) return false;
    if (!check(waitUntil([&starts]() { return starts.size() >= 2; }), "next download starts after cancellation")) return false;

    manager.setConcurrencyLimit(2);
    if (!check(waitUntil([&manager]() { return manager.activeCount() == 2; }), "raising limit starts another task")) return false;
    manager.setConcurrencyLimit(1);
    if (!check(manager.activeCount() == 2, "lowering limit preserves active tasks")) return false;
    manager.cancelAll();
    if (!check(waitUntil([&manager]() { return !manager.hasWork(); }), "cancel all clears active downloads")) return false;

    manager.setConcurrencyLimit(1);
    const int startsBeforePendingCancellation = starts.size();
    for (int number = 20; number <= 22; ++number) {
        if (!check(manager.enqueueDownload(requestFor(output.path(), number)).accepted,
                   "enqueue downloads for global cancellation")) return false;
    }
    if (!check(waitUntil([&manager]() { return manager.activeCount() == 1 && manager.pendingCount() == 2; }),
               "global cancellation scenario has active and pending work")) return false;
    manager.cancelAll();
    if (!check(waitUntil([&manager]() { return !manager.hasWork(); }), "cancel all clears pending downloads")) return false;
    return check(starts.size() == startsBeforePendingCancellation + 1,
                 "cancel all never starts a pending download while clearing the queue");
}

ConversionRequest conversionFor(const QString &input, const QString &output, DownloadId owner)
{
    ConversionRequest request;
    request.ownerDownloadId = owner;
    request.inputFile = input;
    request.format = "MP4 (H.264 / Compatibilidade Universal)";
    request.outputDirectory = output;
    request.gpuType = GPUType::CPU_ONLY;
    return request;
}

bool testMissingToolsAreReported()
{
    QTemporaryDir output;
    if (!check(output.isValid(), "temporary missing-tool directory")) return false;

    DownloadManager downloadManager(nullptr, output.filePath("missing-yt-dlp"));
    const EnqueueResult downloadResult = downloadManager.enqueueDownload(requestFor(output.path(), 99));
    if (!check(!downloadResult.accepted && !downloadResult.error.isEmpty(),
               "missing yt-dlp is reported before scheduling")) {
        return false;
    }

    const QString input = output.filePath("input.mp4");
    QFile inputFile(input);
    if (!check(inputFile.open(QIODevice::WriteOnly), "create missing-tool conversion input")) return false;
    inputFile.close();

    ConversionManager conversionManager(nullptr, output.filePath("missing-ffmpeg"));
    const ConversionEnqueueResult conversionResult =
        conversionManager.enqueueConversion(conversionFor(input, output.path(), 0));
    return check(!conversionResult.accepted && !conversionResult.error.isEmpty(),
                 "missing FFmpeg is reported before scheduling");
}

bool testConversionQueue(const QString &toolPath)
{
    QTemporaryDir output;
    if (!check(output.isValid(), "temporary conversion directory")) return false;
    const QString input = output.filePath("input.mp4");
    QFile inputFile(input);
    if (!check(inputFile.open(QIODevice::WriteOnly), "create conversion input")) return false;
    inputFile.write("input");
    inputFile.close();

    ConversionManager manager(nullptr, toolPath);
    QSet<ConversionId> active;
    int maximumActive = 0;
    int completions = 0;
    int progressEvents = 0;
    bool sawMidProgress = false;
    QObject::connect(&manager, &ConversionManager::conversionStatus,
                     [&active, &maximumActive](ConversionId id, DownloadId, const QString &message) {
        if (message == "Convertendo mídia...") active.insert(id);
        maximumActive = qMax(maximumActive, active.size());
    });
    QObject::connect(&manager, &ConversionManager::conversionCompleted,
                     [&active, &completions](ConversionId id, DownloadId, const QString &) {
        active.remove(id);
        ++completions;
    });
    QObject::connect(&manager, &ConversionManager::conversionFailed,
                     [&active](ConversionId id, DownloadId, const QString &) { active.remove(id); });
    QObject::connect(&manager, &ConversionManager::conversionProgress,
                     [&progressEvents, &sawMidProgress](ConversionId, DownloadId, double percent) {
        ++progressEvents;
        sawMidProgress = sawMidProgress || (percent >= 49.0 && percent <= 51.0);
    });

    if (!check(manager.enqueueConversion(conversionFor(input, output.path(), 101)).accepted,
               "first automatic conversion accepted")) return false;
    if (!check(manager.enqueueConversion(conversionFor(input, output.path(), 102)).accepted,
               "second automatic conversion accepted")) return false;
    if (!check(waitUntil([&manager]() { return !manager.hasWork(); }), "conversion queue completes")) return false;
    if (!check(completions == 2 && maximumActive == 1, "automatic conversions are strictly serial")
        || !check(progressEvents >= 4 && sawMidProgress,
                  "conversion progress is parsed and reaches the UI")) return false;

    int manualCompleted = 0;
    int automaticCancelled = 0;
    QObject::connect(&manager, &ConversionManager::conversionCompleted,
                     [&manualCompleted](ConversionId, DownloadId owner, const QString &) {
        if (owner == 0) ++manualCompleted;
    });
    QObject::connect(&manager, &ConversionManager::conversionCancelled,
                     [&automaticCancelled](ConversionId, DownloadId owner) {
        if (owner != 0) ++automaticCancelled;
    });
    if (!check(manager.enqueueConversion(conversionFor(input, output.path(), 0)).accepted,
               "manual conversion accepted")) return false;
    if (!check(manager.enqueueConversion(conversionFor(input, output.path(), 201)).accepted,
               "automatic conversion accepted beside manual")) return false;
    manager.cancelAllAutomatic();
    if (!check(waitUntil([&manager]() { return !manager.hasWork(); }), "manual conversion completes after automatic cancellation")) return false;
    return check(manualCompleted == 1, "cancel all automatic preserves manual conversion")
        && check(automaticCancelled == 1, "automatic conversion was cancelled");
}

}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    if (argc < 2) {
        std::cerr << "Fake media tool path is required\n";
        return 2;
    }
    const QString toolPath = QString::fromLocal8Bit(argv[1]);
    const bool success = testMissingToolsAreReported()
        && testDownloadScheduling(toolPath)
        && testReportedOutputPathVariants(toolPath)
        && testDuplicateCancellationAndLimit(toolPath)
        && testConversionQueue(toolPath);
    return success ? 0 : 1;
}
