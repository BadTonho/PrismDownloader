#include "AppUpdateService.h"
#include "Ed25519Verifier.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <openssl/evp.h>

#include <iostream>

namespace {

bool check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

QByteArray publicKey()
{
    return QByteArray::fromHex("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
}

QByteArray signatureForEmptyMessage()
{
    return QByteArray::fromHex("e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e06522490155"
                               "5fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b");
}

QByteArray signForTest(const QByteArray &message)
{
    const QByteArray privateKey = QByteArray::fromHex(
        "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60");
    EVP_PKEY *key = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr,
                                                   reinterpret_cast<const unsigned char *>(privateKey.constData()),
                                                   privateKey.size());
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    size_t signatureLength = 0;
    const bool initialized = key && context
        && EVP_DigestSignInit(context, nullptr, nullptr, nullptr, key) == 1
        && EVP_DigestSign(context, nullptr, &signatureLength,
                          reinterpret_cast<const unsigned char *>(message.constData()),
                          message.size()) == 1;
    QByteArray signature(initialized ? static_cast<int>(signatureLength) : 0, Qt::Uninitialized);
    const bool signedMessage = initialized
        && EVP_DigestSign(context, reinterpret_cast<unsigned char *>(signature.data()), &signatureLength,
                          reinterpret_cast<const unsigned char *>(message.constData()),
                          message.size()) == 1;
    if (!signedMessage) {
        signature.clear();
    } else {
        signature.resize(static_cast<int>(signatureLength));
    }
    EVP_MD_CTX_free(context);
    EVP_PKEY_free(key);
    return signature;
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
    bool success = check(Ed25519Verifier::verify({}, signatureForEmptyMessage(), publicKey()),
                         "RFC 8032 Ed25519 test vector verifies")
        && check(!Ed25519Verifier::verify("x", signatureForEmptyMessage(), publicKey()),
                 "signature for a different manifest is rejected");

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
    const AppUpdateReleaseInfo release = AppUpdateService::parseVerifiedRelease(
        releasePayload(), manifest, signForTest(manifest), publicKey(),
        AppUpdatePackageKind::WindowsPortable, &error);
    success = check(release.isValid() && release.version == QStringLiteral("2.0.1")
                        && release.assetName == QStringLiteral("PrismDownloader_v2.0.1_Portable.zip"),
                    "signed manifest selects the expected platform package") && success;

    const AppUpdateReleaseInfo incompleteRelease = AppUpdateService::parseVerifiedRelease(
        releasePayload(false), manifest, signForTest(manifest), publicKey(),
        AppUpdatePackageKind::WindowsSetup, &error);
    success = check(!incompleteRelease.isValid()
                        && error.contains(QStringLiteral("prism-downloader_2.0.1_amd64.deb")),
                    "release missing a required platform asset is rejected") && success;

    const QByteArray malformedManifest("{");
    const AppUpdateReleaseInfo malformedRelease = AppUpdateService::parseVerifiedRelease(
        releasePayload(), malformedManifest, signForTest(malformedManifest), publicKey(),
        AppUpdatePackageKind::LinuxDeb, &error);
    success = check(!malformedRelease.isValid() && error.contains(QStringLiteral("JSON")),
                    "malformed signed manifest is rejected") && success;

    return success ? 0 : 1;
}
