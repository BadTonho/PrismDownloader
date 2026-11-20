#include "AppUpdateService.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <iostream>

namespace {

bool check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

QJsonObject asset(const QString &name)
{
    QJsonObject object;
    object.insert(QStringLiteral("name"), name);
    object.insert(QStringLiteral("browser_download_url"),
                  QStringLiteral("https://github.com/BadTonho/PrismDownloader/releases/download/v2.0.1/") + name);
    return object;
}

QByteArray releasePayload(bool includeLinuxAsset = true)
{
    QJsonArray assets;
    assets.append(asset(QStringLiteral("PrismDownloader_v2.0.1_Setup.exe")));
    assets.append(asset(QStringLiteral("PrismDownloader_v2.0.1_Portable.zip")));
    if (includeLinuxAsset) {
        assets.append(asset(QStringLiteral("prism-downloader_2.0.1_amd64.deb")));
    }
    QJsonObject release;
    release.insert(QStringLiteral("tag_name"), QStringLiteral("v2.0.1"));
    release.insert(QStringLiteral("assets"), assets);
    return QJsonDocument(release).toJson(QJsonDocument::Compact);
}

QByteArray validManifest()
{
    const auto manifestAsset = [](const QString &name, const QLatin1Char hashCharacter) {
        QJsonObject object;
        object.insert(QStringLiteral("name"), name);
        object.insert(QStringLiteral("sha256"), QString(64, hashCharacter));
        return object;
    };
    QJsonArray assets;
    assets.append(manifestAsset(QStringLiteral("PrismDownloader_v2.0.1_Setup.exe"), QLatin1Char('a')));
    assets.append(manifestAsset(QStringLiteral("PrismDownloader_v2.0.1_Portable.zip"), QLatin1Char('b')));
    assets.append(manifestAsset(QStringLiteral("prism-downloader_2.0.1_amd64.deb"), QLatin1Char('c')));
    QJsonObject manifest;
    manifest.insert(QStringLiteral("schema"), 1);
    manifest.insert(QStringLiteral("version"), QStringLiteral("2.0.1"));
    manifest.insert(QStringLiteral("assets"), assets);
    return QJsonDocument(manifest).toJson(QJsonDocument::Compact);
}

}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    bool success = true;

    const QByteArray binary("Prism update package");
    const QString hash = QString::fromLatin1(
        QCryptographicHash::hash(binary, QCryptographicHash::Sha256).toHex());
    success = check(AppUpdateService::hasValidChecksum(binary, hash), "matching SHA-256 is accepted")
        && check(!AppUpdateService::hasValidChecksum(binary, QString(64, QLatin1Char('0'))),
                 "wrong SHA-256 is rejected") && success;
    success = check(AppUpdateService::expectedAssetName(QStringLiteral("2.0.1"),
                                                          AppUpdatePackageKind::WindowsSetup)
                        == QStringLiteral("PrismDownloader_v2.0.1_Setup.exe"),
                    "Windows setup asset name is stable")
        && check(AppUpdateService::expectedAssetName(QStringLiteral("2.0.1"),
                                                     AppUpdatePackageKind::WindowsPortable)
                        == QStringLiteral("PrismDownloader_v2.0.1_Portable.zip"),
                 "Windows portable asset name is stable")
        && check(AppUpdateService::expectedAssetName(QStringLiteral("2.0.1"),
                                                     AppUpdatePackageKind::LinuxDeb)
                        == QStringLiteral("prism-downloader_2.0.1_amd64.deb"),
                 "Linux package asset name is stable") && success;

    success = check(AppUpdateService::expectedAssetName(QStringLiteral("2.0.1"),
                                                         AppUpdatePackageKind::LinuxDebUbuntu2204)
                        == QStringLiteral("prism-downloader_2.0.1_ubuntu22.04_amd64.deb"),
                    "Ubuntu 22.04 deb name")
        && check(AppUpdateService::expectedAssetName(QStringLiteral("2.0.1"),
                                                     AppUpdatePackageKind::LinuxDebDebian12)
                        == QStringLiteral("prism-downloader_2.0.1_debian12_amd64.deb"),
                 "Debian 12 deb name")
        && check(AppUpdateService::expectedAssetName(QStringLiteral("2.0.1"),
                                                     AppUpdatePackageKind::LinuxRpm)
                        == QStringLiteral("prism-downloader_2.0.1_x86_64.rpm"),
                 "RPM name")
        && check(AppUpdateService::expectedAssetName(QStringLiteral("2.0.1"),
                                                     AppUpdatePackageKind::LinuxAppImage)
                        == QStringLiteral("PrismDownloader_v2.0.1_x86_64.AppImage"),
                 "AppImage name")
        && check(AppUpdateService::expectedAssetName(QStringLiteral("2.0.1"),
                                                     AppUpdatePackageKind::LinuxTarball)
                        == QStringLiteral("PrismDownloader_v2.0.1_linux_x86_64.tar.gz"),
                 "tarball name") && success;

    success = check(AppUpdateService::packageForLinuxId(QStringLiteral("deb-ubuntu24.04"), false)
                        == AppUpdatePackageKind::LinuxDeb, "default id maps to the legacy deb")
        && check(AppUpdateService::packageForLinuxId(QStringLiteral("deb-ubuntu22.04"), false)
                     == AppUpdatePackageKind::LinuxDebUbuntu2204, "ubuntu 22.04 id")
        && check(AppUpdateService::packageForLinuxId(QStringLiteral("deb-debian12"), false)
                     == AppUpdatePackageKind::LinuxDebDebian12, "debian 12 id")
        && check(AppUpdateService::packageForLinuxId(QStringLiteral("rpm"), false)
                     == AppUpdatePackageKind::LinuxRpm, "rpm id")
        && check(AppUpdateService::packageForLinuxId(QStringLiteral("bundle"), true)
                     == AppUpdatePackageKind::LinuxAppImage, "bundle under AppImage")
        && check(AppUpdateService::packageForLinuxId(QStringLiteral("bundle"), false)
                     == AppUpdatePackageKind::LinuxTarball, "bundle outside AppImage") && success;

    const QByteArray manifest = validManifest();
    QString error;

    // Formatos além dos três históricos só são exigidos quando são o do cliente.
    const AppUpdateReleaseInfo missingRpm = AppUpdateService::parseRelease(
        releasePayload(), manifest, AppUpdatePackageKind::LinuxRpm, &error);
    success = check(!missingRpm.isValid() && error.contains(QStringLiteral("x86_64.rpm")),
                    "release missing the client's own optional format is rejected") && success;
    const AppUpdateReleaseInfo withoutRpmForDeb = AppUpdateService::parseRelease(
        releasePayload(), manifest, AppUpdatePackageKind::LinuxDeb, &error);
    success = check(withoutRpmForDeb.isValid(),
                    "missing optional formats do not block other clients") && success;
    const AppUpdateReleaseInfo release = AppUpdateService::parseRelease(
        releasePayload(), manifest,
        AppUpdatePackageKind::WindowsPortable, &error);
    success = check(release.isValid() && release.version == QStringLiteral("2.0.1")
                        && release.assetName == QStringLiteral("PrismDownloader_v2.0.1_Portable.zip"),
                    "manifest selects the expected platform package") && success;

    const AppUpdateReleaseInfo incompleteRelease = AppUpdateService::parseRelease(
        releasePayload(false), manifest,
        AppUpdatePackageKind::WindowsSetup, &error);
    success = check(!incompleteRelease.isValid()
                        && error.contains(QStringLiteral("prism-downloader_2.0.1_amd64.deb")),
                    "release missing a required platform asset is rejected") && success;

    const QByteArray malformedManifest("{");
    const AppUpdateReleaseInfo malformedRelease = AppUpdateService::parseRelease(
        releasePayload(), malformedManifest,
        AppUpdatePackageKind::LinuxDeb, &error);
    success = check(!malformedRelease.isValid() && error.contains(QStringLiteral("JSON")),
                    "malformed manifest is rejected") && success;

    return success ? 0 : 1;
}
