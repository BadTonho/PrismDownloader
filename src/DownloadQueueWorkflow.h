#ifndef DOWNLOAD_QUEUE_WORKFLOW_H
#define DOWNLOAD_QUEUE_WORKFLOW_H

#include <QList>
#include <QStringList>

#include "DownloadManager.h"
#include "PlaylistItem.h"

struct DownloadBatchOptions {
    QString quality;
    QString formatSelector;
    QString timeRange;
    QString outputDirectory;
};

struct EnqueuedDownload {
    DownloadId id{0};
    DownloadRequest request;
    QString itemLabel;
};

struct DownloadBatchResult {
    QList<EnqueuedDownload> accepted;
    QStringList rejected;
};

class DownloadQueueWorkflow final {
public:
    explicit DownloadQueueWorkflow(DownloadManager *downloadManager);

    DownloadBatchResult enqueue(const QList<PlaylistItem> &items,
                                const DownloadBatchOptions &options) const;

private:
    DownloadManager *m_downloadManager{nullptr};
};

#endif // DOWNLOAD_QUEUE_WORKFLOW_H
