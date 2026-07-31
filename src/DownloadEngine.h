#ifndef DOWNLOADENGINE_H
#define DOWNLOADENGINE_H

#include <QObject>
#include <QProcess>
#include <QList>
#include "MediaItem.h"
#include "GPUDetector.h"

class DownloadEngine : public QObject {
    Q_OBJECT
public:
    explicit DownloadEngine(QObject *parent = nullptr);
    ~DownloadEngine();

    void initialize();
    void startDownload(const QString &url, const QString &quality = "1080p", const QString &timeRange = "");
    void cancelCurrent();

    bool isDownloading() const;
    GPUDetector* gpuDetector();

signals:
    void progressUpdated(double percentage, const QString &speed, const QString &eta);
    void statusChanged(DownloadStatus status, const QString &message);

private slots:
    void onProcessReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

private:
    QProcess *m_process;
    GPUDetector *m_gpuDetector;
    MediaItem m_currentItem;
    bool m_isRunning = false;

    void parseYtDlpOutput(const QString &line);
};

#endif // DOWNLOADENGINE_H
