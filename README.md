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
  <a href="https://www.microsoft.com/windows"><img src="https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011-0078D6?style=for-the-badge&logo=windows&logoColor=white" alt="Windows"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-6b21a8?style=for-the-badge&logo=open-source-initiative&logoColor=white" alt="MIT License"></a>
</p>

---

## ⚡ What is Prism Downloader?

Developed by **Tonho Studios**, **Prism Downloader** was born to revolutionize the audio and video download and processing experience on Windows desktops. Like an optical prism that bends and organizes beams of light, the suite combines the power of high-performance **C++17** with industry-standard engines (`yt-dlp` and `FFmpeg`) executed directly on the Win32 API to deliver an ultra-fast, clean, secure, and transparent workflow.

---

## 🔥 Key Features

### 🎬 1. Time-Slice Extraction (Trimming)
* **Save bandwidth by downloading only what you need!**
* Define the exact start and end times directly in the panel (e.g., from `00:03:15` to `00:05:45`). The engine requests and downloads only the target section in record time.

### ⚡ 2. Low-Level Graphics Acceleration (NVIDIA NVENC, AMD & QSV)
* **High-Speed RAM Detection (0.001ms):** The system queries the native Win32 API (`EnumDisplayDevicesA`) during startup to identify dedicated graphics processors (fully optimized for **NVIDIA GeForce GTX 1660 SUPER** and above, as well as **AMD Radeon AMF** and **Intel QSV**).
* All format conversions utilize hardware codecs (`h264_nvenc`), sparing 100% of your CPU and completing rendering jobs in mere seconds.

### 📡 3. Real-Time Log Terminal with Telemetry Filters
* Track every step of the download, conversion, and audio extraction through an integrated terminal in the application's dedicated tab.
* **Dynamic Filter Toolbar:** Switch on the fly between:
  * 🌐 **All Logs:** Full console stream.
  * ⚙️ **Processes:** Raw command output and Windows sub-process telemetry.
  * ❌ **Errors:** Quick visual isolation of error codes and warnings highlighted in red.
  * 📌 **System & General:** Auto-updater, hardware detection notifications, and engine status logs.
  * 🧹 **Clear Terminal:** Clear the display buffer instantly.

### 🛡️ 4. Graphical Sandboxing and Zero Terminal Windows (`CREATE_NO_WINDOW`)
* No more black CMD or Windows Terminal windows flashing on your screen while downloading!
* The engine executes binaries at a low level via **`QProcess::startCommand`** using Win32 Kernel creation flags (`0x08000000`), ensuring a 100% graphical, silent operation with strict `exit code` monitoring to prevent false positive reports.

### ☁️ 5. Tonho Studios Update Center
* **GitHub Cloud Synchronization:** Asynchronous update checking on startup with support for silent backgrounds.
* **Decoupled Engine Updater:** Update the downloader's signature engine (`yt-dlp`) with a single click without needing to reinstall the entire suite.

---

## 📦 Installation & Usage

Visit our [Official Releases Page on GitHub](https://github.com/BadTonho/PrismDownloader/releases/latest) to download the standalone Windows binaries (pre-packaged with FFmpeg and yt-dlp):

| Package | Purpose |
| :--- | :--- |
| ⭐ **[`PrismDownloader_vX.X.X_Setup.exe`](https://github.com/BadTonho/PrismDownloader/releases/latest)** | **Official Tonho Studios Installer** (Recommended). Complete setup wizard in Portuguese (Brazil) with Desktop shortcut creation. |
| 💼 **[`PrismDownloader_vX.X.X_Portable.zip`](https://github.com/BadTonho/PrismDownloader/releases/latest)** | **Portable Archive**. Extract anywhere on your Hard Drive or USB flash drive and run immediately. |

---

## 🛠️ Compiling from Source (For Developers)

To build the native application on your Windows machine:

### System Requirements
* **Windows 10 or 11 (64-bit)**
* **Visual Studio 2019 / 2022** (with MSVC C++ x64 compiler toolset)
* **Qt 6.7+** (specifically `Widgets` and `Network` modules)
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

---

## ⚖️ License and Copyrights

This software is released under the terms of the **MIT License** (see [LICENSE](LICENSE) for details).  
The community is free to use, modify, and redistribute this project, **provided the copyright notices remain intact (Copyright © Tonho Studios)**.

* To report vulnerabilities, refer to our [Security Policy](SECURITY.md).
* Want to help improve the C++ codebase? Read our [Contributing Guide](CONTRIBUTING.md).

---

<p align="center">
  <b>Developed with ☕ and passion by <a href="https://github.com/BadTonho">Tonho Studios</a></b><br>
  <i>All rights reserved • C++ Engineering & Premium Windows Visual Experience.</i>
</p>
