# 🛡️ Security Policy — Prism Downloader

**Prism Downloader** is a personal open-source project maintained by a single developer (**Tonho Studios / BadTonho**).
Security is taken seriously. This document describes which versions receive security fixes and how to responsibly report vulnerabilities.

---

## 📌 Supported Versions

Only the latest release branch receives active security patches.
Older versions are no longer maintained and will not receive fixes.

| Version | Supported | Notes |
| :--- | :---: | :--- |
| **v1.1.x** (Latest) | ✅ Yes | Actively maintained. Security patches are released as hotfixes. |
| **v1.0.x and earlier** | ❌ No | End-of-life. These were released under the legacy "NeoVDownloader" name. Upgrade to v1.1.x. |

---

## 🔒 Reporting a Vulnerability

If you discover a security bug — such as argument injection in the download engine, unsafe subprocess handling, path traversal, or any other exploitable behavior — please **do not open a public GitHub Issue immediately**, as that could expose users before a fix is available.

### Preferred method: GitHub Private Advisory

1. Go to the [**Security tab**](https://github.com/BadTonho/PrismDownloader/security/advisories/new) of the repository.
2. Click **"Report a vulnerability"** to open a private advisory draft.
3. Describe the issue clearly: what the vulnerability is, how to reproduce it, and what the potential impact is.
4. I will acknowledge your report and respond as soon as possible.

> [!NOTE]
> This is a solo indie project maintained in spare time. There are no automated response SLAs or a security team. I will do my best to triage and patch confirmed vulnerabilities promptly, but response time may vary depending on availability.

---

## 🧩 Scope — What Is in Scope

The following areas are relevant to the security of Prism Downloader:

- **Argument injection** — inputs passed to `yt-dlp.exe` or `ffmpeg.exe` via `QProcess`
- **Path traversal** — output directory or filename handling that could escape intended paths
- **Unsafe subprocess creation** — privilege escalation or unintended code execution via child processes
- **Update mechanism** — the GitHub-based version check and engine updater (`yt-dlp` auto-update)
- **Bundled binaries** — shipping a compromised or outdated version of `yt-dlp.exe` or `ffmpeg.exe`

---

## 🚫 Out of Scope

The following are **not** considered security vulnerabilities for this project:

- Bugs that only affect the UI or cosmetic appearance
- YouTube, Twitch, or other platform-side changes that break downloads (these are upstream `yt-dlp` issues)
- Issues that require the attacker to already have physical or administrative access to the machine
- Theoretical risks with no practical proof-of-concept

---

## ⚙️ Technical Context

Understanding the architecture helps scope valid reports:

- **Language / Framework:** C++17 with Qt 6 (Widgets + Network)
- **Platform:** Windows 10 / 11 (64-bit) only
- **External engines:** [`yt-dlp`](https://github.com/yt-dlp/yt-dlp) and [`FFmpeg`](https://ffmpeg.org/) are bundled as pre-compiled `.exe` binaries
- **Process isolation:** Child processes are spawned via `QProcess` with `CREATE_NO_WINDOW` (`0x08000000`) and monitored via exit codes
- **Network activity:** Limited to the GitHub Releases API for update checks (no telemetry or user data is collected or transmitted)
- **License:** [MIT](LICENSE) — Copyright © 2026 Tonho Studios (BadTonho)

---

## ✅ Disclosure Policy

Once a fix is ready and released:

1. A new version will be published on the [Releases page](https://github.com/BadTonho/PrismDownloader/releases).
2. The private advisory will be made public after users have had reasonable time to update.
3. Credit will be given to the reporter in the release notes, unless anonymity is requested.

---

*Thank you for helping keep Prism Downloader safe for all Windows users.*
