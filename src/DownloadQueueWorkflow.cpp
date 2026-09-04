#include "DownloadQueueWorkflow.h"

DownloadQueueWorkflow::DownloadQueueWorkflow(DownloadManager *downloadManager)
    : m_downloadManager(downloadManager)
{
}

DownloadBatchResult DownloadQueueWorkflow::enqueue(
    const QList<PlaylistItem> &items, const DownloadBatchOptions &options) const
{
    DownloadBatchResult batch;
    if (!m_downloadManager) {
        batch.rejected.append(QStringLiteral("Gerenciador de downloads indisponível."));
        return batch;
    }

    for (const PlaylistItem &item : items) {
        DownloadRequest request;
        request.url = item.url;
        request.quality = options.quality;
        request.formatSelector = options.formatSelector;
        request.timeRange = options.timeRange;
        request.outputDirectory = options.outputDirectory;

        const EnqueueResult result = m_downloadManager->enqueueDownload(request);
        if (!result.accepted) {
            batch.rejected.append(item.title + QStringLiteral(": ") + result.error);
            continue;
        }

        EnqueuedDownload accepted;
        accepted.id = result.id;
        accepted.request = request;
        accepted.itemLabel = item.title.isEmpty() ? item.url.toString() : item.title;
        batch.accepted.append(accepted);
    }
    return batch;
}
