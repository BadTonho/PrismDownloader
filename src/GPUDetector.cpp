#include "GPUDetector.h"
#include <iostream>
#include <array>
#include <memory>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

std::string GPUDetector::execCommand(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    
    std::unique_ptr<FILE, decltype(&PCLOSE)> pipe(POPEN(cmd, "r"), PCLOSE);
    if (!pipe) {
        return "";
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

void GPUDetector::detect() {
    std::cout << "[GPUDetector] Iniciando sondagem nativa de hardware em C++ puro...\n";

    std::string output = execCommand("ffmpeg -hwaccels 2>&1");
    // No Windows 11 o comando wmic foi descontinuado pela Microsoft, então usamos o padrão moderno do PowerShell CIM:
    std::string winGPU = execCommand("powershell -NoProfile -Command \"(Get-CimInstance -ClassName Win32_VideoController).Name\" 2>&1");
    
    std::string totalDump = output + " \n " + winGPU;
    
    std::cout << "[GPUDetector] Resposta Bruta da Sondagem de Hardware (Placa de Vídeo Localizada):\n  -> " << winGPU << "\n";

    std::string lower = totalDump;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Prioridade por fabricante de hardware
    if (lower.find("cuda") != std::string::npos || 
        lower.find("nvenc") != std::string::npos || 
        lower.find("nvidia") != std::string::npos ||
        lower.find("geforce") != std::string::npos ||
        lower.find("1660") != std::string::npos) {
        
        m_type = GPUType::NVIDIA;
        m_name = "NVIDIA GeForce (NVENC Hardware Accelerated - GTX Compatível)";
        m_codec = "h264_nvenc";
        std::cout << "⚡ [SUCESSO] Hardware NVIDIA Detectado! Otimizado para GTX 1660 Super!\n";
        
    } else if (lower.find("amf") != std::string::npos || lower.find("radeon") != std::string::npos || lower.find("amd") != std::string::npos) {
        m_type = GPUType::AMD;
        m_name = "AMD Radeon (AMF Hardware Accelerated)";
        m_codec = "h264_amf";
        std::cout << "⚡ [SUCESSO] Hardware AMD Radeon Detectado!\n";
        
    } else if (lower.find("qsv") != std::string::npos || lower.find("intel") != std::string::npos) {
        m_type = GPUType::INTEL;
        m_name = "Intel HD/Iris/Arc (QuickSync QSV Accelerated)";
        m_codec = "h264_qsv";
        std::cout << "⚡ [SUCESSO] Hardware Intel QuickSync Detectado!\n";
        
    } else {
        m_type = GPUType::CPU_ONLY;
        m_name = "CPU Multi-Thread (Fallback limpo sem GPU dedicada identificada)";
        m_codec = "libx264";
        std::cout << "ℹ️ [INFO] Modo fallback otimizado de CPU selecionado.\n";
    }
}

GPUType GPUDetector::getGPUType() const { return m_type; }
std::string GPUDetector::getGPUName() const { return m_name; }
std::string GPUDetector::getRecommendedCodec() const { return m_codec; }

bool GPUDetector::hasHardwareAcceleration() const {
    return m_type != GPUType::CPU_ONLY;
}
