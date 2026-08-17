#ifndef GPUDETECTOR_H
#define GPUDETECTOR_H

#include <string>

enum class GPUType {
    NVIDIA,
    AMD,
    INTEL,
    VAAPI,
    CPU_ONLY
};

class GPUDetector {
public:
    GPUDetector() = default;

    // Sondagem nativa de hardware em C++ puro. O modo detalhado é usado pelo
    // diagnóstico de terminal e não altera a resolução normal do aplicativo.
    void detect(bool verbose = false);
    
    GPUType getGPUType() const;
    std::string getGPUName() const;
    std::string getRecommendedCodec() const;
    std::string getHardwareDevice() const;
    std::string getDiagnostic() const;
    bool hasHardwareAcceleration() const;

private:
    GPUType m_type = GPUType::CPU_ONLY;
    std::string m_name = "CPU Multi-Thread Fallback";
    std::string m_codec = "libx264";
    std::string m_device;
    std::string m_diagnostic;

};

#endif // GPUDETECTOR_H
