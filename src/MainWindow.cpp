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
    resize(820, 480); // Janela agora é ultracompatada, moderna e limpa sem o blocão de logs ocupando o fundo!

    setupUI();
    setupStyles();

    m_engine.initialize();

    GPUDetector *gpu = m_engine.gpuDetector();
    QString gpuName = QString::fromStdString(gpu->getGPUName());
    QString codec = QString::fromStdString(gpu->getRecommendedCodec());
    bool hasAccel = gpu->hasHardwareAcceleration();

    if (hasAccel) {
        m_hardwareInfoText = QString(
            "=== INFORMAÇÕES DE HARDWARE E SISTEMA ===\n\n"
            "[GPU DETECTADA]\n"
            "Modelo: %1\n"
            "Codec Acelerado (Turbo): %2\n"
            "Status: ACELERAÇÃO DE HARDWARE ATIVA\n\n"
            "[MOTOR NATIVO]\n"
            "Núcleo C++17 Padrão + Qt 6.7\n"
            "Processador de Mídia: Zero-Loss Stream Copy habilitado."
        ).arg(gpuName, codec);
        logMessage(QString("[System] Placa gráfica ativa no motor: %1 (Codec: %2)").arg(gpuName, codec));
    } else {
        m_hardwareInfoText = 
            "=== INFORMAÇÕES DE HARDWARE E SISTEMA ===\n\n"
            "Modo: Fallback CPU Multi-thread\n"
            "Aviso: Nenhuma aceleração de GPU dedicada NVIDIA foi localizada.\n"
            "Processamento utilizando threads nativas do processador principal.";
        logMessage("[System] Operando no modo Fallback Multi-thread CPU.");
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
    if (m_logDialog) m_logDialog->close();
}

void MainWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // 1. Grupo de Configurações e Parâmetros
    QGroupBox *inputGroup = new QGroupBox("Parâmetros do Download", this);
    QGridLayout *inputLayout = new QGridLayout(inputGroup);
    inputLayout->setSpacing(12);
    inputLayout->setContentsMargins(16, 24, 16, 16);

    QLabel *urlLabel = new QLabel("URL da Mídia:", this);
    m_urlInput = new QLineEdit(this);
    m_urlInput->setPlaceholderText("Cole aqui o link do vídeo ou stream (YouTube, Vimeo, etc)...");

    QLabel *qualityLabel = new QLabel("Qualidade / Resolução:", this);
    m_qualityCombo = new QComboBox(this);
    m_qualityCombo->addItem("4K (Melhor Disponível no Servidor)");
    m_qualityCombo->addItem("1080p Full HD");
    m_qualityCombo->addItem("720p HD");
    m_qualityCombo->addItem("Áudio MP3 (Extração Direta)");

    QLabel *timeLabel = new QLabel("Recorte de Tempo (Opcional):", this);
    m_timeRangeInput = new QLineEdit(this);
    m_timeRangeInput->setPlaceholderText("Ex: 00:01:15-00:03:00 (Deixe em branco para o vídeo completo)");

    QLabel *folderLabel = new QLabel("Pasta de Destino:", this);
    m_outputDirInput = new QLineEdit(this);
    
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

    m_browseDirBtn = new QPushButton("Escolher...", this);
    m_browseDirBtn->setCursor(Qt::PointingHandCursor);
    m_browseDirBtn->setMinimumHeight(34);
    m_browseDirBtn->setObjectName("browseBtn");

    QHBoxLayout *folderLayout = new QHBoxLayout();
    folderLayout->addWidget(m_outputDirInput);
    folderLayout->addWidget(m_browseDirBtn);

    m_notifyCheckBox = new QCheckBox("Exibir aviso pop-up ao concluir o download (Desativado por padrão)", this);
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

    // 2. Duas linhas organizadas de Botões: Controles Principais (Superior) e Ferramentas/Utilitários (Inferior)
    QHBoxLayout *mainBtnLayout = new QHBoxLayout();
    m_startBtn = new QPushButton("INICIAR DOWNLOAD ACELERADO", this);
    m_startBtn->setObjectName("startBtn");
    m_startBtn->setCursor(Qt::PointingHandCursor);
    m_startBtn->setMinimumHeight(44);

    m_cancelBtn = new QPushButton("CANCELAR", this);
    m_cancelBtn->setObjectName("cancelBtn");
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    m_cancelBtn->setMinimumHeight(44);
    m_cancelBtn->setEnabled(false);

    mainBtnLayout->addWidget(m_startBtn, 3);
    mainBtnLayout->addWidget(m_cancelBtn, 1);
    mainLayout->addLayout(mainBtnLayout);

    QHBoxLayout *toolsLayout = new QHBoxLayout();
    m_openFolderBtn = new QPushButton("ABRIR PASTA", this);
    m_openFolderBtn->setObjectName("openFolderBtn");
    m_openFolderBtn->setCursor(Qt::PointingHandCursor);
    m_openFolderBtn->setMinimumHeight(36);

    m_logsBtn = new QPushButton("TERMINAL DE LOGS", this);
    m_logsBtn->setObjectName("toolBtn");
    m_logsBtn->setCursor(Qt::PointingHandCursor);
    m_logsBtn->setMinimumHeight(36);

    m_infoBtn = new QPushButton("INFO / HARDWARE", this);
    m_infoBtn->setObjectName("toolBtn");
    m_infoBtn->setCursor(Qt::PointingHandCursor);
    m_infoBtn->setMinimumHeight(36);

    toolsLayout->addWidget(m_openFolderBtn, 2);
    toolsLayout->addWidget(m_logsBtn, 2);
    toolsLayout->addWidget(m_infoBtn, 2);
    mainLayout->addLayout(toolsLayout);

    // 3. Monitor de Progresso
    QGroupBox *monitorGroup = new QGroupBox("Monitor de Progresso", this);
    QVBoxLayout *monitorLayout = new QVBoxLayout(monitorGroup);
    monitorLayout->setSpacing(12);
    monitorLayout->setContentsMargins(16, 24, 16, 16);

    m_statusLabel = new QLabel("Status: Pronto para iniciar...", this);
    m_statusLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #10b981;");

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setMinimumHeight(22);

    QHBoxLayout *statsLayout = new QHBoxLayout();
    m_speedLabel = new QLabel("Velocidade: 0.0 MB/s", this);
    m_etaLabel = new QLabel("Tempo Restante: --:--", this);
    statsLayout->addWidget(m_speedLabel);
    statsLayout->addStretch();
    statsLayout->addWidget(m_etaLabel);

    monitorLayout->addWidget(m_statusLabel);
    monitorLayout->addWidget(m_progressBar);
    monitorLayout->addLayout(statsLayout);

    mainLayout->addWidget(monitorGroup);

    // 4. Conectar todos os botões aos seus respectivos slots
    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &MainWindow::onCancelClicked);
    connect(m_browseDirBtn, &QPushButton::clicked, this, &MainWindow::onBrowseClicked);
    connect(m_openFolderBtn, &QPushButton::clicked, this, &MainWindow::onOpenFolderClicked);
    connect(m_infoBtn, &QPushButton::clicked, this, &MainWindow::onInfoClicked);
    connect(m_logsBtn, &QPushButton::clicked, this, &MainWindow::onLogsClicked);

    // 5. Configurar a Janela Flutuante de Terminal de Logs (oculta até que o usuário clique em TERMINAL DE LOGS)
    m_logDialog = new QDialog(this);
    m_logDialog->setWindowTitle("Terminal de Processamento Nativo (Logs)");
    m_logDialog->resize(720, 420);
    QVBoxLayout *dlgLayout = new QVBoxLayout(m_logDialog);
    dlgLayout->setContentsMargins(14, 14, 14, 14);

    QLabel *dlgTitle = new QLabel("Saída em Tempo Real dos Motores C++ e Extratores:", m_logDialog);
    dlgTitle->setStyleSheet("font-weight: bold; color: #10b981; font-size: 13px;");
    dlgLayout->addWidget(dlgTitle);

    m_logEdit = new QTextEdit(m_logDialog);
    m_logEdit->setReadOnly(true);
    m_logEdit->setObjectName("logArea");
    dlgLayout->addWidget(m_logEdit);

    QPushButton *closeDlgBtn = new QPushButton("FECHAR TERMINAL", m_logDialog);
    closeDlgBtn->setCursor(Qt::PointingHandCursor);
    closeDlgBtn->setMinimumHeight(34);
    closeDlgBtn->setObjectName("cancelBtn");
    connect(closeDlgBtn, &QPushButton::clicked, m_logDialog, &QDialog::hide);
    dlgLayout->addWidget(closeDlgBtn);
}

void MainWindow::setupStyles()
{
    QString qss = R"(
        QMainWindow, QDialog {
            background-color: #141414;
            color: #dedede;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 13px;
        }
        QWidget {
            color: #dedede;
        }
        QGroupBox {
            background-color: #1a1a1a;
            border: 1px solid #2a2a2a;
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
            padding: 8px;
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
            padding: 8px;
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
        QPushButton#openFolderBtn {
            background-color: #162630;
            color: #38bdf8;
            font-weight: bold;
            font-size: 13px;
            border: 1px solid #38bdf8;
            border-radius: 5px;
            padding: 8px;
        }
        QPushButton#openFolderBtn:hover {
            background-color: #38bdf8;
            color: #061824;
        }
        QPushButton#toolBtn {
            background-color: #222222;
            color: #e2e8f0;
            font-weight: bold;
            font-size: 13px;
            border: 1px solid #525252;
            border-radius: 5px;
            padding: 8px;
        }
        QPushButton#toolBtn:hover {
            background-color: #383838;
            color: #ffffff;
            border: 1px solid #10b981;
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
            background-color: #0c0f0d;
            border: 1px solid #1a241c;
            border-radius: 5px;
            color: #10b981;
            font-family: 'Consolas', 'Courier New', monospace;
            font-size: 12px;
            padding: 8px;
        }
    )";

    setStyleSheet(qss);
    if (m_logDialog) m_logDialog->setStyleSheet(qss);
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

void MainWindow::onInfoClicked()
{
    QMessageBox::information(this, "Informações de Hardware e Sistema", m_hardwareInfoText);
    logMessage("[System] Exibindo janela de Informações e Hardware.");
}

void MainWindow::onLogsClicked()
{
    if (m_logDialog) {
        m_logDialog->show();
        m_logDialog->raise();
        m_logDialog->activateWindow();
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
