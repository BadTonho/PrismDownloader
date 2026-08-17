#ifndef YTDLPUPDATESERVICE_H
#define YTDLPUPDATESERVICE_H

#include <QObject>
#include <QUrl>

class QNetworkAccessManager;

struct YtDlpReleaseInfo {
    QString version;
    QUrl binaryUrl;
    QUrl checksumsUrl;

    bool isValid() const {
        return !version.isEmpty() && binaryUrl.isValid() && checksumsUrl.isValid();
    }
};

class YtDlpUpdateService : public QObject {
    Q_OBJECT

public:
    explicit YtDlpUpdateService(QObject *parent = nullptr);

    static QString platformAssetName();
    static YtDlpReleaseInfo parseLatestRelease(const QByteArray &payload, QString *errorMessage = nullptr);
    static QString checksumForAsset(const QByteArray &payload, const QString &assetName);
    static bool hasValidChecksum(const QByteArray &binary, const QString &expectedSha256);

    bool hasLatestRelease() const;
    QString latestVersion() const;
    bool isBusy() const;
    void checkLatestRelease();
    void installLatestRelease();

signals:
    void releaseChecked(const YtDlpReleaseInfo &release);
    void checkFailed(const QString &message);
    void updateProgress(qint64 received, qint64 total);
    void updateCompleted(const QString &version, const QString &path);
    void updateFailed(const QString &message);

private:
    QNetworkAccessManager *m_networkManager;
    YtDlpReleaseInfo m_latestRelease;
    bool m_checking{false};
    bool m_updating{false};

    void downloadChecksums();
    void downloadBinary(const QString &expectedSha256);
    bool installBinary(const QByteArray &binary, const QString &expectedVersion,
                       QString *installedVersion, QString *errorMessage) const;
    void finishUpdateFailure(const QString &message);
};

#endif // YTDLPUPDATESERVICE_H
