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
- `prism-downloader_X.Y.Z_amd64.deb`
- `prism-update-manifest.json`

The manifest is generated with `scripts/create_update_manifest.py`:

```bash
python scripts/create_update_manifest.py --version 2.0.1 --output dist/prism-update-manifest.json \
  --asset PrismDownloader_v2.0.1_Setup.exe=dist/PrismDownloader_v2.0.1_Setup.exe \
  --asset PrismDownloader_v2.0.1_Portable.zip=dist/PrismDownloader_v2.0.1_Portable.zip \
  --asset prism-downloader_2.0.1_amd64.deb=dist/prism-downloader_2.0.1_amd64.deb
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

The application downloads and validates `prism-downloader_X.Y.Z_amd64.deb`,
then requests installation through the system package tool.

## Independent `yt-dlp` updates

The `yt-dlp` updater continues to use the official `SHA2-256SUMS` file to
validate the downloaded binary. This is independent of the application release
manifest.
