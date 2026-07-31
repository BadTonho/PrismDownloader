#include <iostream>
#include "DownloadEngine.h"

int main() {
    std::cout << "========================================================\n";
    std::cout << "   🚀 NeoVDownloader Core (C++17 Puro & Nativo v1.0)   \n";
    std::cout << "========================================================\n\n";

    DownloadEngine engine;
    
    // Teste dos Callbacks C++ limpos
    engine.setStatusCallback([](DownloadStatus status, const std::string &msg) {
        std::cout << "  >>> [STATUS] " << msg << "\n";
    });

    // Inicializa a detecção nativa de GPU sem dependências externas
    engine.initialize();

    GPUDetector *gpu = engine.gpuDetector();
    std::cout << "\n========================================================\n";
    std::cout << "       🎯 RESULTADO DA SONDAGEM DE HARDWARE NATIVO      \n";
    std::cout << "========================================================\n";
    std::cout << "  -> Placa Detectada:    " << gpu->getGPUName() << "\n";
    std::cout << "  -> Codec Selecionado:  " << gpu->getRecommendedCodec() << "\n";
    std::cout << "  -> Aceleração Ativada: " << (gpu->hasHardwareAcceleration() ? "SIM ⚡ (Hardware Turbo)" : "NÃO (CPU Fallback)") << "\n";
    std::cout << "========================================================\n\n";

    std::cout << "✅ SUCESSO ABSOLUTO DA ETAPA 2! Motor C++17 compilado e operante!\n";

    return 0;
}
