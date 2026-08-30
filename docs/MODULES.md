# 🧩 Modules & Code Reference — Prism Downloader

<p align="center">
  <a href="pt/MODULOS.md">🇧🇷 Leia a versão em Português do Brasil (PT-BR) aqui.</a>
</p>

This document provides complete technical specifications for all classes, data structures, enums, public APIs, and Qt signal/slot interfaces in **Prism Downloader**.

---

## 📑 Table of Contents

1. [Core Data Types & Models](#1-core-data-types--models)
   - `MediaItem.h`
   - `DownloadProfile.h`
2. [Download Queue Engine (`DownloadManager`)](#2-download-queue-engine-downloadmanager)
3. [Transcoding & Muxing Engine (`ConversionManager`)](#3-transcoding--muxing-engine-conversionmanager)
4. [Hardware & GPU Probing (`GPUDetector`)](#4-hardware--gpu-probing-gpudetector)
5. [Dynamic Tool Resolver (`MediaToolResolver`)](#5-dynamic-tool-resolver-mediatoolresolver)
6. [App Update Service (`AppUpdateService`)](#6-app-update-service-appupdateservice)
7. [Engine Update Service (`YtDlpUpdateService`)](#7-engine-update-service-ytdlpupdateservice)
8. [Portable Staging Helper (`PortableUpdateHelper` & `PortableUpdateCommon`)](#8-portable-staging-helper-portableupdatehelper--portableupdatecommon)
9. [Main Application Controller (`MainWindow`)](#9-main-application-controller-mainwindow)

---

## 1. Core Data Types & Models

### 1.1. `MediaItem.h`
Defines the standard state enum and model for downloaded assets:

```cpp
enum class DownloadStatus {
    Queued,         // In queue waiting for an available concurrency worker slot
    Downloading,    // yt-dlp actively pulling video/audio streams
    Muxing,         // Lossless Stream Copy merge via FFmpeg
    ConvertingGPU,  // Active transcode via GPU hardware or multi-threaded CPU
    Cancelling,     // Cancellation signal dispatched, terminating process tree
    Completed,      // Download and post-processing successfully finished
    Error,          // Network failure, malformed URL, or non-zero process exit
    Cancelled       // Aborted by user request
};
```

#### Struct `MediaItem`
* `std::string url`: Input media URL.
* `std::string title`: Media title resolved from the online platform.
* `std::string quality`: Quality tag (e.g., `"1080p"`, `"4K"`, `"MP3"`).
* `std::string speed`: Formatted download rate (e.g., `"12.5 MB/s"`).
* `std::string eta`: Estimated completion time (e.g., `"01:30"`).
* `double progress`: Progress value ranging from 0.0 to 100.0.
* `DownloadStatus status`: Current lifecycle state.
* `bool isAudioOnly() const`: Evaluates whether the requested quality profile requires extracting audio only (`"MP3"`, `"FLAC"`, `"AUDIO"`).

---

### 1.2. `DownloadProfile.h`
Converts human-readable quality choices into optimized `yt-dlp` format selection strings:

* `formatSelectorForQuality(const std::string &quality)`:
  - `"1080P"` $\rightarrow$ `bv*[height<=1080]+ba/b[height<=1080]`
  - `"720P"` $\rightarrow$ `bv*[height<=720]+ba/b[height<=720]`
  - `"4K"` $\rightarrow$ `bv*[height<=2160]+ba/b[height<=2160]`
  - Default $\rightarrow$ `bv*+ba/b` (Best video and audio streams available).

---

## 2. Download Queue Engine (`DownloadManager`)

**Header & Implementation:** `src/DownloadManager.h` and `src/DownloadManager.cpp`

Orchestrates asynchronous `yt-dlp` execution, concurrency limiting, and process output parsing.

### Types and Structs
* `using DownloadId = quint64`: Unique, auto-incrementing identifier for each download request.
* `struct DownloadRequest`:
  - `QUrl url`: Validated media URL.
  - `QString quality`: Selected resolution or audio profile.
  - `QString timeRange`: Start and end trimming boundaries (e.g., `"00:01:00-00:03:00"`).
  - `QString outputDirectory`: Absolute destination directory path.
* `struct EnqueueResult`:
  - `bool accepted`: True if accepted into the queue.
  - `DownloadId id`: Assigned download identifier.
  - `QString error`: Rejection explanation if invalid.

### Public Methods
* `EnqueueResult enqueueDownload(const DownloadRequest &request)`: Adds request to pending queue and triggers scheduler.
* `bool cancelDownload(DownloadId id)`: Gracefully terminates subprocess associated with the given ID.
* `void cancelAll()`: Terminates all active jobs and purges the pending queue.
* `void setConcurrencyLimit(int limit)`: Configures maximum simultaneous workers (1 to 5).
* `int concurrencyLimit() const`: Returns configured worker limit.
* `int activeCount() const`: Number of currently running downloads.
* `int pendingCount() const`: Number of tasks waiting in queue.
* `bool hasWork() const`: True if jobs are either active or pending.

### Qt Signals
* `void jobProgress(DownloadId id, double percent, const QString &speed, const QString &eta)`: Live telemetry.
* `void jobStatus(DownloadId id, DownloadStatus status, const QString &message)`: Status transition.
* `void jobCompleted(DownloadId id, const QString &filePath)`: Emitted when target file is ready on disk.
* `void jobLog(DownloadId id, const QString &message)`: Raw console output captured from the child process.
* `void queueStateChanged(int active, int pending)`: Queue load notifications.
* `void queueIdle()`: Emitted when all tasks finish.

---

## 3. Transcoding & Muxing Engine (`ConversionManager`)

**Header & Implementation:** `src/ConversionManager.h` and `src/ConversionManager.cpp`

Manages sequential FIFO transcoding jobs via `FFmpeg` to prevent hardware overloading.

### Types and Structs
* `using ConversionId = quint64`: Unique identifier for transcode tasks.
* `struct ConversionRequest`:
  - `DownloadId ownerDownloadId`: Linked download ID or `0` for manual conversions.
  - `QString inputFile`: Absolute source media path.
  - `QString format`: Output format container (`"MP3"`, `"FLAC"`, `"MP4"`, `"MKV"`, etc.).
  - `QString outputDirectory`: Target directory.
  - `GPUType gpuType`: Selected acceleration backend (`NVIDIA`, `AMD`, `INTEL`, `VAAPI`, `CPU_ONLY`).
  - `QString gpuCodec`: Hardware encoder identifier (e.g., `"h264_nvenc"`).
  - `QString gpuDevice`: Device path (e.g., `"/dev/dri/renderD128"` on Linux).

### Public Methods
* `ConversionEnqueueResult enqueueConversion(const ConversionRequest &request)`: Adds conversion to FIFO queue.
* `bool cancelConversion(ConversionId id)`: Cancels active or queued transcode.
* `void cancelByDownloadId(DownloadId downloadId)`: Cancels transcode tied to a specific download ID.
* `void cancelAllAutomatic()`: Purges queued automatic post-download conversions.
* `bool hasWork() const`: True if a conversion is active or pending.

### Qt Signals
* `void conversionQueued(ConversionId id, DownloadId ownerDownloadId, int position)`: Queue position notification.
* `void conversionStatus(ConversionId id, DownloadId ownerDownloadId, const QString &message)`: Status text.
* `void conversionCompleted(ConversionId id, DownloadId ownerDownloadId, const QString &outputFile)`: Finished file path.
* `void conversionFailed(ConversionId id, DownloadId ownerDownloadId, const QString &message)`: Failure details.
* `void conversionCancelled(ConversionId id, DownloadId ownerDownloadId)`: Task cancelled.
* `void conversionLog(ConversionId id, DownloadId ownerDownloadId, const QString &message)`: FFmpeg log stream.

---

## 4. Hardware & GPU Probing (`GPUDetector`)

**Header & Implementation:** `src/GPUDetector.h` and `src/GPUDetector.cpp`

Lightweight, native C++ hardware inspection utility.

### Methods and Properties
* `enum class GPUType { NVIDIA, AMD, INTEL, VAAPI, CPU_ONLY };`
* `void detect(bool verbose = false)`: Scans hardware adapters. When `verbose = true`, prints diagnostic details to standard output (used by `--diagnose-gpu`).
* `GPUType getGPUType() const`: Vendor/technology classification.
* `std::string getGPUName() const`: Device name (e.g., `"NVIDIA GeForce GTX 1660 SUPER"`).
* `std::string getRecommendedCodec() const`: Primary FFmpeg encoder (e.g., `"h264_nvenc"`, `"h264_vaapi"`, `"libx264"`).
* `std::string getHardwareDevice() const`: Hardware DRM/D3D11 node.
* `std::string getDiagnostic() const`: Full diagnostic summary text.
* `bool hasHardwareAcceleration() const`: Returns true if a compatible GPU encoder was validated.

---

## 5. Dynamic Tool Resolver (`MediaToolResolver`)

**Header & Implementation:** `src/MediaToolResolver.h` and `src/MediaToolResolver.cpp`

Discovers, verifies, and prioritizes `yt-dlp` and `FFmpeg` binaries.

* `resolve(MediaTool tool, const QString &programPath = {})`: Returns verified path to the best executable.
* `resolveInfo(MediaTool tool, const QString &programPath = {})`: Returns detailed `MediaToolInfo` struct.
* `selectYtDlpCandidate(const QList<MediaToolInfo> &candidates)`: Selects the newest binary across User Update, Bundle, and PATH.
* `isVersionNewer(const QString &candidate, const QString &current)`: Performs semantic version comparison.
* `ytDlpUserPath()`: Returns persistent user storage path (`%LOCALAPPDATA%/PrismDownloader/yt-dlp.exe` or `~/.local/share/prism-downloader/yt-dlp`).

---

## 6. App Update Service (`AppUpdateService`)

**Header & Implementation:** `src/AppUpdateService.h`, `src/AppUpdateService.cpp`

Provides application update checking, manifest validation, and streaming download hashing.

* `parseRelease(...)`: Parses GitHub release JSON, validates the release manifest, and selects the package for the current platform.
* `checkLatestRelease()`: Asynchronously checks for new published releases.
* `downloadLatestRelease()`: Streams package download while calculating SHA-256 in real time.
* `hasValidChecksum(...)`: Validates computed payload SHA-256 against the verified manifest entry.

---

## 7. Engine Update Service (`YtDlpUpdateService`)

**Header & Implementation:** `src/YtDlpUpdateService.h` and `src/YtDlpUpdateService.cpp`

Keeps the `yt-dlp` media extraction engine up to date with official Nightly releases.

* `checkLatestRelease()`: Queries GitHub Releases for yt-dlp Nightly assets.
* `installLatestRelease()`: Downloads official `SHA2-256SUMS`, verifies binary checksum, and writes executable to user storage without requiring administrator privileges.

---

## 8. Portable Staging Helper (`PortableUpdateHelper` & `PortableUpdateCommon`)

**Header & Implementation:** `src/PortableUpdateCommon.h`, `src/PortableUpdateCommon.cpp`, `src/PortableUpdateHelper.cpp`

Detached helper application (`portable-update-helper.exe`) that executes seamless in-place updates for portable Windows installations:
1. Waits for parent process PID termination (`WaitForSingleObject`).
2. Extracts updated archive to temporary staging folder (`.prism-update-staging-XXXXXX`).
3. Creates atomic backup of previous version (`.prism-update-backup-<PID>`).
4. Replaces target directory, performs rollback if extraction fails, and launches the updated binary via `QProcess::startDetached`.

---

## 9. Main Application Controller (`MainWindow`)

**Header & Implementation:** `src/MainWindow.h` and `src/MainWindow.cpp`

Primary Qt 6 controller coordinating UI presentation, user interactions, and core services.

* **Navigation Sidebar:** Tab switching for Downloads, Library, Converter, Logs, and Settings/Updates.
* **Live Queue Table:** Real-time updates for progress, speed, ETA, and job statuses.
* **Telemetry Terminal:** Dedicated logging view with live filtering modes (All, Processes, Errors, General).
* **Playlist Preview Modal:** Interactive dialog enabling selective batch enqueueing from YouTube playlists.
