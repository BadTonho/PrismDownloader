#include "YtDlpUpdateService.h"

#include "MediaToolResolver.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryFile>

namespace {

const QUrl kNightlyReleaseApi(
    QStringLiteral("https://api.github.com/repos/yt-dlp/yt-dlp-nightly-builds/releases/latest"));

bool isOfficialGitHubUrl(const QUrl &url)
{
    const QString host = url.host().toLower();
    return url.scheme() == QStringLiteral("https")
        && (host == QStringLiteral("github.com") || host.endsWith(QStringLiteral(".github.com")));
}

QNetworkRequest githubRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("PrismDownloader-yt-dlp-Updater"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    return request;
}

QString releaseError(const QString &message)
{
    return QStringLiteral("Não foi possível verificar a versão Nightly do yt-dlp: ") + message;
}

}

YtDlpUpdateService::YtDlpUpdateService(QObject *parent)
    : QObject(parent),
      m_networkManager(new QNetworkAccessManager(this))
{
}

QString YtDlpUpdateService::platformAssetName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("yt-dlp.exe");
#else
    return QStringLiteral("yt-dlp_linux");
#endif
}

YtDlpReleaseInfo YtDlpUpdateService::parseLatestRelease(const QByteArray &payload, QString *errorMessage)
{
    const auto fail = [errorMessage](const QString &message) {
        if (errorMessage) {
            *errorMessage = message;
        }
        return YtDlpReleaseInfo{};
    };

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return fail(QStringLiteral("resposta inválida do GitHub."));
    }

    const QJsonObject object = document.object();
    const QString version = object.value(QStringLiteral("tag_name")).toString().trimmed();
    if (!MediaToolResolver::isVersionNewer(version, QStringLiteral("0.0"))) {
        return fail(QStringLiteral("a tag Nightly não contém uma versão válida."));
    }

    QUrl binaryUrl;
    QUrl checksumsUrl;
    const QJsonArray assets = object.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue &value : assets) {
        const QJsonObject asset = value.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        const QUrl url(asset.value(QStringLiteral("browser_download_url")).toString());
        if (!isOfficialGitHubUrl(url)) {
            continue;
        }
        if (name == platformAssetName()) {
            binaryUrl = url;
        } else if (name == QStringLiteral("SHA2-256SUMS")) {
            checksumsUrl = url;
        }
    }
    if (!binaryUrl.isValid() || !checksumsUrl.isValid()) {
        return fail(QStringLiteral("a release não possui os arquivos oficiais esperados."));
    }
    return {version, binaryUrl, checksumsUrl};
}

QString YtDlpUpdateService::checksumForAsset(const QByteArray &payload, const QString &assetName)
{
    const QString escapedAsset = QRegularExpression::escape(assetName);
    const QRegularExpression expression(
        QStringLiteral("^([A-Fa-f0-9]{64})\\s+\\*?%1\\s*$").arg(escapedAsset),
        QRegularExpression::MultilineOption);
    const QRegularExpressionMatch match = expression.match(QString::fromLatin1(payload));
    return match.hasMatch() ? match.captured(1).toLower() : QString();
}

bool YtDlpUpdateService::hasValidChecksum(const QByteArray &binary, const QString &expectedSha256)
{
    static const QRegularExpression checksumPattern(QStringLiteral("^[A-Fa-f0-9]{64}$"));
    if (!checksumPattern.match(expectedSha256).hasMatch()) {
        return false;
    }
    const QByteArray actual = QCryptographicHash::hash(binary, QCryptographicHash::Sha256).toHex();
    return actual.compare(expectedSha256.toLatin1(), Qt::CaseInsensitive) == 0;
}

bool YtDlpUpdateService::hasLatestRelease() const
{
    return m_latestRelease.isValid();
}

QString YtDlpUpdateService::latestVersion() const
{
    return m_latestRelease.version;
}

bool YtDlpUpdateService::isBusy() const
{
    return m_checking || m_updating;
}

void YtDlpUpdateService::checkLatestRelease()
{
    if (isBusy()) {
        return;
    }

    m_checking = true;
    QNetworkReply *reply = m_networkManager->get(githubRequest(kNightlyReleaseApi));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_checking = false;
        if (reply->error() != QNetworkReply::NoError) {
            emit checkFailed(releaseError(reply->errorString()));
            return;
        }

        QString error;
        const YtDlpReleaseInfo release = parseLatestRelease(reply->readAll(), &error);
        if (!release.isValid()) {
            emit checkFailed(releaseError(error));
            return;
        }
        m_latestRelease = release;
        emit releaseChecked(m_latestRelease);
    });
}

void YtDlpUpdateService::installLatestRelease()
{
    if (m_updating) {
        return;
    }
    if (!m_latestRelease.isValid()) {
        emit updateFailed(QStringLiteral("Verifique a versão Nightly antes de iniciar a atualização."));
        return;
    }
    m_updating = true;
    downloadChecksums();
}

void YtDlpUpdateService::downloadChecksums()
{
    QNetworkReply *reply = m_networkManager->get(githubRequest(m_latestRelease.checksumsUrl));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            finishUpdateFailure(QStringLiteral("Não foi possível baixar os checksums oficiais: ")
                                + reply->errorString());
            return;
        }
        const QString checksum = checksumForAsset(reply->readAll(), platformAssetName());
        if (checksum.isEmpty()) {
            finishUpdateFailure(QStringLiteral("O checksum do binário Nightly não foi encontrado."));
            return;
        }
        downloadBinary(checksum);
    });
}

void YtDlpUpdateService::downloadBinary(const QString &expectedSha256)
{
    QNetworkReply *reply = m_networkManager->get(githubRequest(m_latestRelease.binaryUrl));
    connect(reply, &QNetworkReply::downloadProgress, this, &YtDlpUpdateService::updateProgress);
    connect(reply, &QNetworkReply::finished, this, [this, reply, expectedSha256]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            finishUpdateFailure(QStringLiteral("Não foi possível baixar o binário Nightly: ")
                                + reply->errorString());
            return;
        }
        const QByteArray binary = reply->readAll();
        if (!hasValidChecksum(binary, expectedSha256)) {
            finishUpdateFailure(QStringLiteral("Checksum inválido; a versão atual foi preservada."));
            return;
        }

        QString version;
        QString error;
        if (!installBinary(binary, m_latestRelease.version, &version, &error)) {
            finishUpdateFailure(error);
            return;
        }

        m_updating = false;
        emit updateCompleted(version, MediaToolResolver::ytDlpUserPath());
    });
}

bool YtDlpUpdateService::installBinary(const QByteArray &binary, const QString &expectedVersion,
                                       QString *installedVersion, QString *errorMessage) const
{
    const QString destination = MediaToolResolver::ytDlpUserPath();
    if (destination.isEmpty()) {
        *errorMessage = QStringLiteral("Não foi possível determinar a pasta de dados do usuário.");
        return false;
    }

    const QDir directory = QFileInfo(destination).dir();
    if (!QDir().mkpath(directory.absolutePath())) {
        *errorMessage = QStringLiteral("Não foi possível criar a pasta de atualização do yt-dlp.");
        return false;
    }

#ifdef Q_OS_WIN
    const QString templateName = directory.absoluteFilePath(QStringLiteral(".yt-dlp-XXXXXX.exe"));
#else
    const QString templateName = directory.absoluteFilePath(QStringLiteral(".yt-dlp-XXXXXX"));
#endif
    QTemporaryFile temporaryFile(templateName);
    temporaryFile.setAutoRemove(true);
    if (!temporaryFile.open()) {
        *errorMessage = QStringLiteral("Não foi possível preparar o arquivo temporário da atualização.");
        return false;
    }
    if (temporaryFile.write(binary) != binary.size() || !temporaryFile.flush()) {
        *errorMessage = QStringLiteral("Não foi possível gravar o arquivo temporário da atualização.");
        return false;
    }
    temporaryFile.close();
#ifndef Q_OS_WIN
    if (!QFile::setPermissions(temporaryFile.fileName(),
                               QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)) {
        *errorMessage = QStringLiteral("Não foi possível tornar executável o novo yt-dlp.");
        return false;
    }
#endif

    const QString version = MediaToolResolver::versionForExecutable(temporaryFile.fileName());
    if (version.isEmpty()) {
        *errorMessage = QStringLiteral("O binário baixado não respondeu corretamente a --version.");
        return false;
    }
    if (version != expectedVersion) {
        *errorMessage = QStringLiteral("O binário validado retornou a versão %1, esperada %2.")
            .arg(version, expectedVersion);
        return false;
    }

    QSaveFile destinationFile(destination);
    if (!destinationFile.open(QIODevice::WriteOnly)
        || destinationFile.write(binary) != binary.size()
        || !destinationFile.commit()) {
        *errorMessage = QStringLiteral("Não foi possível substituir a cópia privada do yt-dlp; a versão anterior foi preservada.");
        return false;
    }
#ifndef Q_OS_WIN
    if (!QFile::setPermissions(destination,
                               QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)) {
        *errorMessage = QStringLiteral("O novo yt-dlp foi salvo, mas não recebeu permissão de execução.");
        return false;
    }
#endif
    *installedVersion = version;
    return true;
}

void YtDlpUpdateService::finishUpdateFailure(const QString &message)
{
    m_updating = false;
    emit updateFailed(message);
}
