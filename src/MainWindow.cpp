#include "MainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QWidget>
#include <QMetaObject>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QSettings>
#include <QDir>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("⚡ NeoVDownloader - Turbo GPU Edition");
    resize(880, 750);

    setupUI();
    setupStyles();

    m_engine.initialize();

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

    style()->unpolish(m_gpuBanner);
    style()->polish(m_gpuBanner);

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
                    m_statusLabel->setText("Status: ✨ Concluído e salvo na pasta com sucesso!");
                    logMessage("✨ [SUCESSO] Operação finalizada! Mídia salva no diretório escolhido.");
                    
                    if (m_notifyCheckBox->isChecked()) {
                        QMessageBox::information(this, "Sucesso", "Download finalizado em velocidade máxima!\nSeus arquivos foram salvos limpos na pasta selecionada.");
                    }
                }
            }
        }, Qt::QueuedConnection);
    });
}

MainWindow::~MainWindow()
{
    // Salvar as preferências do usuário no encerramento da janela
    QSettings settings("NeoV Dev Studio", "NeoVDownloader");
    settings.setValue("outputFolder", m_outputDirInput->text().trimmed());
    settings.setValue("showNotifications", m_notifyCheckBox->isChecked());
    settings.setValue("selectedQuality", m_qualityCombo->currentIndex());

    m_engine.cancelCurrent();
}

void MainWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    m_gpuBanner = new QLabel("Sondando GPU na placa-mãe...", this);
    m_gpuBanner->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_gpuBanner);

    QGroupBox *inputGroup = new QGroupBox("🔗 Parâmetros de Download, Pasta e Recorte", this);
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

    QLabel *folderLabel = new QLabel("📁 Pasta de Destino (Salva):", this);
    m_outputDirInput = new QLineEdit(this);
    
    // Carregar preferências salvas com QSettings
    QSettings settings("NeoV Dev Studio", "NeoVDownloader");
    QString savedFolder = settings.value("outputFolder", "").toString();
    if (savedFolder.isEmpty() || !QDir(savedFolder).exists()) {
        savedFolder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (savedFolder.isEmpty()) savedFolder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    }
    m_outputDirInput->setText(savedFolder);

    int savedQualityIndex = settings.value("selectedQuality", 1).toInt(); // 1080p como padrão
    if (savedQualityIndex >= 0 && savedQualityIndex < m_qualityCombo->count()) {
        m_qualityCombo->setCurrentIndex(savedQualityIndex);
    }

    m_browseDirBtn = new QPushButton("📁 Escolher...", this);
    m_browseDirBtn->setCursor(Qt::PointingHandCursor);
    m_browseDirBtn->setMinimumHeight(35);
    m_browseDirBtn->setObjectName("browseBtn");

    QHBoxLayout *folderLayout = new QHBoxLayout();
    folderLayout->addWidget(m_outputDirInput);
    folderLayout->addWidget(m_browseDirBtn);

    m_notifyCheckBox = new QCheckBox("🔔 Exibir aviso pop-up ao concluir o download (Desativado por padrão)", this);
    bool notifyPref = settings.value("showNotifications", false).toBool();
    m_notifyCheckBox->setChecked(notifyPref);
    m_notifyCheckBox->setCursor(Qt::PointingHandCursor);

    inputLayout->addWidget(urlLabel, 0, 0);
    inputLayout->addWidget(m_urlInput, 0, 1);
    inputLayout->addWidget(qualityLabel, 1, 0);
    inputLayout->addWidget(m_qualityCombo, 1, 1);
    inputLayout->addWidget(timeLabel, 2, 0);
    inputLayout->addWidget(m_timeRangeInput, 2, 1);
    inputLayout->addWidget(folderLabel, 3, 0);
    inputLayout->addLayout(folderLayout, 3, 1);
    inputLayout->addWidget(m_notifyCheckBox, 4, 0, 1, 2);

    mainLayout->addWidget(inputGroup);

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

    m_openFolderBtn = new QPushButton("📂 ABRIR PASTA DO DOWNLOAD", this);
    m_openFolderBtn->setObjectName("openFolderBtn");
    m_openFolderBtn->setCursor(Qt::PointingHandCursor);
    m_openFolderBtn->setMinimumHeight(44);

    btnLayout->addWidget(m_startBtn, 3);
    btnLayout->addWidget(m_cancelBtn, 1);
    btnLayout->addWidget(m_openFolderBtn, 2);
    mainLayout->addLayout(btnLayout);

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

    QLabel *logLabel = new QLabel("📟 Terminal de Processamento Nativo:", this);
    mainLayout->addWidget(logLabel);

    m_logArea = new QTextEdit(this);
    m_logArea->setReadOnly(true);
    m_logArea->setObjectName("logArea");
    mainLayout->addWidget(m_logArea);

    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &MainWindow::onCancelClicked);
    connect(m_browseDirBtn, &QPushButton::clicked, this, &MainWindow::onBrowseClicked);
    connect(m_openFolderBtn, &QPushButton::clicked, this, &MainWindow::onOpenFolderClicked);
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
        QCheckBox {
            font-size: 13px;
            color: #cccccc;
            padding-top: 6px;
        }
        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border: 1px solid #363636;
            border-radius: 4px;
            background-color: #242424;
        }
        QCheckBox::indicator:hover {
            border: 1px solid #10b981;
        }
        QCheckBox::indicator:checked {
            background-color: #10b981;
            border: 1px solid #ffffff;
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
        QPushButton#browseBtn {
            background-color: #2e3b36;
            color: #10b981;
            font-weight: bold;
            border: 1px solid #10b981;
            border-radius: 6px;
            padding: 6px 14px;
        }
        QPushButton#browseBtn:hover {
            background-color: #3b4f47;
            color: #ffffff;
        }
        QPushButton#openFolderBtn {
            background-color: #1a2c38;
            color: #38bdf8;
            font-weight: bold;
            font-size: 13px;
            border: 1px solid #38bdf8;
            border-radius: 6px;
            padding: 10px;
        }
        QPushButton#openFolderBtn:hover {
            background-color: #38bdf8;
            color: #061824;
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

void MainWindow::onBrowseClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Escolha a Pasta de Destino para os Downloads", m_outputDirInput->text());
    if (!dir.isEmpty()) {
        m_outputDirInput->setText(dir);
        QSettings settings("NeoV Dev Studio", "NeoVDownloader");
        settings.setValue("outputFolder", dir);
        logMessage("📁 [Sistema] Nova pasta de destino memorizada para futuros downloads: " + dir);
    }
}

void MainWindow::onOpenFolderClicked()
{
    QString dir = m_outputDirInput->text();
    if (!dir.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
        logMessage("📂 [Sistema] Abrindo a pasta no Windows Explorer: " + dir);
    }
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
    QString outputDir = m_outputDirInput->text().trimmed();

    // Memorizar as preferências na hora do clique
    QSettings settings("NeoV Dev Studio", "NeoVDownloader");
    settings.setValue("outputFolder", outputDir);
    settings.setValue("showNotifications", m_notifyCheckBox->isChecked());
    settings.setValue("selectedQuality", m_qualityCombo->currentIndex());

    m_progressBar->setValue(0);
    m_startBtn->setEnabled(false);
    m_cancelBtn->setEnabled(true);
    
    logMessage("\n========================================================");
    logMessage("⚡ [ACELERADOR] Preparando acionamento dos motores C++...");
    if (!timeRange.isEmpty()) {
        logMessage("✂️ [RECORTE INTELIGENTE] Faixa de tempo programada: " + timeRange);
    }
    logMessage("📁 [DESTINO] Arquivos serão salvos em: " + outputDir);
    
    m_engine.startDownload(url.toStdString(), quality.toStdString(), timeRange.toStdString(), outputDir.toStdString());
}

void MainWindow::onCancelClicked()
{
    logMessage("🛑 [ALERTA] Comando de cancelamento enviado para o worker...");
    m_engine.cancelCurrent();
    m_startBtn->setEnabled(true);
    m_cancelBtn->setEnabled(false);
}
