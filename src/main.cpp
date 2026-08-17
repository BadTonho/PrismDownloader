#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <iostream>

#include "GPUDetector.h"
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    bool diagnoseGpu = false;
    for (int index = 1; index < argc; ++index) {
        if (QString::fromLocal8Bit(argv[index]) == QStringLiteral("--diagnose-gpu")) {
            diagnoseGpu = true;
            break;
        }
    }

    if (diagnoseGpu) {
        QCoreApplication app(argc, argv);
        app.setOrganizationName(QStringLiteral("Tonho Studios"));
        app.setApplicationName(QStringLiteral("PrismDownloader"));

        GPUDetector detector;
        detector.detect(true);
        std::cout << "[GPUDetector] Resultado final: "
                  << (detector.hasHardwareAcceleration() ? "aceleração disponível" : "fallback CPU")
                  << "\n";
        std::cout << "[GPUDetector] Nome: " << detector.getGPUName() << "\n";
        std::cout << "[GPUDetector] Codec: " << detector.getRecommendedCodec() << "\n";
        std::cout << "[GPUDetector] Dispositivo: " << detector.getHardwareDevice() << "\n";
        if (!detector.getDiagnostic().empty()) {
            std::cout << "[GPUDetector] Diagnóstico: " << detector.getDiagnostic() << "\n";
        }
        return detector.hasHardwareAcceleration() ? 0 : 1;
    }

    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Tonho Studios"));
    app.setApplicationName(QStringLiteral("PrismDownloader"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/prism-downloader.png")));

    MainWindow window;
    window.show();

    return app.exec();
}
