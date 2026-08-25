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
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>

namespace {

const QUrl kNightlyReleaseApi(
    QStringLiteral("https://api.github.com/repos/yt-dlp/yt-dlp-nightly-builds/releases/latest"));
constexpr qint64 kMaximumBinaryBytes = 128LL * 1024 * 1024;
constexpr qint64 kMaximumReleasePayloadBytes = 2LL * 1024 * 1024;
constexpr qint64 kMaximumChecksumsBytes = 2LL * 1024 * 1024;

void armReplyTimeout(QNetworkReply *reply, int timeoutMs)
{
    QTimer::singleShot(timeoutMs, reply, [reply]() {
        if (reply->isRunning()) {
            reply->abort();
        }
    });
}

bool isOfficialGitHubUrl(const QUrl &url)
{
    const QString host = url.host().toLower();
    return url.scheme() == QStringLiteral("https")
        && host == QStringLiteral("github.com");
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

YtDlpUpdateService::~YtDlpUpdateService() = default;

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

    m_latestRelease = {};
    m_checking = true;
    QNetworkReply *reply = m_networkManager->get(githubRequest(kNightlyReleaseApi));
    armReplyTimeout(reply, 30000);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_checking = false;
        if (reply->error() != QNetworkReply::NoError) {
            emit checkFailed(releaseError(reply->errorString()));
            return;
        }

        const QByteArray payload = reply->readAll();
        if (payload.size() > kMaximumReleasePayloadBytes) {
            emit checkFailed(releaseError(QStringLiteral("resposta da release excede o limite permitido.")));
            return;
        }
        QString error;
        const YtDlpReleaseInfo release = parseLatestRelease(payload, &error);
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
    armReplyTimeout(reply, 30000);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            finishUpdateFailure(QStringLiteral("Não foi possível baixar os checksums oficiais: ")
                                + reply->errorString());
            return;
        }
        const QByteArray checksums = reply->readAll();
        if (checksums.size() > kMaximumChecksumsBytes) {
            finishUpdateFailure(QStringLiteral("O arquivo de checksums excede o limite permitido."));
            return;
        }
        const QString checksum = checksumForAsset(checksums, platformAssetName());
        if (checksum.isEmpty()) {
            finishUpdateFailure(QStringLiteral("O checksum do binário Nightly não foi encontrado."));
            return;
        }
        downloadBinary(checksum);
    });
}

void YtDlpUpdateService::downloadBinary(const QString &expectedSha256)
{
    QString temporaryDirectory = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (temporaryDirectory.isEmpty()) {
        temporaryDirectory = QDir::tempPath();
    }
    temporaryDirectory = QDir(temporaryDirectory).filePath(QStringLiteral("PrismDownloader/yt-dlp"));
    if (!QDir().mkpath(temporaryDirectory)) {
        finishUpdateFailure(QStringLiteral("Não foi possível preparar o arquivo temporário do yt-dlp."));
        return;
    }
#ifdef Q_OS_WIN
    const QString templateName = QDir(temporaryDirectory).filePath(QStringLiteral(".yt-dlp-XXXXXX.exe"));
#else
    const QString templateName = QDir(temporaryDirectory).filePath(QStringLiteral(".yt-dlp-XXXXXX"));
#endif
    m_binaryFile = std::make_unique<QTemporaryFile>(templateName);
    m_binaryFile->setAutoRemove(true);
    if (!m_binaryFile->open()) {
        m_binaryFile.reset();
        finishUpdateFailure(QStringLiteral("Não foi possível criar o arquivo temporário do yt-dlp."));
        return;
    }
    m_binaryHasher.reset();
    m_binaryDownloaded = 0;
    m_binaryWriteFailed = false;

    QNetworkReply *reply = m_networkManager->get(githubRequest(m_latestRelease.binaryUrl));
    armReplyTimeout(reply, 5 * 60 * 1000);
    connect(reply, &QNetworkReply::downloadProgress, this, &YtDlpUpdateService::updateProgress);
    const auto writeChunk = [this](const QByteArray &bytes) {
        if (bytes.isEmpty() || !m_binaryFile) {
            return true;
        }
        if (m_binaryDownloaded > kMaximumBinaryBytes - bytes.size()
            || m_binaryFile->write(bytes) != bytes.size()) {
            return false;
        }
        m_binaryDownloaded += bytes.size();
        m_binaryHasher.addData(bytes);
        return true;
    };
    connect(reply, &QIODevice::readyRead, this, [this, reply, writeChunk]() {
        if (m_binaryWriteFailed) {
            return;
        }
        const QByteArray bytes = reply->readAll();
        if (!writeChunk(bytes)) {
            m_binaryWriteFailed = true;
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, expectedSha256]() {
        const QByteArray remaining = reply->readAll();
        if (!remaining.isEmpty() && !m_binaryWriteFailed) {
            if (m_binaryDownloaded > kMaximumBinaryBytes - remaining.size()
                || !m_binaryFile || m_binaryFile->write(remaining) != remaining.size()) {
                m_binaryWriteFailed = true;
            } else {
                m_binaryDownloaded += remaining.size();
                m_binaryHasher.addData(remaining);
            }
        }
        reply->deleteLater();
        if (m_binaryWriteFailed) {
            finishUpdateFailure(QStringLiteral("Não foi possível gravar o binário Nightly."));
            return;
        }
        if (reply->error() != QNetworkReply::NoError || !m_binaryFile) {
            finishUpdateFailure(QStringLiteral("Não foi possível baixar o binário Nightly: ")
                                + reply->errorString());
            return;
        }
        if (!m_binaryFile->flush()) {
            finishUpdateFailure(QStringLiteral("Não foi possível finalizar o binário Nightly."));
            return;
        }
        m_binaryFile->close();
        const QByteArray actual = m_binaryHasher.result().toHex();
        if (actual.compare(expectedSha256.toLatin1(), Qt::CaseInsensitive) != 0) {
            finishUpdateFailure(QStringLiteral("Checksum inválido; a versão atual foi preservada."));
            return;
        }

        QString version;
        QString error;
        const QString binaryPath = m_binaryFile->fileName();
        if (!installBinary(binaryPath, m_latestRelease.version, &version, &error)) {
            finishUpdateFailure(error);
            return;
        }

        m_binaryFile.reset();
        m_updating = false;
        MediaToolResolver::clearVersionCache();
        emit updateCompleted(version, MediaToolResolver::ytDlpUserPath());
    });
}

bool YtDlpUpdateService::installBinary(const QString &binaryPath, const QString &expectedVersion,
                                       QString *installedVersion, QString *errorMessage) const
{
    if (!QFileInfo(binaryPath).isFile()) {
        *errorMessage = QStringLiteral("O binário temporário do yt-dlp não existe.");
        return false;
    }
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

#ifndef Q_OS_WIN
    if (!QFile::setPermissions(binaryPath,
                               QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)) {
        *errorMessage = QStringLiteral("Não foi possível tornar executável o novo yt-dlp.");
        return false;
    }
#endif

    const QString version = MediaToolResolver::versionForExecutable(binaryPath);
    if (version.isEmpty()) {
        *errorMessage = QStringLiteral("O binário baixado não respondeu corretamente a --version.");
        return false;
    }
    if (version != expectedVersion) {
        *errorMessage = QStringLiteral("O binário validado retornou a versão %1, esperada %2.")
            .arg(version, expectedVersion);
        return false;
    }

    QFile sourceFile(binaryPath);
    QSaveFile destinationFile(destination);
    if (!sourceFile.open(QIODevice::ReadOnly)
        || !destinationFile.open(QIODevice::WriteOnly)) {
        *errorMessage = QStringLiteral("Não foi possível substituir a cópia privada do yt-dlp; a versão anterior foi preservada.");
        return false;
    }
    while (!sourceFile.atEnd()) {
        const QByteArray chunk = sourceFile.read(1024 * 1024);
        if (chunk.isEmpty() && sourceFile.error() != QFileDevice::NoError) {
            *errorMessage = QStringLiteral("Não foi possível ler o binário temporário do yt-dlp.");
            return false;
        }
        if (destinationFile.write(chunk) != chunk.size()) {
            *errorMessage = QStringLiteral("Não foi possível gravar a cópia privada do yt-dlp.");
            return false;
        }
    }
    if (!destinationFile.commit()) {
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
    m_binaryFile.reset();
    m_updating = false;
    emit updateFailed(message);
}
