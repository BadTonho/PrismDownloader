#include "AppUpdateService.h"

#include "Ed25519Verifier.h"
#include "PrismUpdateKey.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryFile>

namespace {

const QUrl kLatestReleaseApi(
    QStringLiteral("https://api.github.com/repos/BadTonho/PrismDownloader/releases/latest"));
constexpr qint64 kMaximumManifestBytes = 64 * 1024;
constexpr qint64 kMaximumPackageBytes = 1024LL * 1024 * 1024;

bool isOfficialGitHubUrl(const QUrl &url)
{
    const QString host = url.host().toLower();
    return url.scheme() == QStringLiteral("https")
        && (host == QStringLiteral("github.com") || host == QStringLiteral("api.github.com"));
}

QNetworkRequest githubRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("PrismDownloader-AppUpdater"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    return request;
}

QString normalizedVersion(const QString &tag)
{
    static const QRegularExpression versionPattern(QStringLiteral("^v?([0-9]+\\.[0-9]+\\.[0-9]+)$"));
    const QRegularExpressionMatch match = versionPattern.match(tag.trimmed());
    return match.hasMatch() ? match.captured(1) : QString();
}

bool validSha256(const QString &value)
{
    static const QRegularExpression hashPattern(QStringLiteral("^[A-Fa-f0-9]{64}$"));
    return hashPattern.match(value).hasMatch();
}

QByteArray configuredPublicKey()
{
    const QByteArray key = QByteArray::fromBase64(QByteArray(PRISM_UPDATE_PUBLIC_KEY_BASE64));
    return key.size() == 32 ? key : QByteArray();
}

QString failMessage(const QString &message)
{
    return QStringLiteral("Release recusada: ") + message;
}

QUrl namedOfficialAssetUrl(const QJsonArray &assets, const QString &name, QString *error)
{
    QUrl result;
    for (const QJsonValue &value : assets) {
        const QJsonObject asset = value.toObject();
        if (asset.value(QStringLiteral("name")).toString() != name) {
            continue;
        }
        const QUrl url(asset.value(QStringLiteral("browser_download_url")).toString());
        if (!isOfficialGitHubUrl(url) || result.isValid()) {
            if (error) {
                *error = QStringLiteral("asset oficial %1 ausente ou duplicado.").arg(name);
            }
            return {};
        }
        result = url;
    }
    if (!result.isValid() && error) {
        *error = QStringLiteral("asset oficial %1 ausente.").arg(name);
    }
    return result;
}

}

AppUpdateService::AppUpdateService(AppUpdatePackageKind packageKind, QObject *parent)
    : QObject(parent),
      m_networkManager(new QNetworkAccessManager(this)),
      m_packageKind(packageKind)
{
}

AppUpdatePackageKind AppUpdateService::packageForCurrentPlatform(bool installedWindows)
{
#ifdef Q_OS_WIN
    return installedWindows ? AppUpdatePackageKind::WindowsSetup
                            : AppUpdatePackageKind::WindowsPortable;
#else
    Q_UNUSED(installedWindows)
    return AppUpdatePackageKind::LinuxDeb;
#endif
}

QString AppUpdateService::expectedAssetName(const QString &version, AppUpdatePackageKind packageKind)
{
    switch (packageKind) {
    case AppUpdatePackageKind::WindowsSetup:
        return QStringLiteral("PrismDownloader_v%1_Setup.exe").arg(version);
    case AppUpdatePackageKind::WindowsPortable:
        return QStringLiteral("PrismDownloader_v%1_Portable.zip").arg(version);
    case AppUpdatePackageKind::LinuxDeb:
        return QStringLiteral("prism-downloader_%1_amd64.deb").arg(version);
    }
    return {};
}

bool AppUpdateService::hasConfiguredPublicKey()
{
    return configuredPublicKey().size() == 32;
}

bool AppUpdateService::hasValidChecksum(const QByteArray &binary, const QString &expectedSha256)
{
    if (!validSha256(expectedSha256)) {
        return false;
    }
    const QByteArray actual = QCryptographicHash::hash(binary, QCryptographicHash::Sha256).toHex();
    return actual.compare(expectedSha256.toLatin1(), Qt::CaseInsensitive) == 0;
}

AppUpdateReleaseInfo AppUpdateService::parseVerifiedRelease(const QByteArray &releasePayload,
                                                            const QByteArray &manifest,
                                                            const QByteArray &signature,
                                                            const QByteArray &publicKey,
                                                            AppUpdatePackageKind packageKind,
                                                            QString *errorMessage)
{
    const auto fail = [errorMessage](const QString &message) {
        if (errorMessage) {
            *errorMessage = message;
        }
        return AppUpdateReleaseInfo{};
    };
    if (manifest.isEmpty() || manifest.size() > kMaximumManifestBytes || signature.size() != 64
        || publicKey.size() != 32 || !Ed25519Verifier::verify(manifest, signature, publicKey)) {
        return fail(QStringLiteral("manifesto ou assinatura Ed25519 inválidos."));
    }

    QJsonParseError releaseError;
    const QJsonDocument releaseDocument = QJsonDocument::fromJson(releasePayload, &releaseError);
    if (releaseError.error != QJsonParseError::NoError || !releaseDocument.isObject()) {
        return fail(QStringLiteral("resposta de release inválida."));
    }
    const QJsonObject release = releaseDocument.object();
    const QString releaseVersion = normalizedVersion(release.value(QStringLiteral("tag_name")).toString());
    if (releaseVersion.isEmpty()) {
        return fail(QStringLiteral("tag de release inválida."));
    }

    QJsonParseError manifestError;
    const QJsonDocument manifestDocument = QJsonDocument::fromJson(manifest, &manifestError);
    if (manifestError.error != QJsonParseError::NoError || !manifestDocument.isObject()) {
        return fail(QStringLiteral("manifesto assinado não contém JSON válido."));
    }
    const QJsonObject manifestObject = manifestDocument.object();
    if (manifestObject.value(QStringLiteral("schema")).toInt(-1) != 1
        || manifestObject.value(QStringLiteral("version")).toString() != releaseVersion) {
        return fail(QStringLiteral("manifesto não corresponde à tag da release."));
    }

    const QJsonArray releaseAssets = release.value(QStringLiteral("assets")).toArray();
    const QJsonArray manifestAssets = manifestObject.value(QStringLiteral("assets")).toArray();
    QString selectedHash;
    QUrl selectedUrl;
    const QList<AppUpdatePackageKind> requiredPackages{
        AppUpdatePackageKind::WindowsSetup,
        AppUpdatePackageKind::WindowsPortable,
        AppUpdatePackageKind::LinuxDeb};
    for (AppUpdatePackageKind requiredPackage : requiredPackages) {
        const QString requiredName = expectedAssetName(releaseVersion, requiredPackage);
        QString assetError;
        const QUrl releaseUrl = namedOfficialAssetUrl(releaseAssets, requiredName, &assetError);
        if (!releaseUrl.isValid()) {
            return fail(assetError);
        }
        QString hash;
        for (const QJsonValue &value : manifestAssets) {
            const QJsonObject asset = value.toObject();
            if (asset.value(QStringLiteral("name")).toString() != requiredName) {
                continue;
            }
            if (!hash.isEmpty()) {
                return fail(QStringLiteral("manifesto contém asset duplicado."));
            }
            hash = asset.value(QStringLiteral("sha256")).toString().toLower();
        }
        if (!validSha256(hash)) {
            return fail(QStringLiteral("manifesto não contém SHA-256 válido para %1.").arg(requiredName));
        }
        if (requiredPackage == packageKind) {
            selectedHash = hash;
            selectedUrl = releaseUrl;
        }
    }
    return {releaseVersion, expectedAssetName(releaseVersion, packageKind), selectedHash, selectedUrl};
}

bool AppUpdateService::hasLatestRelease() const
{
    return m_latestRelease.isValid();
}

const AppUpdateReleaseInfo &AppUpdateService::latestRelease() const
{
    return m_latestRelease;
}

bool AppUpdateService::isBusy() const
{
    return m_checking || m_downloading;
}

void AppUpdateService::checkLatestRelease()
{
    if (isBusy()) {
        return;
    }
    if (!hasConfiguredPublicKey()) {
        emit checkFailed(QStringLiteral("esta compilação não contém a chave pública de atualização."));
        return;
    }

    m_checking = true;
    m_latestRelease = {};
    QNetworkReply *reply = m_networkManager->get(githubRequest(kLatestReleaseApi));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_checking = false;
            const int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            emit checkFailed(httpCode == 404
                ? QStringLiteral("nenhuma release pública foi publicada.")
                : QStringLiteral("não foi possível consultar o GitHub: ") + reply->errorString());
            return;
        }
        m_releasePayload = reply->readAll();
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(m_releasePayload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            m_checking = false;
            emit checkFailed(failMessage(QStringLiteral("resposta da API inválida.")));
            return;
        }
        const QJsonArray assets = document.object().value(QStringLiteral("assets")).toArray();
        QString error;
        m_manifestUrl = namedOfficialAssetUrl(assets, QStringLiteral("prism-update-manifest.json"), &error);
        m_signatureUrl = namedOfficialAssetUrl(assets, QStringLiteral("prism-update-manifest.sig"), &error);
        if (!m_manifestUrl.isValid() || !m_signatureUrl.isValid()) {
            m_checking = false;
            emit checkFailed(failMessage(error));
            return;
        }
        downloadManifest();
    });
}

void AppUpdateService::downloadManifest()
{
    QNetworkReply *reply = m_networkManager->get(githubRequest(m_manifestUrl));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_checking = false;
            emit checkFailed(failMessage(QStringLiteral("não foi possível baixar o manifesto: ")
                                         + reply->errorString()));
            return;
        }
        m_manifest = reply->readAll();
        if (m_manifest.isEmpty() || m_manifest.size() > kMaximumManifestBytes) {
            m_checking = false;
            emit checkFailed(failMessage(QStringLiteral("manifesto com tamanho inválido.")));
            return;
        }
        downloadSignature();
    });
}

void AppUpdateService::downloadSignature()
{
    QNetworkReply *reply = m_networkManager->get(githubRequest(m_signatureUrl));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_checking = false;
            emit checkFailed(failMessage(QStringLiteral("não foi possível baixar a assinatura: ")
                                         + reply->errorString()));
            return;
        }
        const QByteArray signature = reply->readAll();
        QString error;
        m_latestRelease = parseVerifiedRelease(m_releasePayload, m_manifest, signature,
                                               configuredPublicKey(), m_packageKind, &error);
        m_checking = false;
        if (!m_latestRelease.isValid()) {
            emit checkFailed(failMessage(error));
            return;
        }
        emit releaseChecked(m_latestRelease);
    });
}

void AppUpdateService::downloadLatestRelease()
{
    if (m_downloading) {
        return;
    }
    if (!m_latestRelease.isValid()) {
        emit updateFailed(QStringLiteral("verifique uma release assinada antes de baixar a atualização."));
        return;
    }

    QString temporaryDirectory = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (temporaryDirectory.isEmpty()) {
        temporaryDirectory = QDir::tempPath();
    }
    temporaryDirectory = QDir(temporaryDirectory).filePath(QStringLiteral("PrismDownloader/updates"));
    if (!QDir().mkpath(temporaryDirectory)) {
        emit updateFailed(QStringLiteral("não foi possível preparar a pasta temporária da atualização."));
        return;
    }
    const QString extension = QFileInfo(m_latestRelease.assetName).suffix();
    m_downloadFile = std::make_unique<QTemporaryFile>(QDir(temporaryDirectory).filePath(
        QStringLiteral(".prism-update-XXXXXX.%1").arg(extension)));
    m_downloadFile->setAutoRemove(false);
    if (!m_downloadFile->open()) {
        m_downloadFile.reset();
        emit updateFailed(QStringLiteral("não foi possível criar o arquivo temporário da atualização."));
        return;
    }

    m_downloadHasher.reset();
    m_downloadedBytes = 0;
    m_downloadWriteFailed = false;
    m_downloading = true;
    QNetworkReply *reply = m_networkManager->get(githubRequest(m_latestRelease.downloadUrl));
    connect(reply, &QNetworkReply::downloadProgress, this, &AppUpdateService::downloadProgress);
    connect(reply, &QIODevice::readyRead, this, [this, reply]() {
        const QByteArray bytes = reply->readAll();
        if (bytes.isEmpty() || !m_downloadFile) {
            return;
        }
        m_downloadedBytes += bytes.size();
        if (m_downloadedBytes > kMaximumPackageBytes
            || m_downloadFile->write(bytes) != bytes.size()) {
            m_downloadWriteFailed = true;
            reply->abort();
            return;
        }
        m_downloadHasher.addData(bytes);
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray remaining = reply->readAll();
        if (!remaining.isEmpty() && m_downloadFile && !m_downloadWriteFailed) {
            m_downloadedBytes += remaining.size();
            if (m_downloadedBytes > kMaximumPackageBytes
                || m_downloadFile->write(remaining) != remaining.size()) {
                m_downloadWriteFailed = true;
            } else {
                m_downloadHasher.addData(remaining);
            }
        }
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError || m_downloadWriteFailed || !m_downloadFile) {
            finishDownloadFailure(m_downloadWriteFailed
                ? QStringLiteral("não foi possível gravar o pacote verificado.")
                : QStringLiteral("não foi possível baixar o pacote: ") + reply->errorString());
            return;
        }
        if (!m_downloadFile->flush()) {
            finishDownloadFailure(QStringLiteral("não foi possível finalizar o pacote temporário."));
            return;
        }
        m_downloadFile->close();
        const QByteArray actual = m_downloadHasher.result().toHex();
        if (actual.compare(m_latestRelease.sha256.toLatin1(), Qt::CaseInsensitive) != 0) {
            finishDownloadFailure(QStringLiteral("SHA-256 inválido; a versão atual foi preservada."));
            return;
        }
        const QString packagePath = m_downloadFile->fileName();
        m_downloadFile.reset();
        m_downloading = false;
        emit packageVerified(m_latestRelease.version, packagePath);
    });
}

void AppUpdateService::finishDownloadFailure(const QString &message)
{
    if (m_downloadFile) {
        const QString path = m_downloadFile->fileName();
        m_downloadFile->close();
        m_downloadFile.reset();
        QFile::remove(path);
    }
    m_downloading = false;
    emit updateFailed(message);
}
