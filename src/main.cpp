#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include "DownloadEngine.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    qDebug() << "========================================================";
    qDebug() << "   🚀 NeoVDownloader Core (C++ & Qt Engine v1.0)     ";
    qDebug() << "========================================================";

    DownloadEngine engine;
    
    // Inicializa o motor e realiza a sondagem de GPU nativa
    engine.initialize();

    GPUDetector *gpu = engine.gpuDetector();
    qDebug() << "\n[RESULTADO DA SONDAGEM DE HARDWARE]:";
    qDebug() << "  -> Placa de Vídeo Identificada:" << gpu->getGPUName();
    qDebug() << "  -> Codec de Aceleração:" << gpu->getRecommendedCodec();
    qDebug() << "  -> Aceleração GPU Ativa:" << (gpu->hasHardwareAcceleration() ? "SIM ⚡ (Pronto para alta performance)" : "NÃO (Fallback CPU Multi-Thread)");

    qDebug() << "\n✅ Teste da Etapa 2 finalizado! Motor C++ estruturado e afiado para acoplarmos à Janela Gráfica (Etapa 3).";

    // Encerra graciosamente após 500ms
    QTimer::singleShot(500, &app, &QCoreApplication::quit);

    return app.exec();
}
