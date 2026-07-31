#include "DownloadEngine.h"
#include <QDebug>
#include <QRegularExpression>

DownloadEngine::DownloadEngine(QObject *parent) 
    : QObject(parent), m_process(new QProcess(this)), m_gpuDetector(new GPUDetector(this)) {
    
    connect(m_process, &QProcess::readyReadStandardOutput, this, &DownloadEngine::onProcessReadyRead);
    connect(m_process, &QProcess::readyReadStandardError, this, &DownloadEngine::onProcessReadyRead);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), 
            this, &DownloadEngine::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &DownloadEngine::onProcessError);
}

DownloadEngine::~DownloadEngine() {
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

void DownloadEngine::initialize() {
    qDebug() << "[DownloadEngine] Inicializando motor C++ e verificando GPU...";
    m_gpuDetector->detect();
}

GPUDetector* DownloadEngine::gpuDetector() {
    return m_gpuDetector;
}

bool DownloadEngine::isDownloading() const {
    return m_isRunning;
}

void DownloadEngine::startDownload(const QString &url, const QString &quality, const QString &timeRange) {
    if (m_isRunning) {
        qWarning() << "[DownloadEngine] Já existe um download em progresso!";
        return;
    }

    m_currentItem = MediaItem{url, "Analisando...", quality, "0 MB/s", "00:00", 0.0, DownloadStatus::Queued};
    m_isRunning = true;

    QStringList args;
    
    // Configurações base otimizadas do yt-dlp para parseamento fluido
    args << "--progress" << "--newline" << "--no-mtime";

    // Suporte ao recorte de tempo
    if (!timeRange.isEmpty()) {
        qDebug() << "✂️ [DownloadEngine] Aplicando recorte inteligente de tempo:" << timeRange;
        args << "--download-sections" << QString("*%1").arg(timeRange);
    }

    // Áudio ou Vídeo com Aceleração/Stream Copy
    if (m_currentItem.isAudioOnly()) {
        args << "-x" << "--audio-format" << "mp3" << "--audio-quality" << "0";
        emit statusChanged(DownloadStatus::ConvertingGPU, "Extraindo Áudio Puro...");
    } else {
        args << "-f" << "bestvideo+bestaudio/best" << "--merge-output-format" << "mp4";
        
        // Aplicação dos codecs detectadas da GPU (ex: h264_nvenc)
        if (m_gpuDetector->hasHardwareAcceleration()) {
            qDebug() << "⚡ [DownloadEngine] Conectando acelerador na linha de processamento:" << m_gpuDetector->getRecommendedCodec();
            args << "--postprocessor-args" << QString("ffmpeg:-vcodec %1").arg(m_gpuDetector->getRecommendedCodec());
        }
        
        emit statusChanged(DownloadStatus::Downloading, "Baixando e Juntando com Aceleração...");
    }

    args << url;

    qDebug() << "[DownloadEngine] Disparando QProcess silencioso com yt-dlp:" << args.join(" ");
    m_process->start("yt-dlp", args);
}

void DownloadEngine::cancelCurrent() {
    if (m_isRunning && m_process->state() != QProcess::NotRunning) {
        qDebug() << "[DownloadEngine] Cancelado pelo usuário.";
        m_process->kill();
        m_isRunning = false;
        m_currentItem.status = DownloadStatus::Cancelled;
        emit statusChanged(DownloadStatus::Cancelled, "Download Cancelado");
    }
}

void DownloadEngine::onProcessReadyRead() {
    QString output = QString::fromUtf8(m_process->readAllStandardOutput()) + 
                     QString::fromUtf8(m_process->readAllStandardError());
    
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        parseYtDlpOutput(line.trimmed());
    }
}

void DownloadEngine::parseYtDlpOutput(const QString &line) {
    qDebug() << "[Output]" << line;

    QRegularExpression rx("\\[download\\]\\s+([0-9.]+)%.*at\\s+([0-9a-zA-Z./]+)\\s+ETA\\s+([0-9:]+)");
    QRegularExpressionMatch match = rx.match(line);
    
    if (match.hasMatch()) {
        double percent = match.captured(1).toDouble();
        QString speed = match.captured(2);
        QString eta = match.captured(3);
        
        m_currentItem.progress = percent;
        m_currentItem.speed = speed;
        m_currentItem.eta = eta;
        
        emit progressUpdated(percent, speed, eta);
    }
}

void DownloadEngine::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    m_isRunning = false;
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        m_currentItem.status = DownloadStatus::Completed;
        emit statusChanged(DownloadStatus::Completed, "Download Concluído com Sucesso!");
    } else {
        m_currentItem.status = DownloadStatus::Error;
        emit statusChanged(DownloadStatus::Error, "Falha durante o processo de download.");
    }
}

void DownloadEngine::onProcessError(QProcess::ProcessError error) {
    m_isRunning = false;
    qDebug() << "[DownloadEngine] Erro no processo:" << error;
    emit statusChanged(DownloadStatus::Error, "Não foi possível iniciar o binário do yt-dlp ou ffmpeg.");
}
