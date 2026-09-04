#include "DownloadManager.h"

#include "DownloadProfile.h"
#include "MediaToolResolver.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSet>
#include <QTimer>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef Q_OS_LINUX
#include <csignal>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace {
constexpr auto kCompletedFilePrefix = "__PRISM_OUTPUT__";
constexpr qsizetype kMaximumUnterminatedOutputBytes = 1024 * 1024;

QString processErrorName(QProcess::ProcessError error)
{
    switch (error) {
    case QProcess::FailedToStart: return QStringLiteral("falha ao iniciar");
    case QProcess::Crashed: return QStringLiteral("processo encerrado inesperadamente");
    case QProcess::Timedout: return QStringLiteral("tempo limite excedido");
    case QProcess::ReadError: return QStringLiteral("erro de leitura");
    case QProcess::WriteError: return QStringLiteral("erro de escrita");
    case QProcess::UnknownError: return QStringLiteral("erro desconhecido");
    }
    return QStringLiteral("erro não identificado");
}

QString unquotePath(const QString &value)
{
    QString path = value.trimmed();
    if (path.size() >= 2
        && ((path.startsWith(QLatin1Char('\"')) && path.endsWith(QLatin1Char('\"')))
            || (path.startsWith(QLatin1Char('\'')) && path.endsWith(QLatin1Char('\''))))) {
        path = path.mid(1, path.size() - 2).trimmed();
    }
    return path;
}

QString decodePrintedPath(const QString &value)
{
    const QString encoded = value.trimmed();
    if (encoded.isEmpty()) {
        return {};
    }

    // %(filepath)j is a JSON string. Wrapping it in an array lets Qt parse a
    // scalar JSON value on Qt versions where QJsonDocument accepts only
    // object/array roots.
    if (encoded.startsWith(QLatin1Char('\"')) && encoded.endsWith(QLatin1Char('\"'))) {
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(
            (QStringLiteral("[") + encoded + QLatin1Char(']')).toUtf8(), &error);
        if (error.error == QJsonParseError::NoError && document.isArray()
            && document.array().size() == 1 && document.array().first().isString()) {
            return document.array().first().toString().trimmed();
        }
    }
    return unquotePath(encoded);
}

bool isMediaFile(const QFileInfo &fileInfo)
{
    if (!fileInfo.isFile() || fileInfo.size() <= 0) {
        return false;
    }
    static const QSet<QString> extensions{
        QStringLiteral("aac"), QStringLiteral("avi"), QStringLiteral("flac"),
        QStringLiteral("flv"),
        QStringLiteral("m4a"), QStringLiteral("mkv"), QStringLiteral("mov"),
        QStringLiteral("mp3"), QStringLiteral("mp4"), QStringLiteral("ogg"),
        QStringLiteral("opus"), QStringLiteral("wav"), QStringLiteral("webm")
    };
    return extensions.contains(fileInfo.suffix().toLower());
}

}

struct DownloadManager::Job {
    DownloadId id{0};
    DownloadRequest request;
    QString normalizedUrl;
    QProcess *process{nullptr};
    QByteArray outputBuffer;
    QString completedFilePath;
    QStringList observedFilePaths;
    QStringList postProcessFilePaths;
    QSet<QString> outputFilesAtStart;
    QDateTime startedAt;
    bool cancelRequested{false};
    bool terminal{false};
    void *nativeJobHandle{nullptr};
};

DownloadManager::DownloadManager(QObject *parent, const QString &programPath)
    : QObject(parent),
      m_programPath(MediaToolResolver::resolve(MediaTool::YtDlp, programPath)),
      m_programPathOverride(programPath)
{
}

DownloadManager::~DownloadManager()
{
    m_shuttingDown = true;
    m_pending.clear();
    const auto jobs = m_jobs.values();
    for (Job *job : jobs) {
        if (job->process) {
            disconnect(job->process, nullptr, this, nullptr);
            terminateProcessTree(job);
            job->process->kill();
            job->process->waitForFinished(2000);
            if (job->process->state() != QProcess::NotRunning) {
                forceTerminateProcessTree(job);
                job->process->waitForFinished(1000);
            }
        }
        cleanupJob(job->id);
    }
}

EnqueueResult DownloadManager::enqueueDownload(const DownloadRequest &request)
{
    if (!refreshProgramPath()) {
        return {false, 0, MediaToolResolver::missingMessage(MediaTool::YtDlp)};
    }
    if (!request.url.isValid() || request.url.host().isEmpty()
        || (request.url.scheme() != "https" && request.url.scheme() != "http")) {
        return {false, 0, "A URL informada não é válida."};
    }
    if (request.outputDirectory.isEmpty()
        || (!QDir(request.outputDirectory).exists() && !QDir().mkpath(request.outputDirectory))) {
        return {false, 0, "A pasta de destino não pôde ser criada ou acessada."};
    }

    const QString canonicalUrl = normalizedUrl(request.url);
    const DownloadId existingId = m_urlOwners.value(canonicalUrl, 0);
    if (existingId != 0 && m_jobs.contains(existingId)) {
        return {false, existingId, "Esta URL já está ativa ou aguardando na fila."};
    }

    auto *job = new Job;
    job->id = m_nextId++;
    job->request = request;
    // Keep one absolute directory for yt-dlp, completion detection and the
    // automatic converter. A relative --path makes %(filepath)s relative to
    // the process working directory, not necessarily to the chosen folder.
    job->request.outputDirectory = QDir(request.outputDirectory).absolutePath();
    job->normalizedUrl = canonicalUrl;
    m_jobs.insert(job->id, job);
    m_urlOwners.insert(canonicalUrl, job->id);
    m_pending.enqueue(job->id);

    QTimer::singleShot(0, this, [this, id = job->id]() {
        if (!m_jobs.contains(id)) {
            return;
        }
        emit jobStatus(id, DownloadStatus::Queued, "Aguardando uma vaga na fila...");
        emit jobLog(id, "Tarefa adicionada à fila de downloads.");
        emitQueueState();
        schedule();
    });
    return {true, job->id, {}};
}

bool DownloadManager::cancelDownload(DownloadId id)
{
    Job *job = m_jobs.value(id, nullptr);
    if (!job || job->terminal) {
        return false;
    }

    job->cancelRequested = true;
    if (!job->process) {
        m_pending.removeOne(id);
        job->terminal = true;
        emit jobStatus(id, DownloadStatus::Cancelled, "Download cancelado antes de iniciar.");
        emit jobLog(id, "Tarefa removida da fila.");
        cleanupJob(id);
        emitQueueState();
        if (!m_cancellingAll) {
            schedule();
        }
        return true;
    }

    emit jobStatus(id, DownloadStatus::Cancelling, "Cancelando download...");
    emit jobLog(id, "Cancelamento solicitado; encerrando a árvore de processos.");
    terminateProcessTree(job);
#ifndef Q_OS_LINUX
    job->process->kill();
#endif
    QTimer::singleShot(3000, this, [this, id]() {
        Job *current = m_jobs.value(id, nullptr);
        if (!current || !current->process || !current->cancelRequested
            || current->process->state() == QProcess::NotRunning) {
            return;
        }
        forceTerminateProcessTree(current);
    });
    return true;
}

void DownloadManager::cancelAll()
{
    m_cancellingAll = true;
    const QList<DownloadId> ids = m_jobs.keys();
    for (DownloadId id : ids) {
        cancelDownload(id);
    }
    m_cancellingAll = false;
    emitQueueState();
}

void DownloadManager::setConcurrencyLimit(int limit)
{
    m_concurrencyLimit = qBound(1, limit, 5);
    emitQueueState();
    schedule();
}

int DownloadManager::concurrencyLimit() const
{
    return m_concurrencyLimit;
}

int DownloadManager::activeCount() const
{
    int count = 0;
    for (const Job *job : m_jobs) {
        if (!job->terminal && job->process) {
            ++count;
        }
    }
    return count;
}

int DownloadManager::pendingCount() const
{
    return m_pending.size();
}

bool DownloadManager::hasWork() const
{
    return activeCount() > 0 || !m_pending.isEmpty();
}

void DownloadManager::schedule()
{
    if (m_shuttingDown || m_cancellingAll) {
        return;
    }

    while (activeCount() < m_concurrencyLimit && !m_pending.isEmpty()) {
        const DownloadId id = m_pending.dequeue();
        Job *job = m_jobs.value(id, nullptr);
        if (job && !job->terminal && !job->cancelRequested) {
            startJob(job);
        }
    }
    emitQueueState();
}

void DownloadManager::startJob(Job *job)
{
    if (!refreshProgramPath()) {
        failToStart(job->id, MediaToolResolver::missingMessage(MediaTool::YtDlp));
        return;
    }

    job->startedAt = QDateTime::currentDateTime();
    const QDir outputDirectory(job->request.outputDirectory);
    const QFileInfoList existingFiles = outputDirectory.entryInfoList(
        QDir::Files | QDir::NoSymLinks, QDir::Name);
    for (const QFileInfo &fileInfo : existingFiles) {
        job->outputFilesAtStart.insert(fileInfo.absoluteFilePath());
    }

    auto *process = new QProcess(this);
    job->process = process;
    process->setWorkingDirectory(QCoreApplication::applicationDirPath());
    process->setProcessChannelMode(QProcess::MergedChannels);
#ifdef _WIN32
    process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments) {
        arguments->flags |= 0x08000000; // CREATE_NO_WINDOW
    });
#endif

    connect(process, &QProcess::started, this, [this, id = job->id]() {
        Job *startedJob = m_jobs.value(id, nullptr);
        if (!startedJob || !startedJob->process) {
            return;
        }
#ifdef _WIN32
        HANDLE nativeJob = CreateJobObjectW(nullptr, nullptr);
        if (nativeJob) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            const BOOL configured = SetInformationJobObject(
                nativeJob, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
            HANDLE processHandle = OpenProcess(
                PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE,
                static_cast<DWORD>(startedJob->process->processId()));
            const BOOL assigned = configured && processHandle
                ? AssignProcessToJobObject(nativeJob, processHandle)
                : FALSE;
            if (processHandle) {
                CloseHandle(processHandle);
            }
            if (assigned) {
                startedJob->nativeJobHandle = nativeJob;
            } else {
                CloseHandle(nativeJob);
            }
        }
#endif
        emit jobStatus(id, DownloadStatus::Downloading, "Download iniciado.");
        emit jobLog(id, "yt-dlp iniciado com argumentos separados e seguros.");
    });
    connect(process, &QProcess::readyReadStandardOutput, this, [this, id = job->id]() {
        readProcessOutput(id);
    });
    connect(process, &QProcess::errorOccurred, this, [this, id = job->id](QProcess::ProcessError error) {
        Job *currentJob = m_jobs.value(id, nullptr);
        if (!currentJob || currentJob->terminal || !currentJob->process) {
            return;
        }
        const QString detail = currentJob->process->errorString().trimmed();
        const QString diagnostic = QStringLiteral("Erro do processo yt-dlp (%1)%2")
            .arg(processErrorName(error), detail.isEmpty() ? QString() : QStringLiteral(": ") + detail);
        if (error == QProcess::FailedToStart) {
            emit jobLog(id, diagnostic);
            failToStart(id, "Não foi possível iniciar "
                            + MediaToolResolver::executableName(MediaTool::YtDlp) + ".");
        } else {
            emit jobLog(id, diagnostic);
        }
    });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, id = job->id](int exitCode, QProcess::ExitStatus exitStatus) {
        finishJob(id, exitCode, exitStatus);
    });

#ifdef Q_OS_LINUX
    // yt-dlp pode iniciar o FFmpeg. Uma sessão própria permite sinalizar toda
    // a árvore de processos quando o usuário cancela a tarefa.
    const QString setsid = QStandardPaths::findExecutable(QStringLiteral("setsid"));
    if (setsid.isEmpty()) {
        failToStart(job->id, "O utilitário setsid não foi encontrado para isolar o download.");
        return;
    }
    QStringList sessionArguments;
    sessionArguments << m_programPath << buildArguments(job->request);
    process->start(setsid, sessionArguments);
#else
    process->start(m_programPath, buildArguments(job->request));
#endif
}

bool DownloadManager::refreshProgramPath()
{
    m_programPath = MediaToolResolver::resolve(MediaTool::YtDlp, m_programPathOverride);
    return !m_programPath.isEmpty() && QFile::exists(m_programPath);
}

void DownloadManager::readProcessOutput(DownloadId id, bool flushRemainder)
{
    Job *job = m_jobs.value(id, nullptr);
    if (!job || !job->process) {
        return;
    }

    job->outputBuffer += job->process->readAllStandardOutput();
    qsizetype scanOffset = 0;
    qsizetype newline = -1;
    while ((newline = job->outputBuffer.indexOf('\n', scanOffset)) >= 0) {
        const QByteArray line = job->outputBuffer.mid(scanOffset, newline - scanOffset);
        scanOffset = newline + 1;
        parseOutputLine(job, QString::fromUtf8(line).trimmed());
    }
    if (scanOffset > 0) {
        job->outputBuffer.remove(0, scanOffset);
    }
    if (flushRemainder && !job->outputBuffer.isEmpty()) {
        parseOutputLine(job, QString::fromUtf8(job->outputBuffer).trimmed());
        job->outputBuffer.clear();
    } else if (job->outputBuffer.size() > kMaximumUnterminatedOutputBytes) {
        parseOutputLine(job, QString::fromUtf8(job->outputBuffer.left(kMaximumUnterminatedOutputBytes)).trimmed());
        job->outputBuffer.clear();
    }
}

void DownloadManager::parseOutputLine(Job *job, const QString &line)
{
    if (line.isEmpty()) {
        return;
    }
    if (line.startsWith(kCompletedFilePrefix)) {
        job->completedFilePath = decodePrintedPath(
            line.mid(QString(kCompletedFilePrefix).size()));
        if (!job->completedFilePath.isEmpty()) {
            job->observedFilePaths.append(job->completedFilePath);
        }
        emit jobLog(job->id, "Arquivo final identificado pelo yt-dlp.");
        return;
    }

    // These lines are useful as a fallback for yt-dlp versions that print a
    // stale after_move:filepath after a merge or audio extraction.
    static const QStringList outputMarkers{
        QStringLiteral("Destination:"),
        QStringLiteral("Merging formats into:")
    };
    for (const QString &marker : outputMarkers) {
        const int markerIndex = line.indexOf(marker, 0, Qt::CaseInsensitive);
        if (markerIndex < 0) {
            continue;
        }
        const QString reportedPath = decodePrintedPath(line.mid(markerIndex + marker.size()));
        if (!reportedPath.isEmpty()) {
            job->observedFilePaths.append(reportedPath);
            if (marker.compare(QStringLiteral("Merging formats into:"), Qt::CaseInsensitive) == 0
                || line.contains(QStringLiteral("[ExtractAudio]"), Qt::CaseInsensitive)
                || line.contains(QStringLiteral("[FFmpegExtractAudio]"), Qt::CaseInsensitive)
                || line.contains(QStringLiteral("[VideoConvertor]"), Qt::CaseInsensitive)) {
                job->postProcessFilePaths.append(reportedPath);
            }
        }
        break;
    }
    if (line.contains("ERROR:", Qt::CaseInsensitive)
        || line.contains("HTTP Error", Qt::CaseInsensitive)
        || line.contains("Unable to download", Qt::CaseInsensitive)) {
        emit jobLog(job->id, "Erro: " + line);
        return;
    }
    if (line.contains("WARNING:", Qt::CaseInsensitive)) {
        emit jobLog(job->id, "Alerta: " + line);
        return;
    }
    if (line.contains("[Merger]", Qt::CaseInsensitive)
        || line.contains("Merging formats into", Qt::CaseInsensitive)) {
        emit jobStatus(job->id, DownloadStatus::Muxing, "Mesclando áudio e vídeo...");
        emit jobLog(job->id, line);
        return;
    }

    static const QRegularExpression progressPattern(
        R"(\[download\]\s+([0-9.]+)%.*at\s+([^\s]+)\s+ETA\s+([0-9:]+))");
    const QRegularExpressionMatch match = progressPattern.match(line);
    if (match.hasMatch()) {
        emit jobProgress(job->id, match.captured(1).toDouble(), match.captured(2), match.captured(3));
        return;
    }

    if (!line.startsWith("[download]")) {
        emit jobLog(job->id, line);
    }
}

QString DownloadManager::resolveCompletedFilePath(Job *job) const
{
    if (!job) {
        return {};
    }

    const QDir outputDirectory(job->request.outputDirectory);
    const auto absolutePath = [&outputDirectory](const QString &reportedPath) {
        const QString path = decodePrintedPath(reportedPath);
        if (path.isEmpty()) {
            return QString();
        }
        const QFileInfo fileInfo(path);
        return fileInfo.isAbsolute()
            ? fileInfo.absoluteFilePath()
            : outputDirectory.absoluteFilePath(path);
    };
    const auto existingMediaPath = [](const QString &path) {
        const QFileInfo fileInfo(path);
        return isMediaFile(fileInfo) ? fileInfo.absoluteFilePath() : QString();
    };

    // Prefer paths explicitly emitted by a postprocessor. Some yt-dlp
    // versions can leave an older intermediate path in after_move:filepath
    // even though the merged/extracted file already exists beside it.
    for (auto iterator = job->postProcessFilePaths.crbegin();
         iterator != job->postProcessFilePaths.crend(); ++iterator) {
        if (const QString path = existingMediaPath(absolutePath(*iterator));
            !path.isEmpty()) {
            return path;
        }
    }

    // Next prefer the explicit after_move result. It normally knows the final
    // extension after audio extraction or format merging.
    if (const QString path = existingMediaPath(absolutePath(job->completedFilePath));
        !path.isEmpty()) {
        return path;
    }

    // If after_move:filepath is stale, yt-dlp's Destination/Merger messages
    // still normally contain the path that was physically written.
    for (auto iterator = job->observedFilePaths.crbegin();
         iterator != job->observedFilePaths.crend(); ++iterator) {
        if (const QString path = existingMediaPath(absolutePath(*iterator));
            !path.isEmpty()) {
            return path;
        }
    }

    // Last resort: associate a newly-created media file in the selected
    // directory with this job. Existing files are excluded so an unrelated
    // library item is never selected just because it is the newest file.
    QFileInfo newestFile;
    const QFileInfoList files = outputDirectory.entryInfoList(
        QDir::Files | QDir::NoSymLinks, QDir::Time);
    for (const QFileInfo &fileInfo : files) {
        if (!isMediaFile(fileInfo)
            || job->outputFilesAtStart.contains(fileInfo.absoluteFilePath())) {
            continue;
        }
        if (job->startedAt.isValid()
            && fileInfo.lastModified() < job->startedAt.addSecs(-2)) {
            continue;
        }
        if (newestFile.filePath().isEmpty() || fileInfo.lastModified() > newestFile.lastModified()) {
            newestFile = fileInfo;
        }
    }
    return newestFile.filePath().isEmpty() ? QString() : newestFile.absoluteFilePath();
}

void DownloadManager::finishJob(DownloadId id, int exitCode, QProcess::ExitStatus exitStatus)
{
    Job *job = m_jobs.value(id, nullptr);
    if (!job || job->terminal) {
        return;
    }
    readProcessOutput(id, true);
    job->terminal = true;
    const QString resolvedFilePath = resolveCompletedFilePath(job);
    if (!resolvedFilePath.isEmpty()) {
        if (resolvedFilePath != job->completedFilePath) {
            emit jobLog(id, "Arquivo final localizado em: " + resolvedFilePath);
        }
        job->completedFilePath = resolvedFilePath;
    }

    if (job->cancelRequested) {
        emit jobStatus(id, DownloadStatus::Cancelled, "Download cancelado.");
        emit jobLog(id, "Processo encerrado por solicitação do usuário.");
    } else if (exitStatus == QProcess::CrashExit || exitCode != 0) {
        emit jobStatus(id, DownloadStatus::Error,
                       QString("Falha no download (código %1).").arg(exitCode));
        emit jobLog(id, QString("yt-dlp/FFmpeg encerrou com código %1.").arg(exitCode));
    } else if (job->completedFilePath.isEmpty() || !QFile::exists(job->completedFilePath)) {
        emit jobStatus(id, DownloadStatus::Error, "O processo terminou sem informar um arquivo final válido.");
        emit jobLog(id, "Saída final ausente ou inválida.");
    } else {
        emit jobCompleted(id, job->completedFilePath);
        emit jobProgress(id, 100.0, {}, "00:00");
        emit jobStatus(id, DownloadStatus::Completed, "Download concluído.");
        emit jobLog(id, "Download finalizado com sucesso.");
    }

    cleanupJob(id);
    emitQueueState();
    schedule();
}

void DownloadManager::failToStart(DownloadId id, const QString &message)
{
    Job *job = m_jobs.value(id, nullptr);
    if (!job || job->terminal) {
        return;
    }
    job->terminal = true;
    emit jobStatus(id, DownloadStatus::Error, message);
    emit jobLog(id, message);
    cleanupJob(id);
    emitQueueState();
    schedule();
}

void DownloadManager::cleanupJob(DownloadId id)
{
    Job *job = m_jobs.take(id);
    if (!job) {
        return;
    }
    if (m_urlOwners.value(job->normalizedUrl) == id) {
        m_urlOwners.remove(job->normalizedUrl);
    }
#ifdef _WIN32
    if (job->nativeJobHandle) {
        CloseHandle(static_cast<HANDLE>(job->nativeJobHandle));
        job->nativeJobHandle = nullptr;
    }
#endif
    if (job->process) {
        job->process->deleteLater();
        job->process = nullptr;
    }
    delete job;
}

void DownloadManager::terminateProcessTree(Job *job)
{
#ifdef _WIN32
    if (job && job->nativeJobHandle) {
        TerminateJobObject(static_cast<HANDLE>(job->nativeJobHandle), 1);
    }
#elif defined(Q_OS_LINUX)
    if (job && job->process && job->process->processId() > 0) {
        const pid_t sessionLeader = static_cast<pid_t>(job->process->processId());
        ::kill(-sessionLeader, SIGTERM);
    }
#else
    Q_UNUSED(job);
#endif
}

void DownloadManager::forceTerminateProcessTree(Job *job)
{
#ifdef _WIN32
    if (job && job->nativeJobHandle) {
        TerminateJobObject(static_cast<HANDLE>(job->nativeJobHandle), 1);
    }
#elif defined(Q_OS_LINUX)
    if (job && job->process && job->process->processId() > 0) {
        const pid_t sessionLeader = static_cast<pid_t>(job->process->processId());
        ::kill(-sessionLeader, SIGKILL);
    }
#else
    Q_UNUSED(job);
#endif
    if (job && job->process && job->process->state() != QProcess::NotRunning) {
        job->process->kill();
    }
}

void DownloadManager::emitQueueState()
{
    const int active = activeCount();
    const int pending = pendingCount();
    emit queueStateChanged(active, pending);
    if (active == 0 && pending == 0) {
        emit queueIdle();
    }
}

QString DownloadManager::normalizedUrl(const QUrl &url) const
{
    return url.adjusted(QUrl::RemoveFragment).toString(QUrl::FullyEncoded);
}

QStringList DownloadManager::buildArguments(const DownloadRequest &request) const
{
    QStringList arguments;
    arguments << "--progress" << "--progress-delta" << "0.25"
              << "--newline" << "--no-mtime"
              << "--concurrent-fragments" << "4"
              << "--retries" << "10" << "--fragment-retries" << "10";
#ifdef Q_OS_WIN
    arguments << "--windows-filenames";
#endif
    arguments << "--print" << "after_move:__PRISM_OUTPUT__%(filepath)j"
              << "-P" << QDir::toNativeSeparators(request.outputDirectory)
              << "-o" << "%(title).180B [%(id)s].%(ext)s";

    if (!request.timeRange.isEmpty()) {
        arguments << "--download-sections" << ("*" + request.timeRange);
    }

    const MediaItem item{"", "", request.quality.toStdString(), "", "", 0.0, DownloadStatus::Queued};
    if (item.isAudioOnly()) {
        if (!request.formatSelector.isEmpty()) {
            arguments << "-f" << request.formatSelector;
        }
        arguments << "-x" << "--audio-format" << "mp3" << "--audio-quality" << "0";
    } else {
        const QString formatSelector = request.formatSelector.isEmpty()
            ? QString::fromStdString(
                DownloadProfile::formatSelectorForQuality(request.quality.toStdString()))
            : request.formatSelector;
        arguments << "-f" << formatSelector
                  << "--merge-output-format" << "mp4";
    }
    arguments << "--" << request.url.toString(QUrl::FullyEncoded);
    return arguments;
}
