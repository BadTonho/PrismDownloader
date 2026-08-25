#include "PlaylistPreviewService.h"

#include "MediaToolResolver.h"

#include <QFile>
#include <QProgressDialog>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

namespace {
constexpr int kMaximumPlaylistItems = 500;
constexpr qsizetype kMaximumPlaylistOutputBytes = 4 * 1024 * 1024;
constexpr qsizetype kMaximumPlaylistErrorBytes = 128 * 1024;
}

PlaylistPreviewService::PlaylistPreviewService(QWidget *dialogParent, QObject *parent)
    : QObject(parent),
      m_dialogParent(dialogParent)
{
}

bool PlaylistPreviewService::isBusy() const
{
    return m_process != nullptr;
}

void PlaylistPreviewService::start(const QUrl &url)
{
    if (isBusy()) {
        return;
    }

    const QString program = MediaToolResolver::resolve(MediaTool::YtDlp);
    if (program.isEmpty() || !QFile::exists(program)) {
        emit previewError("Motor indisponível",
                          MediaToolResolver::missingMessage(MediaTool::YtDlp));
        return;
    }

    auto *process = new QProcess(this);
    m_process = process;
    m_output.clear();
    m_errorOutput.clear();
    m_truncated = false;
    process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
        const QByteArray chunk = process->readAllStandardOutput();
        if (chunk.isEmpty()) {
            return;
        }
        const qsizetype remaining = kMaximumPlaylistOutputBytes - m_output.size();
        if (remaining <= 0) {
            m_truncated = true;
            return;
        }
        if (chunk.size() > remaining) {
            m_output.append(chunk.constData(), static_cast<int>(remaining));
            m_truncated = true;
        } else {
            m_output.append(chunk);
        }
    });
    connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
        const QByteArray chunk = process->readAllStandardError();
        if (chunk.isEmpty()) {
            return;
        }
        const qsizetype remaining = kMaximumPlaylistErrorBytes - m_errorOutput.size();
        if (remaining > 0) {
            m_errorOutput.append(
                chunk.constData(), static_cast<int>(qMin<qsizetype>(remaining, chunk.size())));
        }
    });
    connect(process, &QProcess::finished, this,
            [this, process](int exitCode, QProcess::ExitStatus) {
        if (m_process != process) {
            process->deleteLater();
            return;
        }
        finishProcess(process, exitCode);
    });
    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && m_process == process) {
            failToStart(process);
        }
    });

    emit busyChanged(true);
    if (m_dialogParent) {
        m_dialog = new QProgressDialog(
            "Consultando os vídeos da playlist...\nIsso pode levar alguns segundos.",
            "Cancelar", 0, 0, m_dialogParent);
        m_dialog->setWindowTitle("Consultando playlist");
        m_dialog->setWindowModality(Qt::WindowModal);
        m_dialog->setMinimumDuration(0);
        m_dialog->setAutoClose(false);
        m_dialog->setAutoReset(false);
        m_dialog->show();
        connect(m_dialog, &QProgressDialog::canceled, this, &PlaylistPreviewService::cancel);
    }

    emit logMessage("[Playlist] Consultando os vídeos disponíveis...");
    const QStringList arguments = {
        "--flat-playlist",
        "--print", "%(title)s\t%(webpage_url)s\t%(duration_string)s\t%(thumbnail)s",
        "--skip-download",
        "--quiet",
        "--no-warnings",
        "--no-color",
        "--ignore-errors",
        "--",
        url.toString(QUrl::FullyEncoded)
    };
    process->start(program, arguments);
}

void PlaylistPreviewService::cancel()
{
    if (!m_process) {
        return;
    }

    QProcess *process = m_process;
    m_process = nullptr;
    m_output.clear();
    m_errorOutput.clear();
    m_truncated = false;
    process->kill();
    process->deleteLater();
    closeDialog();
    emit busyChanged(false);
    emit logMessage("[Playlist] Consulta cancelada pelo usuário.");
}

void PlaylistPreviewService::closeDialog()
{
    if (!m_dialog) {
        return;
    }
    m_dialog->hide();
    m_dialog->deleteLater();
    m_dialog = nullptr;
}

QList<PlaylistItem> PlaylistPreviewService::parsePreview(const QByteArray &output)
{
    QList<PlaylistItem> items;
    QSet<QString> seenUrls;
    const QStringList lines = QString::fromUtf8(output).split('\n');
    for (const QString &rawLine : lines) {
        if (items.size() >= kMaximumPlaylistItems) {
            break;
        }
        const QString line = rawLine.trimmed();
        const QStringList fields = line.split('\t');
        if (fields.size() < 2) {
            continue;
        }

        PlaylistItem item;
        item.title = fields.at(0).trimmed();
        item.url = QUrl(fields.at(1).trimmed());
        item.duration = fields.value(2).trimmed();
        item.thumbnailUrl = QUrl(fields.value(3).trimmed());
        if (item.title.isEmpty() || !item.url.isValid() || item.url.host().isEmpty()
            || (item.url.scheme() != "http" && item.url.scheme() != "https")) {
            continue;
        }

        const QString normalizedUrl = item.url.adjusted(QUrl::RemoveFragment)
            .toString(QUrl::FullyEncoded);
        if (!seenUrls.contains(normalizedUrl)) {
            seenUrls.insert(normalizedUrl);
            items.append(item);
        }
    }
    return items;
}

void PlaylistPreviewService::finishProcess(QProcess *process, int exitCode)
{
    const QByteArray finalOutput = process->readAllStandardOutput();
    const qsizetype outputRemaining = kMaximumPlaylistOutputBytes - m_output.size();
    if (outputRemaining <= 0) {
        m_truncated = true;
    } else if (finalOutput.size() > outputRemaining) {
        m_output.append(finalOutput.constData(), static_cast<int>(outputRemaining));
        m_truncated = true;
    } else {
        m_output.append(finalOutput);
    }

    const QByteArray finalError = process->readAllStandardError();
    const qsizetype errorRemaining = kMaximumPlaylistErrorBytes - m_errorOutput.size();
    if (errorRemaining > 0) {
        m_errorOutput.append(finalError.constData(),
                             static_cast<int>(qMin<qsizetype>(errorRemaining, finalError.size())));
    }
    const QList<PlaylistItem> items = parsePreview(m_output);
    const bool truncated = m_truncated;
    const QString errorOutput = QString::fromUtf8(m_errorOutput).trimmed();
    m_output.clear();
    m_errorOutput.clear();
    m_truncated = false;
    m_process = nullptr;
    process->deleteLater();
    closeDialog();
    emit busyChanged(false);
    emit previewReady(items, exitCode, truncated, errorOutput);
}

void PlaylistPreviewService::failToStart(QProcess *process)
{
    if (m_process != process) {
        return;
    }
    m_output.clear();
    m_errorOutput.clear();
    m_truncated = false;
    m_process = nullptr;
    process->deleteLater();
    closeDialog();
    emit busyChanged(false);
    emit previewError("Falha ao consultar playlist",
                      "Não foi possível iniciar o yt-dlp para listar a playlist.");
}
