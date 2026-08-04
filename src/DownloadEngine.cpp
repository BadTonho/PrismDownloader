#include "DownloadEngine.h"
#include "DownloadProfile.h"

#include <iostream>
#include <regex>

#ifdef _WIN32
#include <windows.h>
#endif

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>

namespace {
constexpr const char *kCompletedFilePrefix = "__PRISM_OUTPUT__";
}

DownloadEngine::DownloadEngine() = default;

DownloadEngine::~DownloadEngine() {
    cancelCurrent();
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

void DownloadEngine::initialize() {
    std::cout << "[DownloadEngine] Initializing download engine.\n";
    m_gpuDetector.detect();
}

GPUDetector *DownloadEngine::gpuDetector() {
    return &m_gpuDetector;
}

bool DownloadEngine::isDownloading() const {
    return m_isRunning.load();
}

void DownloadEngine::setProgressCallback(std::function<void(double, const std::string &, const std::string &)> cb) {
    m_onProgress = std::move(cb);
}

void DownloadEngine::setStatusCallback(std::function<void(DownloadStatus, const std::string &)> cb) {
    m_onStatus = std::move(cb);
}

void DownloadEngine::setLogCallback(std::function<void(const std::string &)> cb) {
    m_onLog = std::move(cb);
}

void DownloadEngine::setCompletedFileCallback(std::function<void(const std::string &)> cb) {
    m_onCompletedFile = std::move(cb);
}

void DownloadEngine::startDownload(const std::string &url, const std::string &quality, const std::string &timeRange, const std::string &outputFolder) {
    if (m_isRunning.load()) {
        std::cerr << "[DownloadEngine] A download is already running.\n";
        return;
    }

    m_currentItem = MediaItem{url, "Analyzing...", quality, "0 MB/s", "00:00", 0.0, DownloadStatus::Queued};
    m_completedFilePath.clear();
    m_cancelRequested.store(false);
    m_isRunning.store(true);

    const QString bundledYtdlp = QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/yt-dlp.exe");
    if (!QFile::exists(bundledYtdlp)) {
        m_isRunning.store(false);
        if (m_onStatus) {
            m_onStatus(DownloadStatus::Error, "yt-dlp.exe não foi encontrado na pasta do aplicativo.");
        }
        if (m_onLog) {
            m_onLog("[Erro no Motor] Dependência obrigatória ausente: yt-dlp.exe.");
        }
        return;
    }
    const QString program = bundledYtdlp;

    QStringList arguments;
    arguments << "--progress" << "--newline" << "--no-mtime"
              << "--print" << "after_move:__PRISM_OUTPUT__%(filepath)s";

    if (!outputFolder.empty()) {
        arguments << "-P" << QDir::toNativeSeparators(QString::fromStdString(outputFolder));
    }

    if (!timeRange.empty()) {
        std::cout << "[DownloadEngine] Applying time range.\n";
        arguments << "--download-sections" << ("*" + QString::fromStdString(timeRange));
    }

    if (m_currentItem.isAudioOnly()) {
        arguments << "-x" << "--audio-format" << "mp3" << "--audio-quality" << "0";
        if (m_onStatus) {
            m_onStatus(DownloadStatus::ConvertingGPU, "Extraindo audio puro em alta velocidade...");
        }
    } else {
        arguments << "-f" << QString::fromStdString(DownloadProfile::formatSelectorForQuality(quality))
                  << "--merge-output-format" << "mp4";
        if (m_onStatus) {
            m_onStatus(DownloadStatus::Downloading, "Baixando e juntando streams em alta velocidade...");
        }
    }

    // Program and arguments stay separate: user input cannot change argument boundaries.
    arguments << "--" << QString::fromStdString(url);

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    m_workerThread = std::thread(&DownloadEngine::workerLoop, this, program, arguments);
}

void DownloadEngine::cancelCurrent() {
    if (m_isRunning.load()) {
        std::cout << "[DownloadEngine] Cancellation requested.\n";
        m_cancelRequested.store(true);
        m_isRunning.store(false);
    }
}

void DownloadEngine::workerLoop(const QString &program, const QStringList &arguments) {
    if (m_onLog) {
        m_onLog("[Processo Motor] Acionando yt-dlp com argumentos separados e validados.");
    }

    QProcess process;
    process.setWorkingDirectory(QDir::toNativeSeparators(QCoreApplication::applicationDirPath()));
#ifdef _WIN32
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= 0x08000000; // CREATE_NO_WINDOW
    });
#endif
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(program, arguments);

    if (!process.waitForStarted()) {
        std::cerr << "[DownloadEngine] Failed to start yt-dlp.\n";
        m_isRunning.store(false);
        if (m_onStatus) {
            m_onStatus(DownloadStatus::Error, "Falha ao acionar os binarios do yt-dlp/ffmpeg.");
        }
        if (m_onLog) {
            m_onLog("[Erro no Motor] yt-dlp ou FFmpeg nao puderam ser iniciados.");
        }
        return;
    }

    while (m_isRunning.load() && (process.state() == QProcess::Running || process.bytesAvailable() > 0)) {
        if (process.waitForReadyRead(200) || process.bytesAvailable() > 0) {
            while (process.canReadLine() && m_isRunning.load()) {
                parseYtDlpOutput(process.readLine().toStdString());
            }
        }
    }

    if (!m_isRunning.load() && process.state() != QProcess::NotRunning) {
#ifdef _WIN32
        // yt-dlp can spawn FFmpeg. taskkill /T ends the whole process tree.
        wchar_t systemDirectory[MAX_PATH] = {};
        const UINT systemDirectoryLength = GetSystemDirectoryW(systemDirectory, MAX_PATH);
        if (systemDirectoryLength > 0 && systemDirectoryLength < MAX_PATH) {
            QProcess taskkill;
            taskkill.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
                args->flags |= 0x08000000; // CREATE_NO_WINDOW
            });
            const QString taskkillPath = QDir::fromNativeSeparators(QString::fromWCharArray(systemDirectory) + "/taskkill.exe");
            taskkill.start(taskkillPath, QStringList() << "/PID" << QString::number(process.processId()) << "/T" << "/F");
            taskkill.waitForFinished(5000);
        }
#endif
        process.kill();
        process.waitForFinished(2000);
    } else {
        process.waitForFinished(-1);
        while (process.canReadLine()) {
            parseYtDlpOutput(process.readLine().toStdString());
        }
    }

    const int exitCode = process.exitCode();
    const QProcess::ExitStatus exitStatus = process.exitStatus();

    m_isRunning.store(false);
    if (m_cancelRequested.load()) {
        m_currentItem.status = DownloadStatus::Cancelled;
        if (m_onStatus) {
            m_onStatus(DownloadStatus::Cancelled, "Download cancelado.");
        }
    } else if (exitStatus == QProcess::CrashExit || exitCode != 0) {
        m_currentItem.status = DownloadStatus::Error;
        const std::string errorMessage = "Falha no download (codigo de erro: " + std::to_string(exitCode) + "). Consulte os logs.";
        if (m_onStatus) {
            m_onStatus(DownloadStatus::Error, errorMessage);
        }
        if (m_onLog) {
            m_onLog("[Erro no Motor] yt-dlp/FFmpeg encerrou com codigo " + std::to_string(exitCode) + ".");
        }
    } else {
        m_currentItem.status = DownloadStatus::Completed;
        if (!m_completedFilePath.empty() && m_onCompletedFile) {
            m_onCompletedFile(m_completedFilePath);
        }
        if (m_onStatus) {
            m_onStatus(DownloadStatus::Completed, "Download finalizado com sucesso!");
        }
        if (m_onLog) {
            m_onLog("[Sucesso] Midia baixada, processada e salva no disco.");
        }
    }
}

void DownloadEngine::parseYtDlpOutput(const std::string &line) {
    std::cout << "[Output] " << line;
    try {
        if (line.rfind(kCompletedFilePrefix, 0) == 0) {
            m_completedFilePath = line.substr(std::char_traits<char>::length(kCompletedFilePrefix));
            while (!m_completedFilePath.empty() && (m_completedFilePath.back() == '\r' || m_completedFilePath.back() == '\n')) {
                m_completedFilePath.pop_back();
            }
            if (m_onLog && !m_completedFilePath.empty()) {
                m_onLog("[Processo Motor] Arquivo final identificado pelo yt-dlp.");
            }
            return;
        }

        if (line.find("ERROR:") != std::string::npos || line.find("HTTP Error") != std::string::npos ||
            line.find("Unable to download") != std::string::npos || line.find("Video unavailable") != std::string::npos ||
            line.find("Permission denied") != std::string::npos) {
            if (m_onLog) {
                m_onLog("[Erro no Motor] " + line);
            }
            return;
        }
        if (line.find("WARNING:") != std::string::npos) {
            if (m_onLog) {
                m_onLog("[Alerta no Motor] " + line);
            }
            return;
        }
        if (line.find("[Merger]") != std::string::npos || line.find("Merging formats into") != std::string::npos) {
            if (m_onStatus) {
                m_onStatus(DownloadStatus::Muxing, "Mesclando audio e video sem recodificacao...");
            }
            if (m_onLog) {
                m_onLog("[Processo Motor] Mesclando streams: " + line);
            }
            return;
        }
        if (line.find("[ExtractAudio]") != std::string::npos ||
            (line.find("Destination: ") != std::string::npos && line.find(".mp3") != std::string::npos)) {
            if (m_onStatus) {
                m_onStatus(DownloadStatus::ConvertingGPU, "Extraindo faixas de audio MP3...");
            }
            if (m_onLog) {
                m_onLog("[Processo Motor] Extraindo audio MP3: " + line);
            }
            return;
        }
        if (line.find("Deleting original file") != std::string::npos) {
            if (m_onStatus) {
                m_onStatus(DownloadStatus::Muxing, "Limpando arquivos temporarios e finalizando...");
            }
            if (m_onLog) {
                m_onLog("[Processo Motor] Limpando arquivos temporarios.");
            }
            return;
        }
        if (line.find("Already downloaded and merged") != std::string::npos) {
            if (m_onStatus) {
                m_onStatus(DownloadStatus::Muxing, "Verificando integridade da midia...");
            }
            if (m_onLog) {
                m_onLog("[Processo Motor] O arquivo selecionado ja existe no disco.");
            }
            return;
        }

        const std::regex progressPattern("\\[download\\]\\s+([0-9.]+)%.*at\\s+([^\\s]+)\\s+ETA\\s+([0-9:]+)");
        std::smatch match;
        if (std::regex_search(line, match, progressPattern) && match.size() >= 4) {
            const double percent = std::stod(match.str(1));
            const std::string speed = match.str(2);
            const std::string eta = match.str(3);

            m_currentItem.progress = percent;
            m_currentItem.speed = speed;
            m_currentItem.eta = eta;

            if (m_onProgress) {
                m_onProgress(percent, speed, eta);
            }
            return;
        }

        std::string cleanLine = line;
        while (!cleanLine.empty() && (cleanLine.back() == '\r' || cleanLine.back() == '\n' || cleanLine.back() == ' ')) {
            cleanLine.pop_back();
        }
        if (!cleanLine.empty() && cleanLine.find("[download] ") == std::string::npos && m_onLog) {
            m_onLog("[Processo Motor] " + cleanLine);
        }
    } catch (const std::exception &) {
        if (m_onLog) {
            m_onLog("[Alerta no Motor] Nao foi possivel interpretar uma linha de progresso.");
        }
    }
}
