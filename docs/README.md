# 📚 Official Documentation — Prism Downloader

<p align="center">
  <a href="pt/README.md">🇧🇷 Leia a versão em Português do Brasil (PT-BR) aqui.</a>
</p>

Welcome to the official documentation for **Prism Downloader**, the high-performance desktop media suite developed in **Pure C++17** and **Qt 6**, designed for high-speed downloads, lossless stream-copy muxing, audio extraction, time-slice trimming, GPU-accelerated conversions, and SHA-256-validated automatic self-updates.

---

## 🧭 Documentation Index

The documentation is organized into focused, modular technical guides:

| Guide | Description | Portuguese Version |
| :--- | :--- | :--- |
| 🏗️ [**System Architecture & Concurrency**](ARCHITECTURE.md) | Architectural blueprint, decoupled signal/slot reactive pipeline, process isolation model, and concurrent queue scheduling. | [ARQUITETURA.md](pt/ARQUITETURA.md) |
| 🧩 [**Modules & Code Reference**](MODULES.md) | Deep technical breakdown of every class, data structure, enum, public method, and Qt signal/slot across the codebase. | [MODULOS.md](pt/MODULOS.md) |
| ⚡ [**Hardware & GPU Acceleration**](HARDWARE_GPU.md) | Multi-vendor hardware detection (Windows DXGI & Linux DRM/VAAPI), encoder matrix (NVENC, AMF, QSV, VAAPI), safe CPU fallback, and CLI diagnostic mode. | [HARDWARE_GPU.md](pt/HARDWARE_GPU.md) |
| 🛡️ [**Automatic Updates**](AUTO_UPDATE.md) | Self-updating system with streaming SHA-256 hash checks, independent `yt-dlp` updater, and portable staging. | [AUTO_UPDATE.md](pt/AUTO_UPDATE.md) |
| 🎨 [**User Interface & Features Guide**](INTERFACE_UI.md) | Visual guide to the Dark Tech Qt 6 UI, tab workflows (Downloads, Library, Converter, Logs, Updates), playlist modal, and real-time telemetry filters. | [INTERFACE_UI.md](pt/INTERFACE_UI.md) |
| 📦 [**Build & Packaging Guide**](BUILD_PACKAGING.md) | Step-by-step developer compilation instructions for Windows (MSVC/Inno Setup) and Linux (GCC/Clang/CPack DEB), plus automated CTest execution. | [BUILD_PACKAGING.md](pt/BUILD_PACKAGING.md) |

---

## 🛠️ Technology Stack & Dependencies

* **Core Language:** ISO C++17 Standard.
* **UI Framework & Core Engine:** Qt 6.7+ (`QtWidgets`, `QtNetwork`, `QtCore`, `QtGui`).
* **Integrated Media Engines:**
  * `yt-dlp` (Official Nightly release channel for media stream analysis and downloading).
  * `FFmpeg` (Lossless stream-copy muxing, hardware-accelerated video transcode, and high-fidelity audio extraction).
* **Build System & Testing:** CMake 3.16+ and CTest unit testing framework.
* **Packaging Solutions:**
  * Windows: Inno Setup 6 (Official Setup `.exe` wizard) and standalone portable archive (`.zip`).
  * Linux: CPack / `dpkg-dev` (Native `.deb` package targeting Ubuntu 24.04 and Linux Mint 22).

---

## 📌 System Requirements

### Windows
* **Operating System:** Windows 10 (64-bit) or Windows 11.
* **Recommended GPU:** NVIDIA GeForce (GTX 900+ / RTX), AMD Radeon (GCN/RDNA), or Intel Core / Arc Graphics.
* **CPU Fallback:** Universal multi-threaded execution on any modern x86_64 processor.

### Linux
* **Target Distributions:** Linux Mint 22 (Wilma) / Ubuntu 24.04 LTS (Noble Numbat) `amd64`.
* **System Packages:** `ffmpeg`, `qt6-base-dev`.
* **Hardware GPU Access:** User access to `/dev/dri/renderD128` (member of `render` or `video` groups).
