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
    setWindowTitle("NeoVDownloader - Turbo Edition");
    resize(920, 560);

    setupUI();
    setupStyles();

    m_engine.initialize();

    GPUDetector *gpu = m_engine.gpuDetector();
    QString gpuName = QString::fromStdString(gpu->getGPUName());
    QString codec = QString::fromStdString(gpu->getRecommendedCodec());
    bool hasAccel = gpu->hasHardwareAcceleration();

    if (hasAccel) {
        m_hardwareInfoText = QString(
            "==========================================================\n"
            "                 INFORMAÇÕES DO APLICATIVO                \n"
            "==========================================================\n\n"
            " [DADOS DO SOFTWARE]\n"
            "  Nome Oficial: NeoVDownloader (Turbo GPU Edition)\n"
            "  Versão atual: 1.0.0 (Estável / Release)\n"
            "  Desenvolvimento: Núcleo em C++17 Padrão + Interface Gráfica Qt 6.7\n"
            "  Motor do Sistema: Arquitetura Multi-Thread com Isolamento de Execução\n\n"
            "==========================================================\n"
            "                 DIAGNÓSTICO DE HARDWARE                  \n"
            "==========================================================\n\n"
            " [GPU E ACELERAÇÃO DEDICADA]\n"
            "  Placa Gráfica Detectada: %1\n"
            "  Codec Acelerado Recomendado: [%2]\n"
            "  Status NVENC / Hardware Engine: ATIVO E OPERANTE EM VELOCIDADE MÁXIMA\n\n"
            " [RECURSOS E DESEMPENHO]\n"
            "  Mescla de Mídias: FFmpeg Nativo com tecnologia Zero-Loss Stream Copy\n"
            "  Tempo de Junção: < 1 segundo (sem recodificação redundante de vídeo/áudio)\n"
            "  Extrator de Streams: yt-dlp nativo compatível com alta definição."
        ).arg(gpuName, codec);
        logMessage(QString("[System] Placa gráfica ativa no motor: %1 (Codec: %2)").arg(gpuName, codec));
    } else {
        m_hardwareInfoText = 
            "==========================================================\n"
            "                 INFORMAÇÕES DO APLICATIVO                \n"
            "==========================================================\n\n"
            " [DADOS DO SOFTWARE]\n"
            "  Nome Oficial: NeoVDownloader\n"
            "  Versão atual: 1.0.0 (Estável / Release)\n"
            "  Desenvolvimento: Núcleo em C++17 Padrão + Interface Gráfica Qt 6.7\n\n"
            "==========================================================\n"
            "                 DIAGNÓSTICO DE HARDWARE                  \n"
            "==========================================================\n\n"
            " [GPU E ACELERAÇÃO DEDICADA]\n"
            "  Modo Atual: Fallback CPU Multi-thread\n"
            "  Aviso: Nenhuma aceleração dedicada NVIDIA foi localizada ou ativada.\n"
            "  O processamento ocorrerá pelas threads centrais do processador principal.";
        logMessage("[System] Operando no modo Fallback Multi-thread CPU.");
    }

    if (m_infoEdit) {
        m_infoEdit->setText(m_hardwareInfoText);
    }

    m_engine.setProgressCallback([this](double percent, const std::string &speed, const std::string &eta) {
        QMetaObject::invokeMethod(this, [this, percent, speed, eta]() {
            m_progressBar->setValue(static_cast<int>(percent));
            m_speedLabel->setText(QString("Velocidade: %1").arg(QString::fromStdString(speed)));
            m_etaLabel->setText(QString("Tempo Restante: %1").arg(QString::fromStdString(eta)));
        }, Qt::QueuedConnection);
    });

    m_engine.setStatusCallback([this](DownloadStatus status, const std::string &msg) {
        QMetaObject::invokeMethod(this, [this, status, msg]() {
            QString qtMsg = QString::fromStdString(msg);
            m_statusLabel->setText("Status: " + qtMsg);
            logMessage(">>> [Status] " + qtMsg);

            if (status == DownloadStatus::Completed || status == DownloadStatus::Cancelled || status == DownloadStatus::Error) {
                m_startBtn->setEnabled(true);
                m_cancelBtn->setEnabled(false);
                if (status == DownloadStatus::Completed) {
                    m_progressBar->setValue(100);
                    m_statusLabel->setText("Status: Concluído e salvo na pasta com sucesso!");
                    logMessage("[Sucesso] Operação finalizada! Mídia salva no diretório escolhido.");
                    
                    if (m_notifyCheckBox->isChecked()) {
                        QMessageBox::information(this, "Sucesso", "Download finalizado em velocidade máxima!\nOs arquivos foram salvos na pasta de destino.");
                    }
                }
            }
        }, Qt::QueuedConnection);
    });
}

MainWindow::~MainWindow()
{
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

    QHBoxLayout *rootLayout = new QHBoxLayout(centralWidget);
    rootLayout->setSpacing(0);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    // ==========================================
    // 1. BARRA LATERAL (SIDEBAR NAVIGATION)
    // ==========================================
    QFrame *sidebar = new QFrame(this);
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(200);
    QVBoxLayout *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setSpacing(8);
    sidebarLayout->setContentsMargins(0, 16, 0, 16);

    QLabel *brandLabel = new QLabel("NeoV Studio", sidebar);
    brandLabel->setAlignment(Qt::AlignCenter);
    brandLabel->setStyleSheet("font-weight: bold; font-size: 15px; color: #10b981; margin-bottom: 12px;");
    sidebarLayout->addWidget(brandLabel);

    m_navDownloadBtn = new QPushButton("Downloads", sidebar);
    m_navDownloadBtn->setObjectName("navBtn");
    m_navDownloadBtn->setCheckable(true);
    m_navDownloadBtn->setChecked(true);
    m_navDownloadBtn->setCursor(Qt::PointingHandCursor);

    m_navLogsBtn = new QPushButton("Terminal de Logs", sidebar);
    m_navLogsBtn->setObjectName("navBtn");
    m_navLogsBtn->setCheckable(true);
    m_navLogsBtn->setCursor(Qt::PointingHandCursor);

    m_navInfoBtn = new QPushButton("Informações", sidebar);
    m_navInfoBtn->setObjectName("navBtn");
    m_navInfoBtn->setCheckable(true);
    m_navInfoBtn->setCursor(Qt::PointingHandCursor);

    QButtonGroup *navGroup = new QButtonGroup(this);
    navGroup->setExclusive(true);
    navGroup->addButton(m_navDownloadBtn, 0);
    navGroup->addButton(m_navLogsBtn, 1);
    navGroup->addButton(m_navInfoBtn, 2);

    sidebarLayout->addWidget(m_navDownloadBtn);
    sidebarLayout->addWidget(m_navLogsBtn);
    sidebarLayout->addWidget(m_navInfoBtn);
    sidebarLayout->addStretch();

    m_openFolderBtn = new QPushButton("Abrir Pasta", sidebar);
    m_openFolderBtn->setObjectName("openFolderSideBtn");
    m_openFolderBtn->setCursor(Qt::PointingHandCursor);
    sidebarLayout->addWidget(m_openFolderBtn);

    rootLayout->addWidget(sidebar);

    // ==========================================
    // 2. ÁREA CENTRAL (STACKED WIDGET DE TELAS)
    // ==========================================
    m_stackedWidget = new QStackedWidget(this);
    m_stackedWidget->setObjectName("mainArea");
    rootLayout->addWidget(m_stackedWidget);

    // ---> TELA 0: DOWNLOADS <---
    QWidget *pageDownloads = new QWidget(m_stackedWidget);
    QVBoxLayout *downloadsLayout = new QVBoxLayout(pageDownloads);
    downloadsLayout->setSpacing(16);
    downloadsLayout->setContentsMargins(24, 20, 24, 20);

    QGroupBox *inputGroup = new QGroupBox("Parâmetros do Download", pageDownloads);
    QGridLayout *inputLayout = new QGridLayout(inputGroup);
    inputLayout->setSpacing(12);
    inputLayout->setContentsMargins(16, 24, 16, 16);

    QLabel *urlLabel = new QLabel("URL da Mídia:", pageDownloads);
    m_urlInput = new QLineEdit(pageDownloads);
    m_urlInput->setPlaceholderText("Cole aqui o link do vídeo ou stream (YouTube, Vimeo, etc)...");

    QLabel *qualityLabel = new QLabel("Qualidade / Resolução:", pageDownloads);
    m_qualityCombo = new QComboBox(pageDownloads);
    m_qualityCombo->addItem("4K (Melhor Disponível no Servidor)");
    m_qualityCombo->addItem("1080p Full HD");
    m_qualityCombo->addItem("720p HD");
    m_qualityCombo->addItem("Áudio MP3 (Extração Direta)");

    QLabel *timeLabel = new QLabel("Recorte de Tempo (Opcional):", pageDownloads);
    m_timeRangeInput = new QLineEdit(pageDownloads);
    m_timeRangeInput->setPlaceholderText("Ex: 00:01:15-00:03:00 (Deixe em branco para o vídeo completo)");

    QLabel *folderLabel = new QLabel("Pasta de Destino:", pageDownloads);
    m_outputDirInput = new QLineEdit(pageDownloads);
    
    QSettings settings("NeoV Dev Studio", "NeoVDownloader");
    QString savedFolder = settings.value("outputFolder", "").toString();
    if (savedFolder.isEmpty() || !QDir(savedFolder).exists()) {
        savedFolder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (savedFolder.isEmpty()) savedFolder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    }
    m_outputDirInput->setText(savedFolder);

    int savedQualityIndex = settings.value("selectedQuality", 1).toInt();
    if (savedQualityIndex >= 0 && savedQualityIndex < m_qualityCombo->count()) {
        m_qualityCombo->setCurrentIndex(savedQualityIndex);
    }

    m_browseDirBtn = new QPushButton("Escolher...", pageDownloads);
    m_browseDirBtn->setCursor(Qt::PointingHandCursor);
    m_browseDirBtn->setMinimumHeight(34);
    m_browseDirBtn->setObjectName("browseBtn");

    QHBoxLayout *folderLayout = new QHBoxLayout();
    folderLayout->addWidget(m_outputDirInput);
    folderLayout->addWidget(m_browseDirBtn);

    m_notifyCheckBox = new QCheckBox("Exibir aviso pop-up ao concluir o download (Desativado por padrão)", pageDownloads);
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

    downloadsLayout->addWidget(inputGroup);

    QHBoxLayout *mainBtnLayout = new QHBoxLayout();
    m_startBtn = new QPushButton("INICIAR DOWNLOAD ACELERADO", pageDownloads);
    m_startBtn->setObjectName("startBtn");
    m_startBtn->setCursor(Qt::PointingHandCursor);
    m_startBtn->setMinimumHeight(44);

    m_cancelBtn = new QPushButton("CANCELAR", pageDownloads);
    m_cancelBtn->setObjectName("cancelBtn");
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    m_cancelBtn->setMinimumHeight(44);
    m_cancelBtn->setEnabled(false);

    mainBtnLayout->addWidget(m_startBtn, 3);
    mainBtnLayout->addWidget(m_cancelBtn, 1);
    downloadsLayout->addLayout(mainBtnLayout);

    QGroupBox *monitorGroup = new QGroupBox("Monitor de Progresso", pageDownloads);
    QVBoxLayout *monitorLayout = new QVBoxLayout(monitorGroup);
    monitorLayout->setSpacing(12);
    monitorLayout->setContentsMargins(16, 24, 16, 16);

    m_statusLabel = new QLabel("Status: Pronto para iniciar...", pageDownloads);
    m_statusLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #10b981;");

    m_progressBar = new QProgressBar(pageDownloads);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setMinimumHeight(22);

    QHBoxLayout *statsLayout = new QHBoxLayout();
    m_speedLabel = new QLabel("Velocidade: 0.0 MB/s", pageDownloads);
    m_etaLabel = new QLabel("Tempo Restante: --:--", pageDownloads);
    statsLayout->addWidget(m_speedLabel);
    statsLayout->addStretch();
    statsLayout->addWidget(m_etaLabel);

    monitorLayout->addWidget(m_statusLabel);
    monitorLayout->addWidget(m_progressBar);
    monitorLayout->addLayout(statsLayout);

    downloadsLayout->addWidget(monitorGroup);
    downloadsLayout->addStretch();
    m_stackedWidget->addWidget(pageDownloads);

    // ---> TELA 1: TERMINAL DE LOGS <---
    QWidget *pageLogs = new QWidget(m_stackedWidget);
    QVBoxLayout *logsLayout = new QVBoxLayout(pageLogs);
    logsLayout->setSpacing(12);
    logsLayout->setContentsMargins(24, 20, 24, 20);

    QLabel *logsTitle = new QLabel("Terminal de Processamento Nativo e Diagnóstico em Tempo Real:", pageLogs);
    logsTitle->setStyleSheet("font-weight: bold; color: #10b981; font-size: 14px;");
    logsLayout->addWidget(logsTitle);

    m_logEdit = new QTextEdit(pageLogs);
    m_logEdit->setReadOnly(true);
    m_logEdit->setObjectName("logArea");
    logsLayout->addWidget(m_logEdit);
    m_stackedWidget->addWidget(pageLogs);

    // ---> TELA 2: INFORMAÇÕES DO APP E HARDWARE <---
    QWidget *pageInfo = new QWidget(m_stackedWidget);
    QVBoxLayout *infoLayout = new QVBoxLayout(pageInfo);
    infoLayout->setSpacing(12);
    infoLayout->setContentsMargins(24, 20, 24, 20);

    QLabel *infoTitle = new QLabel("Informações do Aplicativo e Diagnóstico de Hardware:", pageInfo);
    infoTitle->setStyleSheet("font-weight: bold; color: #38bdf8; font-size: 14px;");
    infoLayout->addWidget(infoTitle);

    m_infoEdit = new QTextEdit(pageInfo);
    m_infoEdit->setReadOnly(true);
    m_infoEdit->setObjectName("infoArea");
    m_infoEdit->setText(m_hardwareInfoText);
    infoLayout->addWidget(m_infoEdit);
    m_stackedWidget->addWidget(pageInfo);

    connect(navGroup, &QButtonGroup::idClicked, this, &MainWindow::switchPage);
    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &MainWindow::onCancelClicked);
    connect(m_browseDirBtn, &QPushButton::clicked, this, &MainWindow::onBrowseClicked);
    connect(m_openFolderBtn, &QPushButton::clicked, this, &MainWindow::onOpenFolderClicked);
}

void MainWindow::switchPage(int index)
{
    if (m_stackedWidget) {
        m_stackedWidget->setCurrentIndex(index);
    }
}

void MainWindow::setupStyles()
{
    QString qss = R"(
        QMainWindow {
            background-color: #121212;
            color: #dedede;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 13px;
        }
        QWidget {
            color: #dedede;
        }
        QFrame#sidebar {
            background-color: #181818;
            border-right: 1px solid #262626;
        }
        QStackedWidget#mainArea {
            background-color: #121212;
        }
        QPushButton#navBtn {
            background-color: transparent;
            color: #909090;
            border: none;
            border-left: 3px solid transparent;
            padding: 12px 18px;
            text-align: left;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton#navBtn:hover {
            background-color: #222222;
            color: #ffffff;
        }
        QPushButton#navBtn:checked {
            background-color: #1f2a24;
            color: #10b981;
            border-left: 3px solid #10b981;
        }
        QPushButton#openFolderSideBtn {
            background-color: #1c2e3a;
            color: #38bdf8;
            font-weight: bold;
            font-size: 13px;
            border: 1px solid #38bdf8;
            border-radius: 5px;
            padding: 8px;
            margin: 0 14px;
        }
        QPushButton#openFolderSideBtn:hover {
            background-color: #38bdf8;
            color: #061824;
        }
        QGroupBox {
            background-color: #191919;
            border: 1px solid #262626;
            border-radius: 6px;
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
            background-color: #222222;
            border: 1px solid #333333;
            border-radius: 5px;
            padding: 7px 12px;
            color: #ffffff;
            font-size: 13px;
        }
        QLineEdit:focus, QComboBox:focus {
            border: 1px solid #10b981;
            background-color: #262626;
        }
        QComboBox::drop-down {
            border: none;
        }
        QComboBox QAbstractItemView {
            background-color: #222222;
            color: white;
            selection-background-color: #10b981;
            selection-color: black;
        }
        QCheckBox {
            font-size: 13px;
            color: #b5b5b5;
            padding-top: 4px;
        }
        QCheckBox::indicator {
            width: 17px;
            height: 17px;
            border: 1px solid #3e3e3e;
            border-radius: 4px;
            background-color: #222222;
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
        QPushButton#startBtn {
            background-color: #10b981;
            color: #031c12;
            font-weight: bold;
            font-size: 14px;
            border-radius: 5px;
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
            background-color: #dc2626;
            color: #ffffff;
            font-weight: bold;
            font-size: 13px;
            border-radius: 5px;
            border: none;
            padding: 10px;
        }
        QPushButton#cancelBtn:hover {
            background-color: #b91c1c;
        }
        QPushButton#cancelBtn:disabled {
            background-color: #242424;
            color: #666666;
        }
        QPushButton#browseBtn {
            background-color: #263530;
            color: #10b981;
            font-weight: bold;
            border: 1px solid #10b981;
            border-radius: 5px;
            padding: 6px 14px;
        }
        QPushButton#browseBtn:hover {
            background-color: #354a43;
            color: #ffffff;
        }
        QProgressBar {
            background-color: #222222;
            border: 1px solid #333333;
            border-radius: 5px;
            text-align: center;
            font-weight: bold;
            color: #ffffff;
        }
        QProgressBar::chunk {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #10b981, stop:1 #047857);
            border-radius: 4px;
        }
        QTextEdit#logArea {
            background-color: #0a0e0b;
            border: 1px solid #1a241c;
            border-radius: 5px;
            color: #10b981;
            font-family: 'Consolas', 'Courier New', monospace;
            font-size: 12px;
            padding: 12px;
        }
        QTextEdit#infoArea {
            background-color: #0b141a;
            border: 1px solid #1a2c38;
            border-radius: 5px;
            color: #38bdf8;
            font-family: 'Consolas', 'Courier New', monospace;
            font-size: 13px;
            padding: 14px;
        }
    )";

    setStyleSheet(qss);
}

void MainWindow::logMessage(const QString &msg)
{
    if (m_logEdit) {
        m_logEdit->append(msg);
    }
}

void MainWindow::onBrowseClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Escolha a Pasta de Destino para os Downloads", m_outputDirInput->text());
    if (!dir.isEmpty()) {
        m_outputDirInput->setText(dir);
        QSettings settings("NeoV Dev Studio", "NeoVDownloader");
        settings.setValue("outputFolder", dir);
        logMessage("[System] Nova pasta de destino selecionada: " + dir);
    }
}

void MainWindow::onOpenFolderClicked()
{
    QString dir = m_outputDirInput->text();
    if (!dir.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
        logMessage("[System] Abrindo a pasta no Windows Explorer: " + dir);
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

    QSettings settings("NeoV Dev Studio", "NeoVDownloader");
    settings.setValue("outputFolder", outputDir);
    settings.setValue("showNotifications", m_notifyCheckBox->isChecked());
    settings.setValue("selectedQuality", m_qualityCombo->currentIndex());

    m_progressBar->setValue(0);
    m_startBtn->setEnabled(false);
    m_cancelBtn->setEnabled(true);
    
    logMessage("\n========================================================");
    logMessage("[DownloadEngine] Preparando acionamento dos motores C++...");
    if (!timeRange.isEmpty()) {
        logMessage("[Recorte] Faixa de tempo programada: " + timeRange);
    }
    logMessage("[Destino] Mídia será salva em: " + outputDir);
    
    m_engine.startDownload(url.toStdString(), quality.toStdString(), timeRange.toStdString(), outputDir.toStdString());
}

void MainWindow::onCancelClicked()
{
    logMessage("[Alerta] Comando de cancelamento enviado para o worker...");
    m_engine.cancelCurrent();
    m_startBtn->setEnabled(true);
    m_cancelBtn->setEnabled(false);
}
