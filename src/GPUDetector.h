#ifndef GPUDETECTOR_H
#define GPUDETECTOR_H

#include <string>

enum class GPUType {
    NVIDIA,
    AMD,
    INTEL,
    CPU_ONLY
};

class GPUDetector {
public:
    GPUDetector() = default;

    // Sondagem nativa de hardware em C++ puro
    void detect();
    
    GPUType getGPUType() const;
    std::string getGPUName() const;
    std::string getRecommendedCodec() const;
    bool hasHardwareAcceleration() const;

private:
    GPUType m_type = GPUType::CPU_ONLY;
    std::string m_name = "CPU Multi-Thread Fallback";
    std::string m_codec = "libx264";

};

#endif // GPUDETECTOR_H
