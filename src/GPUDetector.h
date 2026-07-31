#ifndef GPUDETECTOR_H
#define GPUDETECTOR_H

#include <QObject>
#include <QString>

enum class GPUType {
    NVIDIA,
    AMD,
    INTEL,
    CPU_ONLY
};

class GPUDetector : public QObject {
    Q_OBJECT
public:
    explicit GPUDetector(QObject *parent = nullptr);

    // Roda a detecção de forma segura sem travar via QProcess
    void detect();
    
    GPUType getGPUType() const;
    QString getGPUName() const;
    QString getRecommendedCodec() const;
    bool hasHardwareAcceleration() const;

signals:
    void detectionCompleted(GPUType type, const QString &name, const QString &codec);

private:
    GPUType m_type = GPUType::CPU_ONLY;
    QString m_name = "CPU Multi-Thread Fallback";
    QString m_codec = "libx264";
};

#endif // GPUDETECTOR_H
