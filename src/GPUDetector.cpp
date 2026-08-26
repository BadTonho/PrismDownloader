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

#ifdef _WIN32
bool ffmpegEncoderWorks(const QString &ffmpeg, const QString &encoder, QString *failure = nullptr)
{
    if (ffmpeg.isEmpty() || !QFileInfo(ffmpeg).isFile()) {
        if (failure) {
            *failure = QStringLiteral("FFmpeg não foi localizado no caminho esperado.");
        }
        return false;
    }

    QProcess probe;
    probe.setProcessChannelMode(QProcess::MergedChannels);
    probe.start(ffmpeg, {
        QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-f"), QStringLiteral("lavfi"), QStringLiteral("-i"),
        // Alguns encoders de hardware rejeitam dimensões menores que 144p.
        QStringLiteral("color=size=320x240:rate=1"), QStringLiteral("-frames:v"), QStringLiteral("1"),
        QStringLiteral("-c:v"), encoder, QStringLiteral("-f"), QStringLiteral("null"), QStringLiteral("-")
    });
    if (!probe.waitForStarted(1000) || !probe.waitForFinished(5000)) {
        if (failure) {
            *failure = probe.errorString().trimmed();
            if (failure->isEmpty()) {
                *failure = QStringLiteral("tempo limite ao executar o teste do encoder.");
            }
        }
        if (probe.state() != QProcess::NotRunning) {
            probe.kill();
            probe.waitForFinished(1000);
        }
        return false;
    }
    const QByteArray output = probe.readAll();
    const bool success = probe.exitStatus() == QProcess::NormalExit && probe.exitCode() == 0;
    if (!success && failure) {
        *failure = QString::fromUtf8(output).simplified();
        if (failure->isEmpty()) {
            *failure = QStringLiteral("FFmpeg encerrou com código %1.").arg(probe.exitCode());
        }
        if (failure->size() > 512) {
            failure->truncate(512);
            failure->append(QStringLiteral("..."));
        }
    }
    return success;
}
#endif

#ifdef Q_OS_LINUX
struct DetectedHardware {
    QString encoder;
    QString device;
    GPUType type{GPUType::CPU_ONLY};
    QString diagnostic;
};

bool runProbe(QProcess &probe, const QString &program, const QStringList &arguments,
              QByteArray *output = nullptr, QString *failure = nullptr)
{
    probe.setProcessChannelMode(QProcess::MergedChannels);
    probe.start(program, arguments);
    if (!probe.waitForStarted(1000)) {
        if (failure) *failure = probe.errorString();
        if (probe.state() != QProcess::NotRunning) {
            probe.kill();
            probe.waitForFinished(1000);
        }
        return false;
    }
    if (!probe.waitForFinished(4000)) {
        if (failure) *failure = QStringLiteral("tempo limite de 4 segundos excedido");
        probe.kill();
        probe.waitForFinished(1000);
        if (output) *output = probe.readAllStandardOutput();
        return false;
    }
    if (output) {
        *output = probe.readAllStandardOutput();
    }
    const bool success = probe.exitStatus() == QProcess::NormalExit && probe.exitCode() == 0;
    if (!success && failure) {
        *failure = QStringLiteral("saída %1, código %2")
            .arg(probe.exitStatus() == QProcess::NormalExit ? "normal" : "anormal")
            .arg(probe.exitCode());
    }
    return success;
}

bool ffmpegListsEncoder(const QByteArray &output, const QString &encoder, bool verbose)
{
    const bool listed = output.contains(encoder.toLatin1());
    if (verbose) {
        std::cout << "[GPUDetector] -encoders: " << encoder.toStdString()
                  << (listed ? " listado" : " não listado") << "\n";
    }
    return listed;
}

void printProbeResult(const QString &encoder, const QString &device,
                      const QStringList &arguments, bool success,
                      const QString &failure, const QByteArray &output)
{
    std::cout << "[GPUDetector] Teste " << encoder.toStdString();
    if (!device.isEmpty()) {
        std::cout << " em " << device.toStdString();
    }
    std::cout << ": " << (success ? "OK" : "FALHOU") << "\n";
    std::cout << "[GPUDetector] Comando: " << arguments.join(QLatin1Char(' ')).toStdString() << "\n";
    if (!failure.isEmpty()) {
        std::cout << "[GPUDetector] Motivo: " << failure.toStdString() << "\n";
    }
    if (!output.isEmpty()) {
        std::cout << "[GPUDetector] Saída do FFmpeg:\n" << output.toStdString() << "\n";
    }
}

bool encoderWorks(const QString &ffmpeg, const QString &encoder, const QString &device,
                  bool verbose)
{
    QStringList arguments{
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
    };
    if (encoder.endsWith(QStringLiteral("_vaapi"))) {
        if (device.isEmpty()) {
            return false;
        }
        // Explicitly bind hwupload to the same DRM device. On FFmpeg 6.x,
        // merely passing -vaapi_device may leave the filter without a device.
        arguments << QStringLiteral("-init_hw_device")
                  << QStringLiteral("vaapi=prism_vaapi:%1").arg(device)
                  << QStringLiteral("-filter_hw_device")
                  << QStringLiteral("prism_vaapi");
    }
    arguments << QStringLiteral("-f") << QStringLiteral("lavfi")
        << QStringLiteral("-i")
        // Keep this input graph software-only. The lavfi demuxer is created
        // before -filter_hw_device is associated with an output graph, so an
        // hwupload placed here has no hardware-device reference.
        << QStringLiteral("color=size=320x240:rate=1")
        << QStringLiteral("-frames:v") << QStringLiteral("1");
    if (encoder.endsWith(QStringLiteral("_vaapi"))) {
        // Attach hwupload to the output filter graph, after the named VAAPI
        // device has been selected. This mirrors ConversionManager and avoids
        // rejecting a working render node during the startup probe.
        arguments << QStringLiteral("-vf") << QStringLiteral("format=nv12,hwupload");
    }
    arguments
        << QStringLiteral("-c:v") << encoder
        << QStringLiteral("-f") << QStringLiteral("null") << QStringLiteral("-");
    QProcess probe;
    QByteArray output;
    QString failure;
    const bool success = runProbe(probe, ffmpeg, arguments, &output, &failure);
    if (verbose) {
        printProbeResult(encoder, device, arguments, success, failure, output);
    }
    return success;
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

const char *gpuTypeName(GPUType type)
{
    switch (type) {
    case GPUType::NVIDIA: return "NVIDIA";
    case GPUType::AMD: return "AMD";
    case GPUType::INTEL: return "Intel";
    case GPUType::VAAPI: return "VAAPI";
    case GPUType::CPU_ONLY: return "CPU";
    }
    return "desconhecida";
}

DetectedHardware findUsableHardwareEncoder(bool verbose)
{
    const QString ffmpeg = MediaToolResolver::resolve(MediaTool::Ffmpeg);
    if (ffmpeg.isEmpty()) {
        return {};
    }
    if (verbose) {
        std::cout << "[GPUDetector] FFmpeg selecionado: " << ffmpeg.toStdString() << "\n";
    }

    QProcess encoderProbe;
    QByteArray encoderList;
    QString encoderFailure;
    if (!runProbe(encoderProbe, ffmpeg,
                  QStringList{QStringLiteral("-hide_banner"), QStringLiteral("-encoders")},
                  &encoderList, &encoderFailure)) {
        if (verbose) {
            std::cout << "[GPUDetector] Falha ao consultar -encoders: "
                      << encoderFailure.toStdString() << "\n";
        }
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
        if (ffmpegListsEncoder(encoderList, candidate, verbose)
            && encoderWorks(ffmpeg, candidate, {}, verbose)) {
            DetectedHardware usable;
            usable.encoder = candidate;
            usable.type = typeForEncoder(candidate);
            return usable;
        }
    }

    if (ffmpegListsEncoder(encoderList, QStringLiteral("h264_vaapi"), verbose)) {
        const QStringList devices = vaapiRenderDevices();
        if (verbose) {
            std::cout << "[GPUDetector] Dispositivos VAAPI encontrados: "
                      << devices.size() << "\n";
            for (const QString &device : devices) {
                std::cout << "  -> " << device.toStdString() << " (vendor "
                          << gpuTypeName(vaapiTypeForDevice(device)) << ")\n";
            }
        }
        if (devices.isEmpty()) {
            DetectedHardware unavailable;
            unavailable.diagnostic = QStringLiteral(
                "h264_vaapi está listado, mas nenhum /dev/dri/renderD* acessível; "
                "verifique mesa-va-drivers e os grupos render/video.");
            return unavailable;
        }
        for (const QString &device : devices) {
            if (encoderWorks(ffmpeg, QStringLiteral("h264_vaapi"), device, verbose)) {
                DetectedHardware usable;
                usable.encoder = QStringLiteral("h264_vaapi");
                usable.device = device;
                usable.type = vaapiTypeForDevice(device);
                return usable;
            }
        }
        DetectedHardware unavailable;
        unavailable.diagnostic = QStringLiteral(
            "h264_vaapi está listado, mas o driver não conseguiu inicializar nenhum "
            "dispositivo DRM acessível.");
        return unavailable;
    }
    return {};
}
#endif

} // namespace

void GPUDetector::detect(bool verbose)
{
    std::cout << "[GPUDetector] Sondando aceleradores de vídeo disponíveis...\n";

    m_type = GPUType::CPU_ONLY;
    m_name = "CPU Multi-Thread (aceleração não disponível)";
    m_codec = "libx264";
    m_device.clear();
    m_diagnostic.clear();

    std::string totalDump;
#ifdef _WIN32
    static_cast<void>(verbose);
    DISPLAY_DEVICEA displayDevice{};
    DWORD deviceNumber = 0;
    while (true) {
        ZeroMemory(&displayDevice, sizeof(displayDevice));
        displayDevice.cb = sizeof(displayDevice);
        if (!EnumDisplayDevicesA(nullptr, deviceNumber, &displayDevice, 0)) {
            break;
        }
        totalDump += displayDevice.DeviceString;
        totalDump += " ";
        ++deviceNumber;
    }
    const std::string windowsHardware = lowerCase(totalDump);
    const QString windowsFfmpeg = MediaToolResolver::resolve(MediaTool::Ffmpeg);

    struct WindowsCandidate {
        QString encoder;
        GPUType type;
        const char *name;
    };
    QList<WindowsCandidate> candidates;
    if (windowsHardware.find("nvidia") != std::string::npos
        || windowsHardware.find("geforce") != std::string::npos) {
        candidates.append({QStringLiteral("h264_nvenc"), GPUType::NVIDIA, "NVIDIA (NVENC disponível no FFmpeg)"});
    }
    if (windowsHardware.find("radeon") != std::string::npos
        || windowsHardware.find("amd") != std::string::npos) {
        candidates.append({QStringLiteral("h264_amf"), GPUType::AMD, "AMD (AMF disponível no FFmpeg)"});
    }
    if (windowsHardware.find("intel") != std::string::npos) {
        candidates.append({QStringLiteral("h264_qsv"), GPUType::INTEL, "Intel (Quick Sync disponível no FFmpeg)"});
    }

    if (candidates.isEmpty()) {
        m_diagnostic = "Nenhum adaptador NVIDIA, AMD ou Intel compatível foi identificado.";
        std::cout << "[GPUDetector] Nenhum encoder de hardware passou no teste real.\n";
        return;
    }
    if (windowsFfmpeg.isEmpty() || !QFileInfo(windowsFfmpeg).isFile()) {
        m_diagnostic = "O adaptador foi identificado, mas o FFmpeg não foi localizado.";
        std::cout << "[GPUDetector] Nenhum encoder de hardware passou no teste real.\n";
        return;
    }

    QStringList failureReasons;
    for (const WindowsCandidate &candidate : candidates) {
        QString encoderFailure;
        if (ffmpegEncoderWorks(windowsFfmpeg, candidate.encoder, &encoderFailure)) {
            m_type = candidate.type;
            m_codec = candidate.encoder.toStdString();
            m_name = candidate.name;
            std::cout << "[GPUDetector] Encoder de hardware validado: "
                      << candidate.encoder.toStdString() << "\n";
            return;
        }
        QString reason = QStringLiteral("Encoder %1 falhou").arg(candidate.encoder);
        if (!encoderFailure.isEmpty()) {
            reason += QStringLiteral(" (%1)").arg(encoderFailure);
        }
        failureReasons.append(reason);
    }

    m_diagnostic = failureReasons.join(QStringLiteral("; ")).toStdString();
    std::cout << "[GPUDetector] Nenhum encoder de hardware passou no teste real.\n";
    return;
#elif defined(Q_OS_LINUX)
    const DetectedHardware detected = findUsableHardwareEncoder(verbose);
    totalDump = detected.encoder.toStdString();
    m_diagnostic = detected.diagnostic.toStdString();
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
std::string GPUDetector::getDiagnostic() const { return m_diagnostic; }

bool GPUDetector::hasHardwareAcceleration() const
{
    return m_type != GPUType::CPU_ONLY;
}
