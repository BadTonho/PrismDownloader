#include "DownloadEngine.h"
#include <iostream>
#include <sstream>
#include <array>
#include <regex>

#ifdef _WIN32
#include <windows.h>
#endif
#include <QProcess>
#include <QString>
#include <QStringList>

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

void DownloadEngine::setLogCallback(std::function<void(const std::string&)> cb) {
    m_onLog = cb;
}

void DownloadEngine::startDownload(const std::string &url, const std::string &quality, const std::string &timeRange, const std::string &outputFolder) {
    if (m_isRunning.load()) {
        std::cerr << "[DownloadEngine] ERRO: Download já está em progresso!\n";
        return;
    }

    m_currentItem = MediaItem{url, "Analisando...", quality, "0 MB/s", "00:00", 0.0, DownloadStatus::Queued};
    m_isRunning.store(true);

    std::ostringstream cmd;
    cmd << "yt-dlp --progress --newline --no-mtime ";
    if (!outputFolder.empty()) {
        cmd << "-P \"" << outputFolder << "\" ";
    }

    if (!timeRange.empty()) {
        std::cout << "✂️ [DownloadEngine] Aplicando recorte inteligente de tempo: " << timeRange << "\n";
        cmd << "--download-sections \"*" << timeRange << "\" ";
    }

    if (m_currentItem.isAudioOnly()) {
        cmd << "-x --audio-format mp3 --audio-quality 0 ";
        if (m_onStatus) m_onStatus(DownloadStatus::ConvertingGPU, "Extraindo Áudio Puro em alta velocidade...");
    } else {
        cmd << "-f \"bestvideo+bestaudio/best\" --merge-output-format mp4 ";
        std::cout << "⚡ [DownloadEngine] Modo Stream Copy ativado (União instantânea sem perda de quadros ou recodificação desnecessária).\n";
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
    std::cout << "[DownloadEngine Worker] Executando comando nativo (Modo Silencioso/GUI): " << command << "\n";
    if (m_onLog) m_onLog("[Processo Motor] Acionando linha de comando no Windows: " + command);
    
    QProcess process;
#ifdef _WIN32
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= 0x08000000; // CREATE_NO_WINDOW (Impede qualquer tela de terminal/conhost de surgir no Windows)
    });
#endif
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start("cmd.exe", QStringList() << "/c" << QString::fromUtf8(command.c_str()));
    
    if (!process.waitForStarted()) {
        std::cerr << "[DownloadEngine] Falha ao abrir processo com yt-dlp/ffmpeg.\n";
        m_isRunning.store(false);
        if (m_onStatus) m_onStatus(DownloadStatus::Error, "Falha ao acionar binários do yt-dlp/ffmpeg.");
        if (m_onLog) m_onLog("[Erro no Motor] Falha crítica: Os executáveis do yt-dlp/ffmpeg não foram encontrados ou não puderam iniciar na pasta da aplicação.");
        return;
    }

    while (m_isRunning.load() && (process.state() == QProcess::Running || process.bytesAvailable() > 0)) {
        if (process.waitForReadyRead(200) || process.bytesAvailable() > 0) {
            while (process.canReadLine() && m_isRunning.load()) {
                QByteArray line = process.readLine();
                parseYtDlpOutput(line.toStdString());
            }
        }
    }

    if (!m_isRunning.load() && process.state() != QProcess::NotRunning) {
        process.kill();
        process.waitForFinished(2000);
    } else {
        process.waitForFinished(-1);
        while (process.canReadLine()) {
            QByteArray line = process.readLine();
            parseYtDlpOutput(line.toStdString());
        }
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
        if (line.find("ERROR:") != std::string::npos || line.find("HTTP Error") != std::string::npos || line.find("Unable to download") != std::string::npos || line.find("Video unavailable") != std::string::npos || line.find("Permission denied") != std::string::npos) {
            if (m_onLog) m_onLog("[Erro no Motor] " + line);
            return;
        }
        if (line.find("WARNING:") != std::string::npos) {
            if (m_onLog) m_onLog("[Alerta no Motor] " + line);
            return;
        }
        if (line.find("[Merger]") != std::string::npos || line.find("Merging formats into") != std::string::npos) {
            if (m_onStatus) m_onStatus(DownloadStatus::Muxing, "📦 Mesclando áudio e vídeo de forma instantânea sem perda (Stream Copy)...");
            if (m_onLog) m_onLog("[Processo Motor] Mesclando streams: " + line);
            return;
        }
        if (line.find("[ExtractAudio]") != std::string::npos || (line.find("Destination: ") != std::string::npos && line.find(".mp3") != std::string::npos)) {
            if (m_onStatus) m_onStatus(DownloadStatus::ConvertingGPU, "🎵 Extraindo faixas de áudio MP3 em alta velocidade...");
            if (m_onLog) m_onLog("[Processo Motor] Extraindo áudio MP3: " + line);
            return;
        }
        if (line.find("Deleting original file") != std::string::npos) {
            if (m_onStatus) m_onStatus(DownloadStatus::Muxing, "🧹 Limpando arquivos temporários e finalizando...");
            if (m_onLog) m_onLog("[Processo Motor] Limpando arquivos temporários de junção...");
            return;
        }
        if (line.find("Already downloaded and merged") != std::string::npos) {
            if (m_onStatus) m_onStatus(DownloadStatus::Muxing, "🔍 Verificando integridade da mídia...");
            if (m_onLog) m_onLog("[Processo Motor] O arquivo selecionado já se encontra no disco.");
            return;
        }

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
            return;
        }

        // Se contiver outras informações valiosas (como [youtube], [info], [ffmpeg], etc.), transmitimos aos logs
        std::string cleanLine = line;
        while (!cleanLine.empty() && (cleanLine.back() == '\r' || cleanLine.back() == '\n' || cleanLine.back() == ' ')) {
            cleanLine.pop_back();
        }
        if (!cleanLine.empty() && cleanLine.find("[download] ") == std::string::npos) {
            if (m_onLog) m_onLog("[Processo Motor] " + cleanLine);
        }
    } catch (...) {
        // Ignora possíveis exceções de formatação do terminal
    }
}
