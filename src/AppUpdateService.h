#ifndef APPUPDATESERVICE_H
#define APPUPDATESERVICE_H

#include <QObject>
#include <QByteArray>
#include <QCryptographicHash>
#include <QUrl>

#include <memory>

class QNetworkAccessManager;
class QNetworkReply;
class QTemporaryFile;

enum class AppUpdatePackageKind {
    WindowsSetup,
    WindowsPortable,
    LinuxDeb
};

struct AppUpdateReleaseInfo {
    QString version;
    QString assetName;
    QString sha256;
    QUrl downloadUrl;

    bool isValid() const {
        return !version.isEmpty() && !assetName.isEmpty() && sha256.size() == 64
            && downloadUrl.isValid();
    }
};

class AppUpdateService : public QObject {
    Q_OBJECT

public:
    explicit AppUpdateService(AppUpdatePackageKind packageKind, QObject *parent = nullptr);

    static AppUpdatePackageKind packageForCurrentPlatform(bool installedWindows);
    static QString expectedAssetName(const QString &version, AppUpdatePackageKind packageKind);
    static bool hasConfiguredPublicKey();
    static bool hasValidChecksum(const QByteArray &binary, const QString &expectedSha256);
    static AppUpdateReleaseInfo parseVerifiedRelease(const QByteArray &releasePayload,
                                                     const QByteArray &manifest,
                                                     const QByteArray &signature,
                                                     const QByteArray &publicKey,
                                                     AppUpdatePackageKind packageKind,
                                                     QString *errorMessage = nullptr);

    bool hasLatestRelease() const;
    const AppUpdateReleaseInfo &latestRelease() const;
    bool isBusy() const;
    void checkLatestRelease();
    void downloadLatestRelease();

signals:
    void releaseChecked(const AppUpdateReleaseInfo &release);
    void checkFailed(const QString &message);
    void downloadProgress(qint64 received, qint64 total);
    void packageVerified(const QString &version, const QString &packagePath);
    void updateFailed(const QString &message);

private:
    QNetworkAccessManager *m_networkManager;
    AppUpdatePackageKind m_packageKind;
    AppUpdateReleaseInfo m_latestRelease;
    QByteArray m_releasePayload;
    QByteArray m_manifest;
    QUrl m_manifestUrl;
    QUrl m_signatureUrl;
    std::unique_ptr<QTemporaryFile> m_downloadFile;
    QCryptographicHash m_downloadHasher{QCryptographicHash::Sha256};
    qint64 m_downloadedBytes{0};
    bool m_downloadWriteFailed{false};
    bool m_checking{false};
    bool m_downloading{false};

    void downloadManifest();
    void downloadSignature();
    void finishDownloadFailure(const QString &message);
};

#endif // APPUPDATESERVICE_H
