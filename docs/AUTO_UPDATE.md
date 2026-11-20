# Automatic Update System — Prism Downloader

This document describes the Prism Downloader application and `yt-dlp` update
flows.

## Application update validation

Each release publishes a `prism-update-manifest.json` containing the release
version and the SHA-256 hashes of the Setup, Portable, and DEB packages. The
application downloads that manifest, checks that it matches the GitHub release
tag, and validates the downloaded package against its declared SHA-256 hash
before installation.

A release needs these assets:

- `PrismDownloader_vX.Y.Z_Setup.exe`
- `PrismDownloader_vX.Y.Z_Portable.zip`
- `prism-downloader_X.Y.Z_amd64.deb` (Ubuntu 24.04 / Mint 22)
- `prism-downloader_X.Y.Z_ubuntu22.04_amd64.deb`
- `prism-downloader_X.Y.Z_debian12_amd64.deb`
- `prism-downloader_X.Y.Z_x86_64.rpm`
- `PrismDownloader_vX.Y.Z_x86_64.AppImage`
- `PrismDownloader_vX.Y.Z_linux_x86_64.tar.gz`
- `prism-update-manifest.json`

The manifest is generated with `scripts/create_update_manifest.py`:

```bash
python scripts/create_update_manifest.py --version 2.0.1 --output dist/prism-update-manifest.json \
  --asset PrismDownloader_v2.0.1_Setup.exe=dist/PrismDownloader_v2.0.1_Setup.exe \
  --asset PrismDownloader_v2.0.1_Portable.zip=dist/PrismDownloader_v2.0.1_Portable.zip \
  --asset prism-downloader_2.0.1_amd64.deb=dist/prism-downloader_2.0.1_amd64.deb
  # optional: --asset for the ubuntu22.04/debian12 .deb, .rpm, .AppImage and .tar.gz
```

The manifest is an integrity check, not an authenticity guarantee. Use the
official GitHub release page and review the release assets before installing.

## Update flow by package type

### Windows installed with Inno Setup

The application downloads `PrismDownloader_vX.Y.Z_Setup.exe`, validates its
SHA-256 hash, runs the installer, and restarts.

### Windows portable

The application downloads and validates the Portable ZIP, then runs
`portable-update-helper.exe` to stage and replace the application after the
current process exits. The helper keeps the existing rollback behavior.

### Linux

Each build knows its own format (`PRISM_LINUX_PACKAGE` at build time; the
`APPIMAGE` environment variable distinguishes the AppImage from the tar.gz) and
downloads the matching asset. Older clients only require the Setup, Portable
and `_amd64.deb` assets, so those names never change; the other formats are
only required by clients that use them.

| Format | How it updates |
| :--- | :--- |
| DEB (Ubuntu 24.04/22.04, Debian 12) | `pkexec` runs a script as root that copies the package to a private directory, re-checks its SHA-256 and runs `apt-get install` on that copy. |
| RPM | Same root-side copy + SHA-256 check, then `dnf install` (or `zypper`, or `rpm -U`). |
| AppImage | The verified file replaces the running `.AppImage` atomically (the folder must be writable); the app restarts. |
| tar.gz | The archive is extracted next to the install folder, which is swapped with rollback; user files not in the new package are kept. Needs a writable install folder. |

When a format cannot update itself (for example, no write permission), the
update is cancelled with an explanation and the current version is kept.

## Independent `yt-dlp` updates

The `yt-dlp` updater continues to use the official `SHA2-256SUMS` file to
validate the downloaded binary. This is independent of the application release
manifest.
