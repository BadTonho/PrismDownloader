<p align="center">
  <img src="app_icon.png" width="140" alt="Prism Downloader Icon Logo" style="border-radius: 24px; box-shadow: 0px 4px 20px rgba(0,255,150,0.2);"/>
</p>

<h1 align="center">💎 Prism Downloader — Tonho Studios Suite</h1>

<p align="center">
  <b>The most comprehensive, fast, and intelligent desktop suite for media download, trimming, and conversion.</b><br>
  Engineered with high-precision in <b>Pure C++17</b> and <b>Qt 6</b>, featuring native hardware acceleration and advanced telemetry logs.
</p>

<p align="center">
  <a href="README_PT.md">🇧🇷 Leia a versão em Português do Brasil (PT-BR) aqui.</a>
</p>

<p align="center">
  <a href="https://github.com/BadTonho/PrismDownloader/releases/latest"><img src="https://img.shields.io/github/v/release/BadTonho/PrismDownloader?color=00e676&label=Official%20Release&style=for-the-badge&logo=github" alt="GitHub Release"></a>
  <a href="https://isocpp.org/"><img src="https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white" alt="C++17"></a>
  <a href="https://www.qt.io/"><img src="https://img.shields.io/badge/Qt_GUI-6.7%20Dark%20Tech-41CD52?style=for-the-badge&logo=qt&logoColor=white" alt="Qt 6"></a>
  <a href="https://developer.nvidia.com/video-codec-sdk"><img src="https://img.shields.io/badge/NVIDIA-NVENC%20Hardware-76B900?style=for-the-badge&logo=nvidia&logoColor=white" alt="NVIDIA NVENC"></a>
  <img src="https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-00a859?style=for-the-badge&logo=linux&logoColor=white" alt="Windows and Linux">
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-6b21a8?style=for-the-badge&logo=open-source-initiative&logoColor=white" alt="MIT License"></a>
</p>

---

## ⚡ What is Prism Downloader?

Developed by **Tonho Studios**, **Prism Downloader** brings a clean desktop workflow for downloading and processing audio and video. It combines high-performance **C++17**, **Qt 6**, and the industry-standard `yt-dlp` and `FFmpeg` engines on Windows and Linux.

---

## 🔥 Key Features

### 🚦 Concurrent Session Queue
* Add as many URLs as needed and run **1 to 5 downloads simultaneously** (default: 2), with isolated progress, speed, ETA, status, and cancellation for every task.
* Automatic and manual conversions share a FIFO queue with exactly one active FFmpeg process, avoiding GPU/CPU overload.

### 🎬 1. Time-Slice Extraction (Trimming)
* **Save bandwidth by downloading only what you need!**
* Define the exact start and end times directly in the panel (e.g., from `00:03:15` to `00:05:45`). The engine requests and downloads only the target section in record time.

### ⚡ 2. Low-Level Graphics Acceleration (NVENC, VAAPI, AMF & QSV)
* **Hardware Acceleration with Safe Fallback:** Windows uses native device detection; Linux tests the encoders exposed by FFmpeg and the DRM device (`/dev/dri`). NVIDIA NVENC, AMD/Intel VAAPI, AMD AMF, and Intel QSV are used only when they actually initialize; CPU conversion remains the reliable fallback.
* The selected codec is shown on the conversion screen. On AMD Linux systems the backend is normally `h264_vaapi`/`hevc_vaapi`, while FFmpeg falls back to CPU if the driver or device permissions are unavailable.
* To diagnose a Linux installation without opening the UI, run `prism-downloader --diagnose-gpu`. It tests each encoder and DRM device and prints the complete FFmpeg output.

### 📡 3. Real-Time Log Terminal with Telemetry Filters
* Track every step of the download, conversion, and audio extraction through an integrated terminal in the application's dedicated tab.
* **Dynamic Filter Toolbar:** Switch on the fly between:
  * 🌐 **All Logs:** Full console stream.
  * ⚙️ **Processes:** Raw command output and Windows sub-process telemetry.
  * ❌ **Errors:** Quick visual isolation of error codes and warnings highlighted in red.
  * 📌 **System & General:** Auto-updater, hardware detection notifications, and engine status logs.
  * 🧹 **Clear Terminal:** Clear the display buffer instantly.

### 🛡️ 4. Graphical Process Management
* No terminal window is opened while downloads run.
* The engine uses **`QProcess::start(program, arguments)`** with strict exit-code monitoring. On Linux, each download has its own session so cancellation also stops child FFmpeg processes.

### ☁️ 5. Tonho Studios Update Center
* **GitHub Cloud Synchronization:** Asynchronous update checking on startup with support for silent backgrounds.
* **Secure self-update for Windows and Linux:** Each release ships a signed Ed25519 manifest containing the SHA-256 of the Setup, Portable, and DEB packages. Prism rejects unsigned, incomplete, or altered releases before installation. Automatic download and installation are opt-in; otherwise the update center presents a **Download and update now** action.
* **Bundled, updatable yt-dlp engine:** Packages include an official `yt-dlp` Nightly. The update center shows the selected version and source, then downloads an update only after confirmation and SHA-256 validation.
* **Newest-version preference:** A newer manually installed copy on `PATH` is also detected. Prism never changes that installation; its own updates stay in the user's data directory.

---

## 📦 Installation & Usage

Visit the [Official GitHub Releases page](https://github.com/BadTonho/PrismDownloader/releases/latest) to download the package for your platform:

| Package | Purpose |
| :--- | :--- |
| ⭐ **[`PrismDownloader_vX.X.X_Setup.exe`](https://github.com/BadTonho/PrismDownloader/releases/latest)** | **Official Tonho Studios Installer** (Recommended). Complete setup wizard in Portuguese (Brazil) with Desktop shortcut creation. |
| 💼 **[`PrismDownloader_vX.X.X_Portable.zip`](https://github.com/BadTonho/PrismDownloader/releases/latest)** | **Portable Archive**. Extract anywhere on your Hard Drive or USB flash drive and run immediately. |
| 🐧 **[`prism-downloader_X.Y.Z_amd64.deb`](https://github.com/BadTonho/PrismDownloader/releases/latest)** | Package for **Linux Mint 22 / Ubuntu 24.04 amd64**. Install with `sudo apt install ./prism-downloader_X.Y.Z_amd64.deb`. It bundles yt-dlp Nightly and APT installs FFmpeg. |

---

## 🛠️ Compiling from Source (For Developers)

### Linux Mint 22 / Ubuntu 24.04 (amd64)

Install the development requirements and build:

```bash
sudo apt update
sudo apt install build-essential cmake qt6-base-dev qt6-base-dev-tools libssl-dev ffmpeg dpkg-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cpack --config build/CPackConfig.cmake -G DEB -B package
sudo apt install ./package/prism-downloader_*.deb
```

The package targets Mint 22 and Ubuntu 24.04 on `amd64`. It installs the official `yt-dlp_linux` Nightly at `/usr/lib/prism-downloader/yt-dlp` and uses the system `ffmpeg`. CMake downloads the pinned Nightly and verifies its SHA-256 during configuration, so package builds require internet access. The application can install a newer Nightly without `sudo` in the user's data directory and also selects a newer manually installed copy from `PATH`.

### Windows 10 / 11 (64-bit)

To build the native application on your Windows machine:

### System Requirements
* **Windows 10 or 11 (64-bit)**
* **Visual Studio 2019 / 2022** (with MSVC C++ x64 compiler toolset)
* **Qt 6.7+** (specifically `Widgets` and `Network` modules)
* **OpenSSL 1.1.1+ development files** (used to verify Ed25519 update manifests)
* **CMake 3.16+**
* **Inno Setup 6+** (optional, to generate the `.exe` setup file)

### Quick Build via CMake (PowerShell)
```powershell
# 1. Clone the repository
git clone https://github.com/BadTonho/PrismDownloader.git
cd PrismDownloader

# 2. Configure project files targeting Visual Studio
cmake -S . -B build -G "Visual Studio 17 2022" -A x64

# 3. Build in Release configuration
cmake --build build --config Release

# 4. Deploy Qt dependencies and run the executable
C:\Qt\6.7.2\msvc2019_64\bin\windeployqt.exe --release --no-translations .\build\Release\PrismDownloader.exe
.\build\Release\PrismDownloader.exe
```

For signed release builds, see [update-signing setup](docs/UPDATE_SIGNING.md). The CI workflow injects the public verification key from the GitHub secret; local development builds can omit it, but will intentionally refuse self-update checks.

---

## ⚖️ License and Copyrights

This software is released under the terms of the **MIT License** (see [LICENSE](LICENSE) for details).  
The community is free to use, modify, and redistribute this project, **provided the copyright notices remain intact (Copyright © Tonho Studios)**.

* To report vulnerabilities, refer to our [Security Policy](SECURITY.md).
* Want to help improve the C++ codebase? Read our [Contributing Guide](CONTRIBUTING.md).

---

<p align="center">
  <b>Developed with ☕ and passion by <a href="https://github.com/BadTonho">Tonho Studios</a></b><br>
  <i>All rights reserved • C++ Engineering & Premium Desktop Visual Experience.</i>
</p>
