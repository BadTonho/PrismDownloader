#include "GPUDetector.h"

#include "MediaToolResolver.h"

#include <QProcess>
#include <QStringList>

#include <algorithm>
#include <cctype>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

std::string lowerCase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

#ifdef Q_OS_LINUX
bool encoderWorks(const QString &ffmpeg, const QString &encoder)
{
    QProcess probe;
    probe.start(ffmpeg, QStringList{
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-f"), QStringLiteral("lavfi"),
        QStringLiteral("-i"), QStringLiteral("color=size=16x16:rate=1"),
        QStringLiteral("-frames:v"), QStringLiteral("1"),
        QStringLiteral("-c:v"), encoder,
        QStringLiteral("-f"), QStringLiteral("null"), QStringLiteral("-")
    });
    if (!probe.waitForStarted(1000)) {
        if (probe.state() != QProcess::NotRunning) {
            probe.kill();
            probe.waitForFinished(1000);
        }
        return {};
    }
    if (!probe.waitForFinished(4000)) {
        probe.kill();
        probe.waitForFinished(1000);
        return {};
    }
    if (probe.exitStatus() != QProcess::NormalExit || probe.exitCode() != 0) {
        return false;
    }
    return true;
}

QString findUsableHardwareEncoder()
{
    const QString ffmpeg = MediaToolResolver::resolve(MediaTool::Ffmpeg);
    if (ffmpeg.isEmpty()) {
        return {};
    }

    // A lista de encoders não basta: distribuições costumam compilar NVENC,
    // QSV e AMF mesmo em máquinas sem o driver correspondente. Este teste
    // renderiza um frame mínimo e só aceita o encoder que realmente inicializa.
    const QStringList candidates{
        QStringLiteral("h264_nvenc"),
        QStringLiteral("h264_amf"),
        QStringLiteral("h264_qsv")
    };
    for (const QString &candidate : candidates) {
        if (encoderWorks(ffmpeg, candidate)) {
            return candidate;
        }
    }
    return {};
}
#endif

}

void GPUDetector::detect()
{
    std::cout << "[GPUDetector] Sondando aceleradores de vídeo disponíveis...\n";

    std::string totalDump;
#ifdef _WIN32
    DISPLAY_DEVICEA displayDevice{};
    displayDevice.cb = sizeof(displayDevice);
    DWORD deviceNumber = 0;
    while (EnumDisplayDevicesA(nullptr, deviceNumber, &displayDevice, 0)) {
        totalDump += displayDevice.DeviceString;
        totalDump += " ";
        ++deviceNumber;
    }
#elif defined(Q_OS_LINUX)
    totalDump = findUsableHardwareEncoder().toStdString();
#endif

    std::cout << "[GPUDetector] Capacidades encontradas:\n  -> " << totalDump << "\n";

    const std::string lower = lowerCase(totalDump);
    if (lower.find("h264_nvenc") != std::string::npos
        || lower.find("hevc_nvenc") != std::string::npos
        || lower.find("nvidia") != std::string::npos
        || lower.find("geforce") != std::string::npos) {
        m_type = GPUType::NVIDIA;
        m_name = "NVIDIA (NVENC disponível no FFmpeg)";
        m_codec = "h264_nvenc";
        std::cout << "[GPUDetector] Encoder NVIDIA NVENC disponível.\n";
    } else if (lower.find("h264_amf") != std::string::npos
               || lower.find("hevc_amf") != std::string::npos
               || lower.find("radeon") != std::string::npos
               || lower.find("amd") != std::string::npos) {
        m_type = GPUType::AMD;
        m_name = "AMD (AMF disponível no FFmpeg)";
        m_codec = "h264_amf";
        std::cout << "[GPUDetector] Encoder AMD AMF disponível.\n";
    } else if (lower.find("h264_qsv") != std::string::npos
               || lower.find("hevc_qsv") != std::string::npos
               || lower.find("intel") != std::string::npos) {
        m_type = GPUType::INTEL;
        m_name = "Intel (Quick Sync disponível no FFmpeg)";
        m_codec = "h264_qsv";
        std::cout << "[GPUDetector] Encoder Intel Quick Sync disponível.\n";
    } else {
        m_type = GPUType::CPU_ONLY;
        m_name = "CPU Multi-Thread (aceleração não disponível)";
        m_codec = "libx264";
        std::cout << "[GPUDetector] Usando fallback de CPU.\n";
    }
}

GPUType GPUDetector::getGPUType() const { return m_type; }
std::string GPUDetector::getGPUName() const { return m_name; }
std::string GPUDetector::getRecommendedCodec() const { return m_codec; }

bool GPUDetector::hasHardwareAcceleration() const
{
    return m_type != GPUType::CPU_ONLY;
}
