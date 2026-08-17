#include "GPUDetector.h"

#include "MediaToolResolver.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
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
struct DetectedHardware {
    QString encoder;
    QString device;
    GPUType type{GPUType::CPU_ONLY};
};

bool runProbe(QProcess &probe, const QString &program, const QStringList &arguments,
              QByteArray *output = nullptr)
{
    probe.setProcessChannelMode(QProcess::MergedChannels);
    probe.start(program, arguments);
    if (!probe.waitForStarted(1000)) {
        if (probe.state() != QProcess::NotRunning) {
            probe.kill();
            probe.waitForFinished(1000);
        }
        return false;
    }
    if (!probe.waitForFinished(4000)) {
        probe.kill();
        probe.waitForFinished(1000);
        return false;
    }
    if (output) {
        *output = probe.readAllStandardOutput();
    }
    return probe.exitStatus() == QProcess::NormalExit && probe.exitCode() == 0;
}

bool ffmpegListsEncoder(const QString &ffmpeg, const QString &encoder)
{
    QProcess probe;
    QByteArray output;
    if (!runProbe(probe, ffmpeg,
                  QStringList{QStringLiteral("-hide_banner"), QStringLiteral("-encoders")},
                  &output)) {
        return false;
    }
    return output.contains(encoder.toLatin1());
}

bool encoderWorks(const QString &ffmpeg, const QString &encoder, const QString &device = {})
{
    QStringList arguments{
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
    };
    if (encoder.endsWith(QStringLiteral("_vaapi"))) {
        if (device.isEmpty()) {
            return false;
        }
        arguments << QStringLiteral("-vaapi_device") << device;
    }
    arguments << QStringLiteral("-f") << QStringLiteral("lavfi")
        << QStringLiteral("-i");
    if (encoder.endsWith(QStringLiteral("_vaapi"))) {
        // VAAPI encoders require frames in a hardware surface. This also
        // prevents a false negative caused by FFmpeg's software test frame.
        arguments << QStringLiteral("color=size=16x16:rate=1,format=nv12,hwupload");
    } else {
        arguments << QStringLiteral("color=size=16x16:rate=1");
    }
    arguments << QStringLiteral("-frames:v") << QStringLiteral("1")
        << QStringLiteral("-c:v") << encoder
        << QStringLiteral("-f") << QStringLiteral("null") << QStringLiteral("-");
    QProcess probe;
    return runProbe(probe, ffmpeg, arguments);
}

QStringList vaapiRenderDevices()
{
    const QDir driDirectory(QStringLiteral("/dev/dri"));
    const QFileInfoList entries = driDirectory.entryInfoList(
        QStringList{QStringLiteral("renderD*")}, QDir::System | QDir::Readable, QDir::Name);
    QStringList devices;
    for (const QFileInfo &entry : entries) {
        if (entry.isReadable()) {
            devices.append(entry.absoluteFilePath());
        }
    }
    return devices;
}

GPUType vaapiTypeForDevice(const QString &device)
{
    const QString vendorPath = QStringLiteral("/sys/class/drm/%1/device/vendor")
        .arg(QFileInfo(device).fileName());
    QFile vendorFile(vendorPath);
    if (!vendorFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return GPUType::VAAPI;
    }
    const QByteArray vendor = vendorFile.readAll().trimmed().toLower();
    if (vendor == "0x1002") {
        return GPUType::AMD;
    }
    if (vendor == "0x8086") {
        return GPUType::INTEL;
    }
    if (vendor == "0x10de") {
        return GPUType::NVIDIA;
    }
    return GPUType::VAAPI;
}

GPUType typeForEncoder(const QString &encoder)
{
    if (encoder.endsWith(QStringLiteral("_nvenc"))) return GPUType::NVIDIA;
    if (encoder.endsWith(QStringLiteral("_amf"))) return GPUType::AMD;
    if (encoder.endsWith(QStringLiteral("_qsv"))) return GPUType::INTEL;
    return GPUType::VAAPI;
}

DetectedHardware findUsableHardwareEncoder()
{
    const QString ffmpeg = MediaToolResolver::resolve(MediaTool::Ffmpeg);
    if (ffmpeg.isEmpty()) {
        return {};
    }

    // A lista de encoders não basta: distribuições costumam compilar NVENC,
    // QSV e VAAPI mesmo em máquinas sem o driver correspondente. Este teste
    // renderiza um frame mínimo e só aceita o encoder que realmente inicializa.
    const QStringList candidates{
        QStringLiteral("h264_nvenc"),
        QStringLiteral("h264_amf"),
        QStringLiteral("h264_qsv")
    };
    for (const QString &candidate : candidates) {
        if (ffmpegListsEncoder(ffmpeg, candidate) && encoderWorks(ffmpeg, candidate)) {
            return {candidate, {}, typeForEncoder(candidate)};
        }
    }

    if (ffmpegListsEncoder(ffmpeg, QStringLiteral("h264_vaapi"))) {
        for (const QString &device : vaapiRenderDevices()) {
            if (encoderWorks(ffmpeg, QStringLiteral("h264_vaapi"), device)) {
                return {QStringLiteral("h264_vaapi"), device, vaapiTypeForDevice(device)};
            }
        }
    }
    return {};
}
#endif

}

void GPUDetector::detect()
{
    std::cout << "[GPUDetector] Sondando aceleradores de vídeo disponíveis...\n";

    m_type = GPUType::CPU_ONLY;
    m_name = "CPU Multi-Thread (aceleração não disponível)";
    m_codec = "libx264";
    m_device.clear();

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
    const DetectedHardware detected = findUsableHardwareEncoder();
    totalDump = detected.encoder.toStdString();
    if (!detected.encoder.isEmpty()) {
        m_type = detected.type;
        m_codec = detected.encoder.toStdString();
        m_device = detected.device.toStdString();
        switch (detected.type) {
        case GPUType::NVIDIA:
            m_name = detected.encoder.endsWith("_nvenc")
                ? "NVIDIA (NVENC disponível no FFmpeg)"
                : "NVIDIA (VAAPI disponível no FFmpeg)";
            break;
        case GPUType::AMD:
            m_name = detected.encoder.endsWith("_amf")
                ? "AMD (AMF disponível no FFmpeg)"
                : "AMD Radeon (VAAPI disponível no FFmpeg)";
            break;
        case GPUType::INTEL:
            m_name = detected.encoder.endsWith("_qsv")
                ? "Intel (Quick Sync disponível no FFmpeg)"
                : "Intel (VAAPI disponível no FFmpeg)";
            break;
        case GPUType::VAAPI:
            m_name = "GPU (VAAPI disponível no FFmpeg)";
            break;
        case GPUType::CPU_ONLY:
            break;
        }
        std::cout << "[GPUDetector] Encoder de hardware utilizável: " << totalDump;
        if (!m_device.empty()) {
            std::cout << " (dispositivo " << m_device << ")";
        }
        std::cout << "\n";
        return;
    }
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
std::string GPUDetector::getHardwareDevice() const { return m_device; }

bool GPUDetector::hasHardwareAcceleration() const
{
    return m_type != GPUType::CPU_ONLY;
}
