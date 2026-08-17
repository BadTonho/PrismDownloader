# Release signing for automatic updates

Every Prism release is accepted by the application only when all three platform
packages are listed in `prism-update-manifest.json` and that exact file has a
valid detached Ed25519 signature in `prism-update-manifest.sig`.

## One-time setup

On a trusted Windows machine with OpenSSL and GitHub CLI authenticated for the
repository owner, run:

```powershell
.\scripts\generate-update-signing-key.ps1 -Repository BadTonho/PrismDownloader
```

The script creates an Ed25519 key only in a temporary directory, streams its
Base64 PEM to the GitHub Secret `PRISM_UPDATE_ED25519_PRIVATE_KEY`, then removes
the temporary directory. Do not print, commit, copy to repository variables, or
reuse that private key outside GitHub Secrets.

The release workflow derives the public key from that secret before compiling
Windows and Linux packages. Consequently each published application embeds the
matching public key without exposing signing material.

## Release contract

The tag must be `vX.Y.Z` and match `version.iss`. The workflow refuses to
publish unless it has all of these exact assets:

- `PrismDownloader_vX.Y.Z_Setup.exe`
- `PrismDownloader_vX.Y.Z_Portable.zip`
- `prism-downloader_X.Y.Z_amd64.deb`

The existing `2.0.0` DEB is not modified. The first release using this workflow
is the bootstrap for automatic self-updates.
