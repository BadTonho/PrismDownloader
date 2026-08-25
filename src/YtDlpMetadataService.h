#ifndef YTDLP_METADATA_SERVICE_H
#define YTDLP_METADATA_SERVICE_H

#include <QByteArray>
#include <QList>
#include <QObject>

#include "MediaMetadata.h"
#include "PlaylistItem.h"

class QProcess;
class QProgressDialog;
class QWidget;

class YtDlpMetadataService final : public QObject {
    Q_OBJECT

public:
    explicit YtDlpMetadataService(QObject *parent = nullptr);
    ~YtDlpMetadataService() override;

    bool isRunning() const;
    bool start(const QList<PlaylistItem> &items, QWidget *progressParent);
    void cancel();

signals:
    void busyChanged(bool busy);
    void metadataReady(const QList<PlaylistItem> &items, const MediaMetadata &metadata);
    void logMessage(const QString &message);

private:
    void finishProcess();
    void appendOutput(QByteArray &target, const QByteArray &chunk, qsizetype limit);
    void completeWithMetadata(MediaMetadata metadata);

    QProcess *m_process{nullptr};
    QProgressDialog *m_progressDialog{nullptr};
    QByteArray m_output;
    QByteArray m_errorOutput;
    QList<PlaylistItem> m_pendingItems;
};

#endif // YTDLP_METADATA_SERVICE_H
