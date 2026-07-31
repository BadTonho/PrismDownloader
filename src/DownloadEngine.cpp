#include "DownloadEngine.h"
#include <iostream>
#include <sstream>
#include <array>
#include <regex>

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

DownloadEngine::DownloadEngine() {}

DownloadEngine::~DownloadEngine() {
    cancelCurrent();
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

void DownloadEngine::initialize() {
    std::cout << "[DownloadEngine] Inicializando Motor Nativo C++17...\n";
    m_gpuDetector.detect();
}

GPUDetector* DownloadEngine::gpuDetector() {
    return &m_gpuDetector;
}

bool DownloadEngine::isDownloading() const {
    return m_isRunning.load();
}

void DownloadEngine::setProgressCallback(std::function<void(double, const std::string&, const std::string&)> cb) {
    m_onProgress = cb;
}

void DownloadEngine::setStatusCallback(std::function<void(DownloadStatus, const std::string&)> cb) {
    m_onStatus = cb;
}

void DownloadEngine::startDownload(const std::string &url, const std::string &quality, const std::string &timeRange) {
    if (m_isRunning.load()) {
        std::cerr << "[DownloadEngine] ERRO: Download já está em progresso!\n";
        return;
    }

    m_currentItem = MediaItem{url, "Analisando...", quality, "0 MB/s", "00:00", 0.0, DownloadStatus::Queued};
    m_isRunning.store(true);

    std::ostringstream cmd;
    cmd << "yt-dlp --progress --newline --no-mtime ";

    if (!timeRange.empty()) {
        std::cout << "✂️ [DownloadEngine] Aplicando recorte inteligente de tempo: " << timeRange << "\n";
        cmd << "--download-sections \"*" << timeRange << "\" ";
    }

    if (m_currentItem.isAudioOnly()) {
        cmd << "-x --audio-format mp3 --audio-quality 0 ";
        if (m_onStatus) m_onStatus(DownloadStatus::ConvertingGPU, "Extraindo Áudio Puro em alta velocidade...");
    } else {
        cmd << "-f \"bestvideo+bestaudio/best\" --merge-output-format mp4 ";
        if (m_gpuDetector.hasHardwareAcceleration()) {
            std::cout << "⚡ [DownloadEngine] Ativando processamento na placa: " << m_gpuDetector.getRecommendedCodec() << "\n";
            cmd << "--postprocessor-args \"ffmpeg:-vcodec " << m_gpuDetector.getRecommendedCodec() << "\" ";
        }
        if (m_onStatus) m_onStatus(DownloadStatus::Downloading, "Baixando e Juntando streams em alta velocidade...");
    }

    cmd << "\"" << url << "\" 2>&1";

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    // Disparamos thread separada nativa em C++
    m_workerThread = std::thread(&DownloadEngine::workerLoop, this, cmd.str());
}

void DownloadEngine::cancelCurrent() {
    if (m_isRunning.load()) {
        std::cout << "[DownloadEngine] Cancelando operação...\n";
        m_isRunning.store(false);
        m_currentItem.status = DownloadStatus::Cancelled;
        if (m_onStatus) m_onStatus(DownloadStatus::Cancelled, "Download Cancelado.");
    }
}

void DownloadEngine::workerLoop(const std::string &command) {
    std::cout << "[DownloadEngine Worker] Executando comando nativo: " << command << "\n";
    std::array<char, 256> buffer;
    
    std::unique_ptr<FILE, decltype(&PCLOSE)> pipe(POPEN(command.c_str(), "r"), PCLOSE);
    if (!pipe) {
        std::cerr << "[DownloadEngine] Falha ao abrir pipe com yt-dlp.\n";
        m_isRunning.store(false);
        if (m_onStatus) m_onStatus(DownloadStatus::Error, "Falha ao acionar binários do yt-dlp/ffmpeg.");
        return;
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr && m_isRunning.load()) {
        std::string line(buffer.data());
        parseYtDlpOutput(line);
    }

    m_isRunning.store(false);
    if (m_currentItem.status != DownloadStatus::Cancelled) {
        m_currentItem.status = DownloadStatus::Completed;
        if (m_onStatus) m_onStatus(DownloadStatus::Completed, "Download finalizado com sucesso!");
    }
}

void DownloadEngine::parseYtDlpOutput(const std::string &line) {
    std::cout << "[Output] " << line;
    try {
        std::regex rx("\\[download\\]\\s+([0-9.]+)%.*at\\s+([0-9a-zA-Z./]+)\\s+ETA\\s+([0-9:]+)");
        std::smatch match;
        if (std::regex_search(line, match, rx) && match.size() >= 4) {
            double percent = std::stod(match.str(1));
            std::string speed = match.str(2);
            std::string eta = match.str(3);

            m_currentItem.progress = percent;
            m_currentItem.speed = speed;
            m_currentItem.eta = eta;

            if (m_onProgress) m_onProgress(percent, speed, eta);
        }
    } catch (...) {
        // Ignora possíveis exceções de formatação do terminal
    }
}
