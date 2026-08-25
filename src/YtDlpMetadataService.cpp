#include "YtDlpMetadataService.h"

#include "MediaToolResolver.h"

#include <QFile>
#include <QProcess>
#include <QProgressDialog>

namespace {
constexpr qsizetype kMaximumOutputBytes = 8 * 1024 * 1024;
constexpr qsizetype kMaximumErrorBytes = 128 * 1024;
}

YtDlpMetadataService::YtDlpMetadataService(QObject *parent)
    : QObject(parent)
{
}

YtDlpMetadataService::~YtDlpMetadataService()
{
    if (m_process) {
        disconnect(m_process, nullptr, this, nullptr);
        m_process->kill();
        m_process->waitForFinished(1000);
        m_process->deleteLater();
        m_process = nullptr;
    }
}

bool YtDlpMetadataService::isRunning() const
{
    return m_process != nullptr;
}

void YtDlpMetadataService::appendOutput(QByteArray &target,
                                        const QByteArray &chunk,
                                        qsizetype limit)
{
    if (chunk.isEmpty() || target.size() >= limit) {
        return;
    }
    const qsizetype remaining = limit - target.size();
    target.append(chunk.constData(),
                  static_cast<int>(qMin<qsizetype>(remaining, chunk.size())));
}

bool YtDlpMetadataService::start(const QList<PlaylistItem> &items, QWidget *progressParent)
{
    if (items.isEmpty() || m_process) {
        return false;
    }

    const QString program = MediaToolResolver::resolve(MediaTool::YtDlp);
    if (program.isEmpty() || !QFile::exists(program)) {
        MediaMetadata metadata;
        metadata.error = MediaToolResolver::missingMessage(MediaTool::YtDlp);
        emit metadataReady(items, metadata);
        return true;
    }

    const PlaylistItem &item = items.first();
    if (!item.url.isValid()) {
        MediaMetadata metadata;
        metadata.error = QStringLiteral("A URL da mídia não é válida para consultar os metadados.");
        emit metadataReady(items, metadata);
        return true;
    }

    m_pendingItems = items;
    m_output.clear();
    m_errorOutput.clear();
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    QProcess *process = m_process;

    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
        appendOutput(m_output, process->readAllStandardOutput(), kMaximumOutputBytes);
    });
    connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
        appendOutput(m_errorOutput, process->readAllStandardError(), kMaximumErrorBytes);
    });
    connect(process, &QProcess::finished, this,
            [this, process](int, QProcess::ExitStatus) {
        if (m_process != process) {
            process->deleteLater();
            return;
        }
        appendOutput(m_output, process->readAllStandardOutput(), kMaximumOutputBytes);
        appendOutput(m_errorOutput, process->readAllStandardError(), kMaximumErrorBytes);
        completeWithMetadata(MediaMetadataParser::parse(m_output));
    });
    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || m_process != process) {
            return;
        }
        MediaMetadata metadata;
        metadata.error = QStringLiteral(
            "Não foi possível iniciar o yt-dlp para consultar os metadados.");
        completeWithMetadata(metadata);
    });

    m_progressDialog = new QProgressDialog(
        QStringLiteral("Consultando formato, resolução e tamanho estimado..."),
        QStringLiteral("Cancelar"), 0, 0, progressParent);
    m_progressDialog->setWindowTitle(QStringLiteral("Consultando metadados"));
    m_progressDialog->setWindowModality(Qt::WindowModal);
    m_progressDialog->setMinimumDuration(0);
    m_progressDialog->setAutoClose(false);
    m_progressDialog->setAutoReset(false);
    connect(m_progressDialog, &QProgressDialog::canceled,
            this, &YtDlpMetadataService::cancel);
    m_progressDialog->show();

    emit busyChanged(true);
    emit logMessage(QStringLiteral("[Metadados] Consultando yt-dlp para %1 item(ns)...")
                        .arg(items.size()));
    process->start(program, {
        QStringLiteral("--dump-single-json"), QStringLiteral("--no-warnings"),
        QStringLiteral("--no-playlist"), QStringLiteral("--skip-download"),
        QStringLiteral("--quiet"), QStringLiteral("--no-color"), QStringLiteral("--"),
        item.url.toString(QUrl::FullyEncoded)});
    return true;
}

void YtDlpMetadataService::cancel()
{
    if (!m_process) {
        return;
    }
    QProcess *process = m_process;
    m_process = nullptr;
    m_pendingItems.clear();
    m_output.clear();
    m_errorOutput.clear();
    if (m_progressDialog) {
        m_progressDialog->hide();
        m_progressDialog->deleteLater();
        m_progressDialog = nullptr;
    }
    process->kill();
    process->deleteLater();
    emit busyChanged(false);
    emit logMessage(QStringLiteral("[Metadados] Consulta cancelada pelo usuário."));
}

void YtDlpMetadataService::finishProcess()
{
    if (m_progressDialog) {
        m_progressDialog->hide();
        m_progressDialog->deleteLater();
        m_progressDialog = nullptr;
    }
    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }
    emit busyChanged(false);
}

void YtDlpMetadataService::completeWithMetadata(MediaMetadata metadata)
{
    if (!m_process) {
        return;
    }

    const QList<PlaylistItem> items = m_pendingItems;
    const QString errorOutput = QString::fromUtf8(m_errorOutput).trimmed();
    if (!metadata.error.isEmpty() && !errorOutput.isEmpty()) {
        metadata.error += QStringLiteral(" Detalhe: %1").arg(errorOutput.left(400));
    }
    finishProcess();
    m_pendingItems.clear();
    m_output.clear();
    m_errorOutput.clear();
    emit metadataReady(items, metadata);
}
