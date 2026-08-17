# 🏗️ System Architecture & Concurrency — Prism Downloader

<p align="center">
  <a href="pt/ARQUITETURA.md">🇧🇷 Leia a versão em Português do Brasil (PT-BR) aqui.</a>
</p>

This document provides a comprehensive technical overview of the software architecture, data flow pipeline, process management, and concurrency models in **Prism Downloader**, engineered with **Pure C++17** and **Qt 6**.

---

## 📐 1. Architectural Overview

Prism Downloader follows an asynchronous, decoupled architectural pattern powered by the **Qt Signal and Slot mechanism** and **isolated child process execution (`QProcess`)**. Heavy network operations (`yt-dlp` stream extraction) and intensive multimedia processing (`FFmpeg` muxing and hardware transcode) run in asynchronous subprocesses, keeping the graphical user interface thread (UI Thread) completely responsive at all times.

```mermaid
flowchart TD
    subgraph UI_Layer ["Presentation Layer (GUI - Qt 6)"]
        MW["MainWindow (Dark Tech Interface)"]
        DL_Tab["Downloads Tab"]
        LIB_Tab["Media Library Tab"]
        CONV_Tab["Converter Tab"]
        LOG_Tab["Telemetry & Logs Terminal"]
        UPD_Tab["Update Center Tab"]
    end

    subgraph Core_Services ["Core Services & Logic Layer (C++17)"]
        DM["DownloadManager\n(Concurrent Queue 1-5 Workers)"]
        CM["ConversionManager\n(Strict FIFO Single-Job Queue)"]
        GD["GPUDetector\n(Universal Hardware Probing)"]
        MTR["MediaToolResolver\n(Dynamic Binary Resolution)"]
        AUS["AppUpdateService\n(Ed25519 & SHA-256 Validation)"]
        YUS["YtDlpUpdateService\n(Autonomous Nightly Updater)"]
    end

    subgraph Engine_Layer ["Subprocess Execution Layer"]
        YTDLP["yt-dlp (Nightly Engine)"]
        FFMPEG["FFmpeg (Stream Copy / NVENC / VAAPI)"]
    end

    MW --> DL_Tab & LIB_Tab & CONV_Tab & LOG_Tab & UPD_Tab
    DL_Tab --> DM
    CONV_Tab --> CM
    DM --> CM
    DM --> YTDLP
    CM --> FFMPEG
    DM -.-> MTR
    CM -.-> MTR
    UPD_Tab --> AUS & YUS
    MW -.-> GD
```

---

## 🔄 2. End-to-End Data Flow

The lifecycle of a media task progresses through well-defined, deterministic states:

```mermaid
sequenceDiagram
    autonumber
    actor User as User
    participant UI as MainWindow
    participant DM as DownloadManager
    participant YTDLP as yt-dlp Process
    participant CM as ConversionManager
    participant FFMPEG as FFmpeg Process
    participant FS as Local Filesystem

    User->>UI: Submits URL, selects quality profile, clicks Start
    UI->>DM: enqueueDownload(DownloadRequest)
    DM->>DM: Enqueues job and checks concurrency limits
    DM->>YTDLP: Launches QProcess with tailored parameters
    loop Real-Time Telemetry Stream
        YTDLP-->>DM: Line-buffered stdout/stderr (progress, speed, ETA)
        DM-->>UI: jobProgress(...) / jobLog(...)
        UI-->>User: Updates real-time progress bar and queue row
    end
    YTDLP->>DM: Process finishes (ExitCode 0)
    DM->>UI: jobCompleted(DownloadId, filePath)
    
    alt Post-Download Conversion / Audio Extraction Required
        UI->>CM: enqueueConversion(ConversionRequest)
        CM->>CM: Enqueues into FIFO transcode queue
        CM->>FFMPEG: Launches QProcess with hardware codec (or CPU fallback)
        FFMPEG-->>CM: Transcode telemetry / log stream
        FFMPEG->>CM: Process finishes successfully
        CM->>UI: conversionCompleted(...)
    end

    UI->>FS: Refreshes Media Library index
    UI-->>User: Emits completion notification and enables instant playback
```

---

## ⚙️ 3. Concurrency & Queue Management

### 3.1. Concurrent Download Queue (`DownloadManager`)
* **Dynamic Concurrency Control:** Users can adjust active download workers between **1 and 5 parallel tasks** on the fly via the UI (`m_concurrencySpin`).
* **Reactive Scheduler (`schedule()`):** When a job finishes, fails, is cancelled, or when concurrency limits are increased, the scheduler automatically pulls and activates the next pending job from `m_pending`.
* **Telemetry Line Parsing:** Asynchronous stdout and stderr buffers from `yt-dlp` are parsed in chunks using regular expressions and token extractors to capture:
  - Completion percentage (`%`).
  - Transfer speed (e.g., `14.2MiB/s`).
  - Estimated time of arrival (ETA).
  - Target destination file paths (`[Merger] Merging formats into "..."` or `[download] Destination: ...`).

### 3.2. Sequential Transcode Queue (`ConversionManager`)
* **Strict Single-Job FIFO Execution:** In contrast to network downloads, video and audio transcoding operations place heavy demands on physical hardware (VRAM allocation, hardware encoder channels on NVENC/AMF/QSV, and CPU core utilization).
* To protect the operating system against GPU driver crashes or 100% CPU saturation, `ConversionManager` executes exactly **one active conversion process at a time** (`m_active`), holding subsequent requests in an ordered queue (`m_pending`).

---

## 🛡️ 4. Process Lifecycle & Session Isolation

### 4.1. Zombie and Orphan Process Prevention
* Prism Downloader invokes subprocesses using `QProcess::start(program, arguments)` with structured argument lists, eliminating command-line shell injection risks and avoiding command prompt window flashes.
* On **Windows**, processes are tracked and cleanly terminated using Win32 process handles.
* On **Linux**, each download job spawns in its own process session (`setsid`). If `yt-dlp` launches child `ffmpeg` processes for post-processing, cancelling the task kills the entire process group, leaving zero background orphans.

### 4.2. Safe Application Shutdown (`closeEvent`)
* When closing the main window (`MainWindow::closeEvent`), the application performs a graceful teardown:
  1. Cancels all active and pending downloads (`m_downloadManager->cancelAll()`).
  2. Cancels all scheduled automatic conversions (`m_conversionManager->cancelAllAutomatic()`).
  3. Disconnects network listeners from update services.
  4. Releases file descriptors before exiting the Qt event loop.

---

## 🧩 5. Dynamic Binary Resolution Strategy (`MediaToolResolver`)

The application searches for `yt-dlp` and `FFmpeg` executables across several locations:

```mermaid
graph TD
    Start["Begin Tool Resolution"] --> CheckUser["1. User-updated binary in AppData / .local/share?"]
    CheckUser -- Yes --> CheckVer1["Compare semantic version against other candidates"]
    CheckUser -- No --> CheckBundled["2. Bundled binary in application folder?"]
    CheckBundled -- Yes --> CheckVer2["Compare version against system PATH"]
    CheckBundled -- No --> CheckPath["3. Available on System PATH?"]
    CheckVer1 --> PickNewest["Select highest semantic version"]
    CheckVer2 --> PickNewest
    CheckPath --> UsePath["Use PATH binary"]
    PickNewest --> Ready["Executable Selected and Ready"]
```

This multi-tiered resolution guarantees that:
1. End users can run the app out of the box without configuring environment variables.
2. Self-updating `yt-dlp` Nightly builds take precedence over outdated bundled binaries.
3. Newer system-wide tools installed by power users on `PATH` can be preferred automatically.
