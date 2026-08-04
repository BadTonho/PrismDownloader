#include "DownloadManager.h"

#include "DownloadProfile.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>

#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
constexpr auto kCompletedFilePrefix = "__PRISM_OUTPUT__";
}

struct DownloadManager::Job {
    DownloadId id{0};
    DownloadRequest request;
    QString normalizedUrl;
    QProcess *process{nullptr};
    QByteArray outputBuffer;
    QString completedFilePath;
    bool cancelRequested{false};
    bool terminal{false};
    void *nativeJobHandle{nullptr};
};

DownloadManager::DownloadManager(QObject *parent, const QString &programPath)
    : QObject(parent),
      m_programPath(programPath.isEmpty()
                        ? QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/yt-dlp.exe")
                        : QDir::toNativeSeparators(programPath))
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
        }
        cleanupJob(job->id);
    }
}

EnqueueResult DownloadManager::enqueueDownload(const DownloadRequest &request)
{
    if (!QFile::exists(m_programPath)) {
        return {false, 0, "yt-dlp.exe não foi encontrado na pasta do aplicativo."};
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
    for (Job *existing : std::as_const(m_jobs)) {
        if (!existing->terminal && existing->normalizedUrl == canonicalUrl) {
            return {false, existing->id, "Esta URL já está ativa ou aguardando na fila."};
        }
    }

    auto *job = new Job;
    job->id = m_nextId++;
    job->request = request;
    job->normalizedUrl = canonicalUrl;
    m_jobs.insert(job->id, job);
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
    job->process->kill();
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
        if (error == QProcess::FailedToStart) {
            failToStart(id, "Não foi possível iniciar yt-dlp.exe.");
        }
    });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, id = job->id](int exitCode, QProcess::ExitStatus exitStatus) {
        finishJob(id, exitCode, exitStatus);
    });

    process->start(m_programPath, buildArguments(job->request));
}

void DownloadManager::readProcessOutput(DownloadId id, bool flushRemainder)
{
    Job *job = m_jobs.value(id, nullptr);
    if (!job || !job->process) {
        return;
    }

    job->outputBuffer += job->process->readAllStandardOutput();
    qsizetype newline = -1;
    while ((newline = job->outputBuffer.indexOf('\n')) >= 0) {
        QByteArray line = job->outputBuffer.left(newline + 1);
        job->outputBuffer.remove(0, newline + 1);
        parseOutputLine(job, QString::fromUtf8(line).trimmed());
    }
    if (flushRemainder && !job->outputBuffer.isEmpty()) {
        parseOutputLine(job, QString::fromUtf8(job->outputBuffer).trimmed());
        job->outputBuffer.clear();
    }
}

void DownloadManager::parseOutputLine(Job *job, const QString &line)
{
    if (line.isEmpty()) {
        return;
    }
    if (line.startsWith(kCompletedFilePrefix)) {
        job->completedFilePath = line.mid(QString(kCompletedFilePrefix).size()).trimmed();
        emit jobLog(job->id, "Arquivo final identificado pelo yt-dlp.");
        return;
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

void DownloadManager::finishJob(DownloadId id, int exitCode, QProcess::ExitStatus exitStatus)
{
    Job *job = m_jobs.value(id, nullptr);
    if (!job || job->terminal) {
        return;
    }
    readProcessOutput(id, true);
    job->terminal = true;

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
#else
    Q_UNUSED(job);
#endif
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
    arguments << "--progress" << "--newline" << "--no-mtime" << "--windows-filenames"
              << "--print" << "after_move:__PRISM_OUTPUT__%(filepath)s"
              << "-P" << QDir::toNativeSeparators(request.outputDirectory)
              << "-o" << "%(title).180B [%(id)s].%(ext)s";

    if (!request.timeRange.isEmpty()) {
        arguments << "--download-sections" << ("*" + request.timeRange);
    }

    const MediaItem item{"", "", request.quality.toStdString(), "", "", 0.0, DownloadStatus::Queued};
    if (item.isAudioOnly()) {
        arguments << "-x" << "--audio-format" << "mp3" << "--audio-quality" << "0";
    } else {
        arguments << "-f" << QString::fromStdString(
            DownloadProfile::formatSelectorForQuality(request.quality.toStdString()))
                  << "--merge-output-format" << "mp4";
    }
    arguments << "--" << request.url.toString(QUrl::FullyEncoded);
    return arguments;
}
