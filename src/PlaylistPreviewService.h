#ifndef PLAYLIST_PREVIEW_SERVICE_H
#define PLAYLIST_PREVIEW_SERVICE_H

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

#include "PlaylistItem.h"

class QProcess;
class QProgressDialog;
class QWidget;

class PlaylistPreviewService final : public QObject {
    Q_OBJECT

public:
    explicit PlaylistPreviewService(QWidget *dialogParent, QObject *parent = nullptr);

    bool isBusy() const;
    void start(const QUrl &url);
    void cancel();
    void closeDialog();

signals:
    void busyChanged(bool busy);
    void previewReady(const QList<PlaylistItem> &items,
                      int exitCode,
                      bool truncated,
                      const QString &errorOutput);
    void previewError(const QString &title, const QString &message);
    void logMessage(const QString &message);

private:
    static QList<PlaylistItem> parsePreview(const QByteArray &output);
    void finishProcess(QProcess *process, int exitCode);
    void failToStart(QProcess *process);

    QWidget *m_dialogParent{nullptr};
    QProcess *m_process{nullptr};
    QProgressDialog *m_dialog{nullptr};
    QByteArray m_output;
    QByteArray m_errorOutput;
    bool m_truncated{false};
};

#endif // PLAYLIST_PREVIEW_SERVICE_H
