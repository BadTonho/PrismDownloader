#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QObject>
#include <QHash>
#include <QQueue>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "MediaItem.h"

using DownloadId = quint64;

struct DownloadRequest {
    QUrl url;
    QString quality;
    QString timeRange;
    QString outputDirectory;
};

struct EnqueueResult {
    bool accepted{false};
    DownloadId id{0};
    QString error;
};

class DownloadManager : public QObject {
    Q_OBJECT

public:
    explicit DownloadManager(QObject *parent = nullptr, const QString &programPath = {});
    ~DownloadManager() override;

    EnqueueResult enqueueDownload(const DownloadRequest &request);
    bool cancelDownload(DownloadId id);
    void cancelAll();
    void setConcurrencyLimit(int limit);

    int concurrencyLimit() const;
    int activeCount() const;
    int pendingCount() const;
    bool hasWork() const;

signals:
    void jobProgress(DownloadId id, double percent, const QString &speed, const QString &eta);
    void jobStatus(DownloadId id, DownloadStatus status, const QString &message);
    void jobCompleted(DownloadId id, const QString &filePath);
    void jobLog(DownloadId id, const QString &message);
    void queueStateChanged(int active, int pending);
    void queueIdle();

private:
    struct Job;

    QHash<DownloadId, Job *> m_jobs;
    QHash<QString, DownloadId> m_urlOwners;
    QQueue<DownloadId> m_pending;
    DownloadId m_nextId{1};
    int m_concurrencyLimit{2};
    bool m_shuttingDown{false};
    bool m_cancellingAll{false};
    QString m_programPath;
    QString m_programPathOverride;

    bool refreshProgramPath();
    void schedule();
    void startJob(Job *job);
    void readProcessOutput(DownloadId id, bool flushRemainder = false);
    void parseOutputLine(Job *job, const QString &line);
    QString resolveCompletedFilePath(Job *job) const;
    void finishJob(DownloadId id, int exitCode, QProcess::ExitStatus exitStatus);
    void failToStart(DownloadId id, const QString &message);
    void cleanupJob(DownloadId id);
    void terminateProcessTree(Job *job);
    void forceTerminateProcessTree(Job *job);
    void emitQueueState();
    QString normalizedUrl(const QUrl &url) const;
    QStringList buildArguments(const DownloadRequest &request) const;
};

#endif // DOWNLOADMANAGER_H
