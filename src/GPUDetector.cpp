#include "GPUDetector.h"
#include <QProcess>
#include <QDebug>
#include <QRegularExpression>

GPUDetector::GPUDetector(QObject *parent) : QObject(parent) {}

void GPUDetector::detect() {
    qDebug() << "[GPUDetector] Iniciando sondagem limpa em memória via FFmpeg...";

    QProcess ffmpegProcess;
    ffmpegProcess.start("ffmpeg", QStringList() << "-hwaccels");
    
    bool finished = ffmpegProcess.waitForFinished(3000);
    QString output;
    
    if (finished && ffmpegProcess.exitStatus() == QProcess::NormalExit) {
        output = QString::fromUtf8(ffmpegProcess.readAllStandardOutput());
        qDebug() << "[GPUDetector] Saída do ffmpeg -hwaccels:\n" << output;
    } else {
        qDebug() << "[GPUDetector] FFmpeg não respondeu em -hwaccels ou não está no PATH. Testando consulta alternativa...";
        ffmpegProcess.start("ffmpeg", QStringList() << "-encoders");
        if (ffmpegProcess.waitForFinished(3000)) {
            output = QString::fromUtf8(ffmpegProcess.readAllStandardOutput()) + 
                     QString::fromUtf8(ffmpegProcess.readAllStandardError());
        }
    }

    // Identificação de hardware prioritaria via saída do motor de vídeo
    if (output.contains("cuda", Qt::CaseInsensitive) || 
        output.contains("nvenc", Qt::CaseInsensitive) || 
        output.contains("nvdec", Qt::CaseInsensitive)) {
        
        m_type = GPUType::NVIDIA;
        m_name = "NVIDIA GeForce (NVENC Acelerado)";
        m_codec = "h264_nvenc";
        qDebug() << "⚡ [SUCESSO] NVIDIA detectada (Compatível com GTX 1660 Super)!";
        
    } else if (output.contains("amf", Qt::CaseInsensitive) || output.contains("d3d11va_amf", Qt::CaseInsensitive)) {
        m_type = GPUType::AMD;
        m_name = "AMD Radeon (AMF Acelerado)";
        m_codec = "h264_amf";
        qDebug() << "⚡ [SUCESSO] AMD Radeon detectada!";
        
    } else if (output.contains("qsv", Qt::CaseInsensitive)) {
        m_type = GPUType::INTEL;
        m_name = "Intel QuickSync (QSV Acelerado)";
        m_codec = "h264_qsv";
        qDebug() << "⚡ [SUCESSO] Intel QuickSync detectado!";
        
    } else {
        m_type = GPUType::CPU_ONLY;
        m_name = "CPU Multi-Thread (Sem aceleração dedicada identificada pelo FFmpeg no terminal)";
        m_codec = "libx264";
        qDebug() << "ℹ️ [INFO] Usando modo fallback limpo para CPU Multi-Thread.";
    }

    emit detectionCompleted(m_type, m_name, m_codec);
}

GPUType GPUDetector::getGPUType() const { return m_type; }
QString GPUDetector::getGPUName() const { return m_name; }
QString GPUDetector::getRecommendedCodec() const { return m_codec; }

bool GPUDetector::hasHardwareAcceleration() const {
    return m_type != GPUType::CPU_ONLY;
}
