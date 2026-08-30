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

    const QByteArray manifest = validManifest();
    QString error;
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
