#include "YtDlpUpdateService.h"

#include <QCoreApplication>

#include <iostream>

namespace {

bool check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

QByteArray releasePayload(const QString &binaryName)
{
    return QStringLiteral(R"({
        "tag_name": "2026.08.17.073947",
        "assets": [
            {"name": "%1", "browser_download_url": "https://github.com/yt-dlp/yt-dlp-nightly-builds/releases/download/2026.08.17.073947/%1"},
            {"name": "SHA2-256SUMS", "browser_download_url": "https://github.com/yt-dlp/yt-dlp-nightly-builds/releases/download/2026.08.17.073947/SHA2-256SUMS"}
        ]
    })").arg(binaryName).toUtf8();
}

}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    const QString assetName = YtDlpUpdateService::platformAssetName();

    QString error;
    const YtDlpReleaseInfo release = YtDlpUpdateService::parseLatestRelease(releasePayload(assetName), &error);
    bool success = check(release.isValid(), "valid official nightly release is parsed")
        && check(release.version == QStringLiteral("2026.08.17.073947"), "nightly version is retained")
        && check(release.binaryUrl.host() == QStringLiteral("github.com"), "binary URL remains official")
        && check(release.checksumsUrl.fileName() == QStringLiteral("SHA2-256SUMS"), "checksum asset is selected");

    const QByteArray checksumList(
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa  yt-dlp_linux\n"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb *yt-dlp.exe\n");
    success = check(YtDlpUpdateService::checksumForAsset(checksumList, QStringLiteral("yt-dlp_linux"))
                        == QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
                    "linux checksum is extracted")
        && check(YtDlpUpdateService::checksumForAsset(checksumList, QStringLiteral("yt-dlp.exe"))
                        == QStringLiteral("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"),
                    "windows checksum is extracted")
        && check(YtDlpUpdateService::checksumForAsset(checksumList, QStringLiteral("missing")).isEmpty(),
                 "missing checksum is rejected")
        && success;

    const QByteArray binary("prism test binary");
    const QString matchingChecksum(QStringLiteral("fb07bb87cf334d154214116f26f377909d6478d0d7b81484eb63c466251e2378"));
    success = check(YtDlpUpdateService::hasValidChecksum(binary, matchingChecksum),
                    "matching SHA-256 is accepted")
        && check(!YtDlpUpdateService::hasValidChecksum(binary, QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")),
                 "wrong SHA-256 is rejected")
        && check(!YtDlpUpdateService::hasValidChecksum(binary, QStringLiteral("invalid")),
                 "malformed SHA-256 is rejected")
        && success;

    error.clear();
    const YtDlpReleaseInfo invalidHost = YtDlpUpdateService::parseLatestRelease(
        QByteArray(R"({"tag_name":"2026.08.17","assets":[{"name":"yt-dlp_linux","browser_download_url":"https://example.test/yt-dlp_linux"}]})"),
        &error);
    success = check(!invalidHost.isValid() && !error.isEmpty(),
                    "non-official or incomplete release is rejected")
        && success;

    return success ? 0 : 1;
}
