#include "ConversionManager.h"

#include "MediaToolResolver.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>

namespace {

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

QString encoderFor(const ConversionRequest &request, bool hevc)
{
    if (request.gpuType != GPUType::CPU_ONLY && !request.gpuCodec.isEmpty()) {
        QString codec = request.gpuCodec;
        if (codec.startsWith(QStringLiteral("h264_"))) {
            codec.replace(0, 5, hevc ? QStringLiteral("hevc_") : QStringLiteral("h264_"));
        } else if (!hevc && codec.startsWith(QStringLiteral("hevc_"))) {
            codec.replace(0, 5, QStringLiteral("h264_"));
        }
        return codec;
    }

    switch (request.gpuType) {
    case GPUType::NVIDIA:
        return hevc ? "hevc_nvenc" : "h264_nvenc";
    case GPUType::AMD:
        return hevc ? "hevc_amf" : "h264_amf";
    case GPUType::INTEL:
        return hevc ? "hevc_qsv" : "h264_qsv";
    case GPUType::VAAPI:
        return hevc ? "hevc_vaapi" : "h264_vaapi";
    case GPUType::CPU_ONLY:
        return {};
    }
    return {};
}

QString uniqueOutputPath(const QDir &directory, const QString &stem, const QString &extension)
{
    QString candidate = directory.absoluteFilePath(stem + extension);
    for (int index = 2; QFile::exists(candidate); ++index) {
        candidate = directory.absoluteFilePath(QString("%1 (%2)%3").arg(stem).arg(index).arg(extension));
    }
    return candidate;
}

}

struct ConversionManager::Job {
    ConversionId id{0};
    ConversionRequest request;
    QProcess *process{nullptr};
    QByteArray outputBuffer;
    QString outputFile;
    QStringList hardwareArguments;
    QStringList cpuArguments;
    bool usesHardware{false};
    bool fallbackAttempted{false};
    bool cancelRequested{false};
    double durationSeconds{0.0};
    double lastProgress{-1.0};
};

ConversionManager::ConversionManager(QObject *parent, const QString &programPath)
    : QObject(parent),
      m_programPath(MediaToolResolver::resolve(MediaTool::Ffmpeg, programPath))
{
}

ConversionManager::~ConversionManager()
{
    m_shuttingDown = true;
    if (m_active && m_active->process) {
        disconnect(m_active->process, nullptr, this, nullptr);
        m_active->process->kill();
        m_active->process->waitForFinished(2000);
    }
    const auto jobs = m_jobs.values();
    for (Job *job : jobs) {
        cleanupJob(job);
    }
    m_pending.clear();
    m_active = nullptr;
}

ConversionEnqueueResult ConversionManager::enqueueConversion(const ConversionRequest &request)
{
    if (m_programPath.isEmpty() || !QFile::exists(m_programPath)) {
        return {false, 0, MediaToolResolver::missingMessage(MediaTool::Ffmpeg)};
    }
    if (request.inputFile.isEmpty() || !QFile::exists(request.inputFile)) {
        return {false, 0, "O arquivo de origem da conversão não existe."};
    }
    if (request.outputDirectory.isEmpty()
        || (!QDir(request.outputDirectory).exists() && !QDir().mkpath(request.outputDirectory))) {
        return {false, 0, "A pasta de saída da conversão não está acessível."};
    }

    auto *job = new Job;
    job->id = m_nextId++;
    job->request = request;
    m_jobs.insert(job->id, job);
    m_pending.enqueue(job);
    const int position = m_pending.size() + (m_active ? 1 : 0);

    QTimer::singleShot(0, this, [this, id = job->id, owner = request.ownerDownloadId, position]() {
        if (!m_jobs.contains(id)) {
            return;
        }
        emit conversionQueued(id, owner, position);
        emit conversionStatus(id, owner, "Aguardando conversão...");
        emitQueueState();
        startNext();
    });
    return {true, job->id, {}};
}

bool ConversionManager::cancelConversion(ConversionId id)
{
    Job *job = m_jobs.value(id, nullptr);
    if (!job) {
        return false;
    }
    job->cancelRequested = true;
    if (job == m_active) {
        emit conversionStatus(job->id, job->request.ownerDownloadId, "Cancelando conversão...");
        if (job->process) {
            job->process->kill();
        }
        return true;
    }

    m_pending.removeOne(job);
    emit conversionCancelled(job->id, job->request.ownerDownloadId);
    emit conversionLog(job->id, job->request.ownerDownloadId, "Conversão removida da fila.");
    cleanupJob(job);
    emitQueueState();
    return true;
}

void ConversionManager::cancelByDownloadId(DownloadId downloadId)
{
    const QList<Job *> jobs = m_jobs.values();
    for (Job *job : jobs) {
        if (job->request.ownerDownloadId == downloadId) {
            cancelConversion(job->id);
        }
    }
}

void ConversionManager::cancelAllAutomatic()
{
    const QList<Job *> jobs = m_jobs.values();
    for (Job *job : jobs) {
        if (job->request.ownerDownloadId != 0) {
            cancelConversion(job->id);
        }
    }
}

bool ConversionManager::hasWork() const
{
    return m_active || !m_pending.isEmpty();
}

bool ConversionManager::hasAutomaticWork() const
{
    if (m_active && m_active->request.ownerDownloadId != 0) {
        return true;
    }
    for (const Job *job : m_pending) {
        if (job->request.ownerDownloadId != 0) {
            return true;
        }
    }
    return false;
}

int ConversionManager::pendingCount() const
{
    return m_pending.size();
}

void ConversionManager::startNext()
{
    if (m_shuttingDown || m_active || m_pending.isEmpty()) {
        emitQueueState();
        return;
    }

    m_active = m_pending.dequeue();
    prepareArguments(m_active);
    emit conversionStatus(m_active->id, m_active->request.ownerDownloadId, "Convertendo mídia...");
    emit conversionLog(m_active->id, m_active->request.ownerDownloadId,
                       "FFmpeg iniciado para " + m_active->outputFile);
    startActiveProcess(m_active->hardwareArguments);
    emitQueueState();
}

void ConversionManager::startActiveProcess(const QStringList &arguments)
{
    if (!m_active) {
        return;
    }
    Job *job = m_active;
    if (!m_active->process) {
        m_active->process = new QProcess(this);
        m_active->process->setProcessChannelMode(QProcess::MergedChannels);
#ifdef _WIN32
        m_active->process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *processArguments) {
            processArguments->flags |= 0x08000000; // CREATE_NO_WINDOW
        });
#endif
        QProcess *process = m_active->process;
        const ConversionId jobId = job->id;
        connect(process, &QProcess::readyReadStandardOutput, this, [this, process, jobId]() {
            if (!m_active || m_active->id != jobId || m_active->process != process) {
                return;
            }
            readActiveOutput(m_active);
        });
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, process, jobId](int exitCode, QProcess::ExitStatus exitStatus) {
            if (m_active && m_active->id == jobId && m_active->process == process) {
                onActiveFinished(exitCode, exitStatus);
            }
        });
        connect(process, &QProcess::errorOccurred, this, [this, process, jobId](QProcess::ProcessError error) {
            if (m_active && m_active->id == jobId && m_active->process == process) {
                const QString detail = process->errorString().trimmed();
                const QString diagnostic = QStringLiteral("Erro do processo FFmpeg (%1)%2")
                    .arg(processErrorName(error), detail.isEmpty() ? QString() : QStringLiteral(": ") + detail);
                emit conversionLog(jobId, m_active->request.ownerDownloadId, diagnostic);
            }
            if (error == QProcess::FailedToStart && m_active
                && m_active->id == jobId && m_active->process == process) {
                finishActiveFailure("Não foi possível iniciar "
                                    + MediaToolResolver::executableName(MediaTool::Ffmpeg) + ".");
            }
        });
    }
    job->outputBuffer.clear();
    job->durationSeconds = 0.0;
    job->lastProgress = -1.0;
    m_active->process->start(m_programPath, arguments);
}

void ConversionManager::readActiveOutput(Job *job, bool flushRemainder)
{
    if (!job || !job->process) {
        return;
    }
    job->outputBuffer += job->process->readAllStandardOutput();
    qsizetype scanOffset = 0;
    qsizetype newline = -1;
    static const QRegularExpression durationPattern(
        QStringLiteral("Duration:\\s*(\\d+):(\\d+):(\\d+(?:\\.\\d+)?)"));
    while ((newline = job->outputBuffer.indexOf('\n', scanOffset)) >= 0) {
        const QString line = QString::fromUtf8(
            job->outputBuffer.mid(scanOffset, newline - scanOffset)).trimmed();
        scanOffset = newline + 1;
        const QRegularExpressionMatch durationMatch = durationPattern.match(line);
        if (durationMatch.hasMatch()) {
            job->durationSeconds = durationMatch.captured(1).toDouble() * 3600.0
                + durationMatch.captured(2).toDouble() * 60.0
                + durationMatch.captured(3).toDouble();
            continue;
        }
        if (line.startsWith(QStringLiteral("out_time_ms="))) {
            if (job->durationSeconds > 0.0) {
                const double elapsedSeconds = line.mid(12).toDouble() / 1000000.0;
                const double progress = qBound(0.0, elapsedSeconds / job->durationSeconds * 100.0, 100.0);
                if (job->lastProgress < 0.0 || progress - job->lastProgress >= 0.5 || progress >= 100.0) {
                    job->lastProgress = progress;
                    emit conversionProgress(job->id, job->request.ownerDownloadId, progress);
                }
            }
            continue;
        }
        if (line.startsWith(QStringLiteral("progress=")) || line.isEmpty()) {
            continue;
        }
        emit conversionLog(job->id, job->request.ownerDownloadId, line);
    }
    if (scanOffset > 0) {
        job->outputBuffer.remove(0, scanOffset);
    }
    if (flushRemainder && !job->outputBuffer.isEmpty()) {
        emit conversionLog(job->id, job->request.ownerDownloadId,
                           QString::fromUtf8(job->outputBuffer).trimmed());
        job->outputBuffer.clear();
    } else if (job->outputBuffer.size() > kMaximumUnterminatedOutputBytes) {
        emit conversionLog(job->id, job->request.ownerDownloadId,
                           QString::fromUtf8(job->outputBuffer.left(kMaximumUnterminatedOutputBytes)).trimmed());
        job->outputBuffer.clear();
    }
}

void ConversionManager::onActiveFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (!m_active) {
        return;
    }
    readActiveOutput(m_active, true);
    if (m_active->cancelRequested) {
        Job *cancelled = m_active;
        emit conversionCancelled(cancelled->id, cancelled->request.ownerDownloadId);
        emit conversionLog(cancelled->id, cancelled->request.ownerDownloadId, "Conversão cancelada.");
        m_active = nullptr;
        cleanupJob(cancelled);
        emitQueueState();
        startNext();
        return;
    }
    if ((exitStatus == QProcess::CrashExit || exitCode != 0)
        && m_active->usesHardware && !m_active->fallbackAttempted) {
        m_active->fallbackAttempted = true;
        QFile::remove(m_active->outputFile);
        emit conversionStatus(m_active->id, m_active->request.ownerDownloadId,
                              "Aceleração indisponível; repetindo com CPU...");
        emit conversionLog(m_active->id, m_active->request.ownerDownloadId,
                           "Encoder de hardware falhou; fallback de CPU iniciado.");
        startActiveProcess(m_active->cpuArguments);
        return;
    }
    if (exitStatus == QProcess::NormalExit && exitCode == 0 && QFile::exists(m_active->outputFile)) {
        finishActiveSuccess();
    } else {
        finishActiveFailure(QString("FFmpeg encerrou com código %1.").arg(exitCode));
    }
}

void ConversionManager::finishActiveSuccess()
{
    Job *completed = m_active;
    m_active = nullptr;

    if (completed->request.removeInputOnSuccess) {
        const QFileInfo inputInfo(completed->request.inputFile);
        const QFileInfo outputInfo(completed->outputFile);
        const QString inputPath = inputInfo.canonicalFilePath();
        const QString outputPath = outputInfo.canonicalFilePath();
        if (!inputPath.isEmpty() && !outputPath.isEmpty() && inputPath != outputPath) {
            if (QFile::remove(inputPath)) {
                emit conversionLog(completed->id, completed->request.ownerDownloadId,
                                   QStringLiteral("Arquivo original removido apos a conversao bem-sucedida."));
            } else {
                emit conversionLog(completed->id, completed->request.ownerDownloadId,
                                   QStringLiteral("Nao foi possivel remover o arquivo original apos a conversao."));
            }
        }
    }

    emit conversionProgress(completed->id, completed->request.ownerDownloadId, 100.0);
    emit conversionCompleted(completed->id, completed->request.ownerDownloadId, completed->outputFile);
    emit conversionLog(completed->id, completed->request.ownerDownloadId, "Conversão concluída.");
    cleanupJob(completed);
    emitQueueState();
    startNext();
}

void ConversionManager::finishActiveFailure(const QString &message)
{
    if (!m_active) {
        return;
    }
    Job *failed = m_active;
    m_active = nullptr;
    emit conversionFailed(failed->id, failed->request.ownerDownloadId, message);
    emit conversionLog(failed->id, failed->request.ownerDownloadId, message);
    cleanupJob(failed);
    emitQueueState();
    startNext();
}

void ConversionManager::cleanupJob(Job *job)
{
    if (!job) {
        return;
    }
    m_jobs.remove(job->id);
    m_pending.removeOne(job);
    if (job->process) {
        job->process->deleteLater();
        job->process = nullptr;
    }
    delete job;
}

void ConversionManager::emitQueueState()
{
    emit queueStateChanged(m_active != nullptr, m_pending.size());
    if (!m_active && m_pending.isEmpty()) {
        emit queueIdle();
    }
}

void ConversionManager::prepareArguments(Job *job)
{
    const QFileInfo input(job->request.inputFile);
    const QDir outputDirectory(job->request.outputDirectory);
    QString stem = input.completeBaseName() + "_convertido";
    QString extension = ".mp4";

    QStringList hardwareArgs{"-hide_banner", "-nostats", "-progress", "pipe:1",
                             "-n", "-i", job->request.inputFile};
    QStringList cpuArgs = hardwareArgs;
    const QString format = job->request.format;

    if (format.startsWith("MP4 (H.264")) {
        const QString encoder = encoderFor(job->request, false);
        const bool usesVaapi = encoder.endsWith(QStringLiteral("_vaapi"));
        job->usesHardware = !encoder.isEmpty()
            && (!usesVaapi || !job->request.gpuDevice.isEmpty());
        if (job->usesHardware && usesVaapi) {
            hardwareArgs = QStringList{
                QStringLiteral("-init_hw_device"),
                QStringLiteral("vaapi=prism_vaapi:%1").arg(job->request.gpuDevice),
                QStringLiteral("-filter_hw_device"),
                QStringLiteral("prism_vaapi")
            } + hardwareArgs;
        }
        if (job->usesHardware) {
            hardwareArgs << "-c:v" << encoder;
            if (usesVaapi) {
                hardwareArgs << "-vf" << "format=nv12,hwupload";
            }
            if (job->request.gpuType == GPUType::NVIDIA && !usesVaapi) {
                hardwareArgs << "-preset" << "p4" << "-cq" << "23";
            } else {
                hardwareArgs << "-b:v" << "5M";
            }
            hardwareArgs << "-c:a" << "aac" << "-b:a" << "192k";
        } else {
            hardwareArgs << "-c:v" << "libx264" << "-crf" << "23" << "-c:a" << "aac";
        }
        cpuArgs << "-c:v" << "libx264" << "-crf" << "23" << "-c:a" << "aac";
    } else if (format.startsWith("MP4 (HEVC")) {
        stem = input.completeBaseName() + "_hevc";
        const QString encoder = encoderFor(job->request, true);
        const bool usesVaapi = encoder.endsWith(QStringLiteral("_vaapi"));
        job->usesHardware = !encoder.isEmpty()
            && (!usesVaapi || !job->request.gpuDevice.isEmpty());
        if (job->usesHardware && usesVaapi) {
            hardwareArgs = QStringList{
                QStringLiteral("-init_hw_device"),
                QStringLiteral("vaapi=prism_vaapi:%1").arg(job->request.gpuDevice),
                QStringLiteral("-filter_hw_device"),
                QStringLiteral("prism_vaapi")
            } + hardwareArgs;
        }
        if (job->usesHardware) {
            hardwareArgs << "-c:v" << encoder;
            if (usesVaapi) {
                hardwareArgs << "-vf" << "format=nv12,hwupload";
            }
            if (job->request.gpuType == GPUType::NVIDIA && !usesVaapi) {
                hardwareArgs << "-preset" << "p4" << "-cq" << "25";
            } else {
                hardwareArgs << "-b:v" << "4M";
            }
            hardwareArgs << "-c:a" << "aac" << "-b:a" << "192k";
        } else {
            hardwareArgs << "-c:v" << "libx265" << "-crf" << "25" << "-c:a" << "aac";
        }
        cpuArgs << "-c:v" << "libx265" << "-crf" << "25" << "-c:a" << "aac";
    } else if (format.startsWith("MKV")) {
        extension = ".mkv";
        hardwareArgs << "-c" << "copy";
        cpuArgs = hardwareArgs;
    } else if (format.startsWith("MP3")) {
        stem = input.completeBaseName() + "_audio";
        extension = ".mp3";
        hardwareArgs << "-vn" << "-c:a" << "libmp3lame" << "-b:a" << "320k";
        cpuArgs = hardwareArgs;
    } else if (format.startsWith("WAV")) {
        stem = input.completeBaseName() + "_audio";
        extension = ".wav";
        hardwareArgs << "-vn" << "-c:a" << "pcm_s16le";
        cpuArgs = hardwareArgs;
    } else {
        extension = ".webm";
        hardwareArgs << "-c:v" << "libvpx-vp9" << "-b:v" << "2M" << "-c:a" << "libopus";
        cpuArgs = hardwareArgs;
    }

    job->outputFile = uniqueOutputPath(outputDirectory, stem, extension);
    hardwareArgs << job->outputFile;
    cpuArgs << job->outputFile;
    job->hardwareArguments = hardwareArgs;
    job->cpuArguments = cpuArgs;
}
