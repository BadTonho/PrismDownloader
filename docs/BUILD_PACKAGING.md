# 📦 Build & Packaging Guide — Prism Downloader

<p align="center">
  <a href="pt/BUILD_PACKAGING.md">🇧🇷 Leia a versão em Português do Brasil (PT-BR) aqui.</a>
</p>

This guide outlines setup requirements, compilation steps, unit testing procedures, and package generation workflows for **Windows** and **Linux**.

---

## 💻 1. Windows Environment (10 / 11 64-bit)

### 1.1. Prerequisites
* **C++ Compiler:** Microsoft Visual C++ (MSVC) 2019 or 2022 (via *Visual Studio Community* with "Desktop development with C++" workload).
* **CMake:** Version 3.16 or higher.
* **Qt 6:** Version 6.7+ (`Widgets` and `Network` components, e.g., in `C:/Qt/6.7.2/msvc2019_64`).
* **Inno Setup 6:** Required to compile the official setup installer (`setup_script.iss`).

### 1.2. Configuration & Build via CMake
Open *x64 Native Tools Command Prompt for VS* or PowerShell:

```powershell
# 1. Configure the build directory
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
      -DCMAKE_BUILD_TYPE=Release `
      -DPRISM_QT_ROOT="C:/Qt/6.7.2/msvc2019_64" `
      -DBUILD_TESTING=ON

# 2. Build the main executable and helper targets in Release mode
cmake --build build --config Release --parallel
```

### 1.3. Running Unit Tests
```powershell
ctest --test-dir build -C Release --output-on-failure
```

### 1.4. Windows Package Generation

#### Standalone Portable Archive (`.zip`)
Bundle the compiled executable (`build/Release/`), Qt 6 runtime DLLs (`Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`, `Qt6Network.dll`), `app_icon.ico`, `portable-update-helper.exe`, and media engines (`ffmpeg.exe` and `yt-dlp.exe`) into a single ZIP archive.

#### Official Installer (`.exe`)
Compile the Inno Setup script:
```powershell
& "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe" setup_script.iss
```
The resulting executable is generated at `dist/PrismDownloader_vX.Y.Z_Setup.exe`.

---

## 🐧 2. Linux Environment (Mint 22 / Ubuntu 24.04 amd64)

### 2.1. System Package Requirements
Install required build tools, Qt 6, and FFmpeg via APT:

```bash
sudo apt update
sudo apt install build-essential cmake qt6-base-dev qt6-base-dev-tools \
                 ffmpeg dpkg-dev
```

### 2.2. Configuration & Build
```bash
# Configure project
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON

# Build in parallel
cmake --build build --parallel
```

### 2.3. Running Unit Tests
```bash
ctest --test-dir build --output-on-failure
```

### 2.4. Debian Package Generation (`.deb`)
Generate the native `.deb` package using **CPack**:

```bash
# Create Debian package in package/ directory
cpack --config build/CPackConfig.cmake -G DEB -B package

# Test local installation
sudo apt install ./package/prism-downloader_*.deb
```

### 2.5. Other Linux formats
The package format of a build is chosen with `-DPRISM_LINUX_PACKAGE=<id>` (it selects which release asset the in-app updater downloads):

| Id | Output | Notes |
| :--- | :--- | :--- |
| `deb-ubuntu24.04` (default) | `cpack -G DEB` | Ubuntu 24.04 / Mint 22 (historic asset name) |
| `deb-ubuntu22.04`, `deb-debian12` | `cpack -G DEB` | Build on that distribution; the CI renames the asset with a suffix |
| `rpm` | `cpack -G RPM` | Build on Fedora; needs `rpm-build`. Depends on `/usr/bin/ffmpeg` |
| `bundle` | AppImage + tar.gz | Built with Qt 6.7.2 and `linuxdeploy` + `linuxdeploy-plugin-qt` (pinned by SHA-256 in `ci.yml`); `cmake --install` into an AppDir first |

---

## 🧪 3. Automated Test Suite (`tests/`)

The repository integrates a comprehensive unit test suite managed through CTest:

| Test Suite | Implementation File | Verification Scope |
| :--- | :--- | :--- |
| `QueueManagerTests` | `tests/QueueManagerTests.cpp` | Validates download enqueuing, concurrency boundaries (1-5), cancellation, and queue state transitions. |
| `AppUpdateServiceTests` | `tests/AppUpdateServiceTests.cpp` | Verifies manifest parsing, required package assets, and SHA-256 stream hashing. |
| `YtDlpUpdateServiceTests` | `tests/YtDlpUpdateServiceTests.cpp` | Validates yt-dlp release parsing and checksum file validation. |
| `MediaToolResolverTests` | `tests/MediaToolResolverTests.cpp` | Tests priority candidate selection (User Update vs. Bundle vs. PATH) and semantic version comparisons. |
| `PortableUpdateCommonTests` | `tests/PortableUpdateCommonTests.cpp` | Validates extraction scripts and arguments for the portable helper. |
| `DownloadProfileTests` | `tests/DownloadProfileTests.cpp` | Validates format selector string generation for all resolution profiles. |

---

## 🌐 4. Continuous Integration & Releases (CI/CD)

Two GitHub Actions workflows exist:
* `.github/workflows/ci.yml` (manual: Actions > CI build > Run workflow) builds, tests and packages Linux/Windows without publishing anything.
* `.github/workflows/release.yml` can be run manually from GitHub Actions (Actions > Release > Run workflow, with optional `tag`, `draft` and `prerelease` inputs) or triggered by pushing a `v*` tag (e.g. `git tag v2.0.3 && git push origin v2.0.3`). The tag must equal `v<PRISM_VERSION>` in `version.iss` (when triggered manually with an empty tag, `v<PRISM_VERSION>` is used automatically). It reuses `ci.yml` and then publishes the release, using `changelog/<version>.md` as release notes when present.

The pipeline:
1. Builds the Linux packages: `.deb` for Ubuntu 24.04, Ubuntu 22.04 and Debian 12; `.rpm` in a Fedora container; AppImage and tar.gz (bundled Qt) on Ubuntu 22.04.
2. Runs the full `ctest` test suite.
3. Generates `prism-update-manifest.json` with the SHA-256 hashes of the release assets.
4. Publishes the manifest and package assets directly to GitHub Releases.
