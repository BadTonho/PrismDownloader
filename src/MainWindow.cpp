#include "MainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QWidget>
#include <QMetaObject>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("⚡ NeoVDownloader - Turbo GPU Edition");
    resize(850, 680);

    setupUI();
    setupStyles();

    // Inicializamos o Motor Core em C++17 puro
    m_engine.initialize();

    // Consultamos o hardware para exibir no cartão visual superior
    GPUDetector *gpu = m_engine.gpuDetector();
    QString gpuName = QString::fromStdString(gpu->getGPUName());
    QString codec = QString::fromStdString(gpu->getRecommendedCodec());
    bool hasAccel = gpu->hasHardwareAcceleration();

    if (hasAccel) {
        m_gpuBanner->setText(QString("⚡ HARDWARE DETECTADO: %1 | Codec Turbo: [%2]").arg(gpuName, codec));
        m_gpuBanner->setObjectName("gpuBannerActive");
        logMessage(QString("🎯 [Hardware Engine] Placa ativa e operante: %1").arg(gpuName));
    } else {
        m_gpuBanner->setText("ℹ️ MODO FALLBACK CPU: Nenhuma aceleração dedicada engatada");
        m_gpuBanner->setObjectName("gpuBannerInactive");
        logMessage("ℹ️ [Hardware Engine] Usando modo fallback multi-thread na CPU.");
    }

    // Atualizamos o estilo caso o objectName tenha mudado
    style()->unpolish(m_gpuBanner);
    style()->polish(m_gpuBanner);

    // Conectamos os Callbacks do worker C++ (std::thread) à nossa interface Qt (thread-safe!)
    m_engine.setProgressCallback([this](double percent, const std::string &speed, const std::string &eta) {
        QMetaObject::invokeMethod(this, [this, percent, speed, eta]() {
            m_progressBar->setValue(static_cast<int>(percent));
            m_speedLabel->setText(QString("🚀 Velocidade: %1").arg(QString::fromStdString(speed)));
            m_etaLabel->setText(QString("⏳ Tempo Restante: %1").arg(QString::fromStdString(eta)));
        }, Qt::QueuedConnection);
    });

    m_engine.setStatusCallback([this](DownloadStatus status, const std::string &msg) {
        QMetaObject::invokeMethod(this, [this, status, msg]() {
            QString qtMsg = QString::fromStdString(msg);
            m_statusLabel->setText("Status: " + qtMsg);
            logMessage(">>> [STATUS] " + qtMsg);

            if (status == DownloadStatus::Completed || status == DownloadStatus::Cancelled || status == DownloadStatus::Error) {
                m_startBtn->setEnabled(true);
                m_cancelBtn->setEnabled(false);
                if (status == DownloadStatus::Completed) {
                    m_progressBar->setValue(100);
                    QMessageBox::information(this, "Sucesso", "Download finalizado com perfeição!\nArquivo processado em alta velocidade.");
                }
            }
        }, Qt::QueuedConnection);
    });
}

MainWindow::~MainWindow()
{
    m_engine.cancelCurrent();
}

void MainWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // 1. Cartão Banner de Hardware no Topo
    m_gpuBanner = new QLabel("Sondando GPU na placa-mãe...", this);
    m_gpuBanner->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_gpuBanner);

    // 2. Grupo de Configurações do Download
    QGroupBox *inputGroup = new QGroupBox("🔗 Parâmetros de Download e Recorte", this);
    QGridLayout *inputLayout = new QGridLayout(inputGroup);
    inputLayout->setSpacing(12);
    inputLayout->setContentsMargins(16, 24, 16, 16);

    QLabel *urlLabel = new QLabel("URL da Mídia:", this);
    m_urlInput = new QLineEdit(this);
    m_urlInput->setPlaceholderText("Cole aqui o link (YouTube, Vimeo, Twitch, etc)...");

    QLabel *qualityLabel = new QLabel("Qualidade / Resolução:", this);
    m_qualityCombo = new QComboBox(this);
    m_qualityCombo->addItem("4K (Melhor Disponível no Servidor)");
    m_qualityCombo->addItem("1080p Full HD");
    m_qualityCombo->addItem("720p HD");
    m_qualityCombo->addItem("Áudio MP3 Puro (Extração Direta)");

    QLabel *timeLabel = new QLabel("✂️ Recorte de Tempo (Opcional):", this);
    m_timeRangeInput = new QLineEdit(this);
    m_timeRangeInput->setPlaceholderText("Ex: 00:01:15-00:03:00 (Deixe em branco para o vídeo completo)");

    inputLayout->addWidget(urlLabel, 0, 0);
    inputLayout->addWidget(m_urlInput, 0, 1);
    inputLayout->addWidget(qualityLabel, 1, 0);
    inputLayout->addWidget(m_qualityCombo, 1, 1);
    inputLayout->addWidget(timeLabel, 2, 0);
    inputLayout->addWidget(m_timeRangeInput, 2, 1);

    mainLayout->addWidget(inputGroup);

    // 3. Botões de Ação Rápida
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_startBtn = new QPushButton("⚡ INICIAR DOWNLOAD ACELERADO", this);
    m_startBtn->setObjectName("startBtn");
    m_startBtn->setCursor(Qt::PointingHandCursor);
    m_startBtn->setMinimumHeight(44);

    m_cancelBtn = new QPushButton("🛑 CANCELAR", this);
    m_cancelBtn->setObjectName("cancelBtn");
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    m_cancelBtn->setMinimumHeight(44);
    m_cancelBtn->setEnabled(false);

    btnLayout->addWidget(m_startBtn, 3);
    btnLayout->addWidget(m_cancelBtn, 1);
    mainLayout->addLayout(btnLayout);

    // 4. Painel de Progresso e Monitoramento ao Vivo
    QGroupBox *monitorGroup = new QGroupBox("📊 Monitor de Progresso em Tempo Real", this);
    QVBoxLayout *monitorLayout = new QVBoxLayout(monitorGroup);
    monitorLayout->setSpacing(12);
    monitorLayout->setContentsMargins(16, 24, 16, 16);

    m_statusLabel = new QLabel("Status: Pronto para acionar motores...", this);
    m_statusLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #10b981;");

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setMinimumHeight(24);

    QHBoxLayout *statsLayout = new QHBoxLayout();
    m_speedLabel = new QLabel("🚀 Velocidade: 0.0 MB/s", this);
    m_etaLabel = new QLabel("⏳ Tempo Restante: --:--", this);
    statsLayout->addWidget(m_speedLabel);
    statsLayout->addStretch();
    statsLayout->addWidget(m_etaLabel);

    monitorLayout->addWidget(m_statusLabel);
    monitorLayout->addWidget(m_progressBar);
    monitorLayout->addLayout(statsLayout);

    mainLayout->addWidget(monitorGroup);

    // 5. Terminal de Log Embutido
    QLabel *logLabel = new QLabel("📟 Terminal de Processamento Nativo:", this);
    mainLayout->addWidget(logLabel);

    m_logArea = new QTextEdit(this);
    m_logArea->setReadOnly(true);
    m_logArea->setObjectName("logArea");
    mainLayout->addWidget(m_logArea);

    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &MainWindow::onCancelClicked);
}

void MainWindow::setupStyles()
{
    QString qss = R"(
        QMainWindow {
            background-color: #121212;
            color: #e0e0e0;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 13px;
        }
        QWidget {
            color: #e0e0e0;
        }
        QGroupBox {
            background-color: #1a1a1a;
            border: 1px solid #2d2d2d;
            border-radius: 8px;
            margin-top: 14px;
            font-weight: bold;
            color: #ffffff;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 0 8px;
            color: #10b981;
        }
        QLineEdit, QComboBox {
            background-color: #242424;
            border: 1px solid #363636;
            border-radius: 6px;
            padding: 8px 12px;
            color: #ffffff;
            font-size: 13px;
        }
        QLineEdit:focus, QComboBox:focus {
            border: 1px solid #10b981;
            background-color: #2a2a2a;
        }
        QComboBox::drop-down {
            border: none;
        }
        QComboBox QAbstractItemView {
            background-color: #242424;
            color: white;
            selection-background-color: #10b981;
            selection-color: black;
        }
        QLabel {
            font-size: 13px;
        }
        QLabel#gpuBannerActive {
            background-color: #0d281a;
            border: 1px solid #10b981;
            border-radius: 8px;
            padding: 12px;
            font-weight: bold;
            font-size: 14px;
            color: #10b981;
        }
        QLabel#gpuBannerInactive {
            background-color: #281d0d;
            border: 1px solid #f59e0b;
            border-radius: 8px;
            padding: 12px;
            font-weight: bold;
            font-size: 14px;
            color: #f59e0b;
        }
        QPushButton#startBtn {
            background-color: #10b981;
            color: #061e14;
            font-weight: bold;
            font-size: 14px;
            border-radius: 6px;
            border: none;
            padding: 10px;
        }
        QPushButton#startBtn:hover {
            background-color: #059669;
            color: #ffffff;
        }
        QPushButton#startBtn:disabled {
            background-color: #242424;
            color: #666666;
        }
        QPushButton#cancelBtn {
            background-color: #ef4444;
            color: #ffffff;
            font-weight: bold;
            font-size: 14px;
            border-radius: 6px;
            border: none;
            padding: 10px;
        }
        QPushButton#cancelBtn:hover {
            background-color: #dc2626;
        }
        QPushButton#cancelBtn:disabled {
            background-color: #242424;
            color: #666666;
        }
        QProgressBar {
            background-color: #242424;
            border: 1px solid #363636;
            border-radius: 6px;
            text-align: center;
            font-weight: bold;
            color: #ffffff;
        }
        QProgressBar::chunk {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #10b981, stop:1 #047857);
            border-radius: 5px;
        }
        QTextEdit#logArea {
            background-color: #0b0f0d;
            border: 1px solid #1c2b22;
            border-radius: 6px;
            color: #10b981;
            font-family: 'Consolas', 'Courier New', monospace;
            font-size: 12px;
            padding: 8px;
        }
    )";

    setStyleSheet(qss);
}

void MainWindow::logMessage(const QString &msg)
{
    m_logArea->append(msg);
}

void MainWindow::onStartClicked()
{
    QString url = m_urlInput->text().trimmed();
    if (url.isEmpty()) {
        QMessageBox::warning(this, "Atenção", "Por favor, cole um link válido da mídia antes de iniciar o download.");
        return;
    }

    QString quality = m_qualityCombo->currentText();
    QString timeRange = m_timeRangeInput->text().trimmed();

    m_progressBar->setValue(0);
    m_startBtn->setEnabled(false);
    m_cancelBtn->setEnabled(true);
    
    logMessage("\n========================================================");
    logMessage("⚡ [ACELERADOR] Preparando acionamento dos motores C++...");
    if (!timeRange.isEmpty()) {
        logMessage("✂️ [RECORTA INTELIGENTE] Faixa de tempo programada: " + timeRange);
    }
    
    m_engine.startDownload(url.toStdString(), quality.toStdString(), timeRange.toStdString());
}

void MainWindow::onCancelClicked()
{
    logMessage("🛑 [ALERTA] Comando de cancelamento enviado para o worker...");
    m_engine.cancelCurrent();
    m_startBtn->setEnabled(true);
    m_cancelBtn->setEnabled(false);
}
