# 🎨 User Interface & Features Guide — Prism Downloader

<p align="center">
  <a href="pt/INTERFACE_UI.md">🇧🇷 Leia a versão em Português do Brasil (PT-BR) aqui.</a>
</p>

This visual and functional guide covers the **Dark Tech Qt 6 interface** of **Prism Downloader**, engineered for high usability, clear visual hierarchy, and fast access to advanced features.

---

## 🖥️ 1. Design Layout & Navigation Model

Prism Downloader uses a **Fixed Left Navigation Sidebar** coupled with a **Dynamic Content Area (`QStackedWidget`)**, enabling immediate context switching across tasks without disrupting active downloads or transcoding jobs:

```
+-------------------------------------------------------------------------------+
|  💎 PRISM DOWNLOADER                              [—]  [□]  [✕] (Qt 6 Window) |
+----------------+--------------------------------------------------------------+
|  [📥 Downloads] |  URL: [ https://www.youtube.com/watch?v=...         ] [▶ START  ] |
|  [📚 Library  ] |  Quality:   [ 1080p Full HD ▼ ]   Trim:  [ 00:01:00 - 00:03:00 ] |
|  [🔄 Converter] |  Output:    [ C:/Users/.../Downloads           ] [📁 Browse]  |
|  [📡 Logs / CLI]|  ---------------------------------------------------------- |
|  [⚙️ Updates  ] |  LIVE DOWNLOAD QUEUE MONITOR (Concurrency: [ 2 ▲▼ ])        |
|                |  • Item 1 | 1080p | [██████████░░░░] 68% | 14.2 MB/s | ETA 00:15   |
|  [📁 Open Folder|  • Item 2 | MP3   | [██████████████] 100%| Completed | 00:00       |
|  [🔔 New Update |                                                              |
+----------------+--------------------------------------------------------------+
```

---

## 📑 2. Tab-by-Tab Feature Reference

### 2.1. 📥 Downloads Tab (Main Hub)
Centralizes web media downloads:

1. **URL Input Bar:** Supports single video links, live streams, audio clips, and full YouTube playlists.
2. **Quality & Format Selector:**
   - `4K Ultra HD` (maximum stream fidelity up to 2160p @ 60fps).
   - `1080p Full HD` (standard high-definition balance).
   - `720p HD` (bandwidth-efficient profile).
   - `Audio MP3 / FLAC` (automated high-bitrate extraction with metadata tagging).
3. **Time-Slice Trimming:**
   - Define exact start and end timestamps (`HH:MM:SS` or `MM:SS`).
   - The engine requests and downloads only the specified range, saving bandwidth and time.
4. **Concurrency Control:**
   - Spinbox supporting **1 to 5 concurrent download workers**.
5. **Real-Time Queue Monitor:**
   - Displays title, quality, isolated progress bar, instant speed (MB/s), ETA, and status messages.
   - Individual task cancellation and global `Cancel All` button.

---

### 2.2. 📚 Media Library Tab
Integrated local file manager:

* **Automatic Scanning:** Lists all videos and audio tracks stored in the configured download directory.
* **Metadata Columns:** Filename, format extension, file size on disk, and modification date.
* **Quick Actions:**
  - Double-clicking any item launches the file in the default system media player.
  - Action button opens the native file manager (Windows Explorer / Nautilus / Dolphin) highlighting the file.

---

### 2.3. 🔄 Media Converter Tab
Standalone converter for local files on disk:

* **Universal Format Ingestion:** Supports `.mp4`, `.mkv`, `.webm`, `.avi`, `.mov`, `.flv`, `.ts`, `.mp3`, `.wav`, `.aac`, `.flac`, `.ogg`, and more.
* **Target Container Presets:** Fast transcode to MP4/MKV/WebM or pure audio extraction to MP3/FLAC.
* **Hardware Acceleration Badge:** Displays active GPU silicon status (e.g., `NVIDIA NVENC (h264_nvenc)` or `VAAPI (h264_vaapi)`).
* **Safe FIFO Queue:** Restricts active transcoding to one job at a time, preventing GPU overheating or CPU saturation.

---

### 2.4. 📡 Telemetry & Logs Terminal Tab
Live terminal console capturing real-time engine activity:

```
[DYNAMIC FILTER TOOLBAR]
[🌐 All Logs]  [⚙️ Processes]  [❌ Errors]  [📌 General / System]  [🧹 Clear Terminal]
```

* 🌐 **All Logs:** Full telemetry console stream.
* ⚙️ **Processes:** Low-level standard output from `yt-dlp` and `FFmpeg`.
* ❌ **Errors:** Red visual isolation for non-zero exit codes, warnings, and connection failures.
* 📌 **General / System:** Lifecycle notifications, hardware detection events, and auto-update statuses.
* 🧹 **Clear Terminal:** Flushes the display buffer instantly.

---

### 2.5. ⚙️ Settings & Update Center Tab
Hardware overview and version management dashboard:

1. **GPU Diagnostics Card:**
   - Detected graphics adapter model and VRAM.
   - Verified hardware encoders.
   - Acceleration status indicator.
2. **Prism Core Updates:**
   - Displays installed version vs. latest GitHub release.
   - **"Check for Prism Updates"** action button.
   - Progress bar with real-time streaming SHA-256 calculation.
   - Toggles for automatic startup checks and automatic download.
3. **yt-dlp Engine Updates:**
   - Displays active `yt-dlp` binary version and source (Bundle, User, or PATH).
   - **"Update yt-dlp Engine"** button for official Nightly updates.

---

## 📋 3. YouTube Playlist Batch Workflow

When a playlist URL is pasted into the URL input bar, Prism Downloader opens the playlist inspection modal:

1. **Asynchronous Fetching:** Retrieves the playlist tracklist without freezing the UI.
2. **Selective Item Checklist:** Select specific videos or use "Select All" / "Deselect All".
3. **Batch Import:** Pushes all chosen tracks into the download queue in an ordered batch.
