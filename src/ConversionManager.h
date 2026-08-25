#ifndef CONVERSIONMANAGER_H
#define CONVERSIONMANAGER_H

#include <QObject>
#include <QHash>
#include <QProcess>
#include <QQueue>
#include <QString>

#include "DownloadManager.h"
#include "GPUDetector.h"

using ConversionId = quint64;

struct ConversionRequest {
    DownloadId ownerDownloadId{0};
    QString inputFile;
    QString format;
    QString outputDirectory;
    GPUType gpuType{GPUType::CPU_ONLY};
    // Optional details from the Linux hardware probe. Older callers can keep
    // using gpuType and the manager will select the legacy encoder mapping.
    QString gpuCodec;
    QString gpuDevice;
};

struct ConversionEnqueueResult {
    bool accepted{false};
    ConversionId id{0};
    QString error;
};

class ConversionManager : public QObject {
    Q_OBJECT

public:
    explicit ConversionManager(QObject *parent = nullptr, const QString &programPath = {});
    ~ConversionManager() override;

    ConversionEnqueueResult enqueueConversion(const ConversionRequest &request);
    bool cancelConversion(ConversionId id);
    void cancelByDownloadId(DownloadId downloadId);
    void cancelAllAutomatic();

    bool hasWork() const;
    bool hasAutomaticWork() const;
    int pendingCount() const;

signals:
    void conversionQueued(ConversionId id, DownloadId ownerDownloadId, int position);
    void conversionStatus(ConversionId id, DownloadId ownerDownloadId, const QString &message);
    void conversionProgress(ConversionId id, DownloadId ownerDownloadId, double percent);
    void conversionCompleted(ConversionId id, DownloadId ownerDownloadId, const QString &outputFile);
    void conversionFailed(ConversionId id, DownloadId ownerDownloadId, const QString &message);
    void conversionCancelled(ConversionId id, DownloadId ownerDownloadId);
    void conversionLog(ConversionId id, DownloadId ownerDownloadId, const QString &message);
    void queueStateChanged(bool active, int pending);
    void queueIdle();

private:
    struct Job;

    QHash<ConversionId, Job *> m_jobs;
    QQueue<Job *> m_pending;
    Job *m_active{nullptr};
    ConversionId m_nextId{1};
    bool m_shuttingDown{false};
    QString m_programPath;

    void startNext();
    void startActiveProcess(const QStringList &arguments);
    void onActiveFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void finishActiveSuccess();
    void finishActiveFailure(const QString &message);
    void readActiveOutput(Job *job, bool flushRemainder = false);
    void cleanupJob(Job *job);
    void emitQueueState();
    void prepareArguments(Job *job);
};

#endif // CONVERSIONMANAGER_H
