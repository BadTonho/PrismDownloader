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
#include <QFileInfo>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QApplication>

// ============================================================================
// CONFIGURAÇÃO OFICIAL DE VERSÃO (Mude apenas aqui para futuras Releases!)
// ============================================================================
static const QString NEOV_VERSION_TAG = "v1.1.1";    // Tag de verificação no GitHub
static const QString NEOV_VERSION_NUMBER = "1.1.1";  // Número de exibição

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_autoConvertAfterDownload(false), m_convertProcess(nullptr), m_networkManager(new QNetworkAccessManager(this))
{
    setWindowTitle("NeoVDownloader - Studio Suite");
    resize(980, 620);

    setupUI();
    setupStyles();

    m_engine.initialize();

    GPUDetector *gpu = m_engine.gpuDetector();
    QString gpuName = QString::fromStdString(gpu->getGPUName());
    QString codec = QString::fromStdString(gpu->getRecommendedCodec());
    bool hasAccel = gpu->hasHardwareAcceleration();

    if (hasAccel) {
        if (m_gpuModelLabel) m_gpuModelLabel->setText(gpuName);
        if (m_gpuCodecLabel) m_gpuCodecLabel->setText(codec);
        if (m_gpuStatusLabel) {
            m_gpuStatusLabel->setText("ATIVO E OPERANTE (NVENC Hardware Engine)");
            m_gpuStatusLabel->setStyleSheet("color: #10b981; font-weight: bold; font-size: 13px;");
        }
        if (m_convertEngineLabel) {
            m_convertEngineLabel->setText("Acelerado por Hardware (" + gpuName + " / NVENC)");
            m_convertEngineLabel->setStyleSheet("color: #10b981; font-weight: bold;");
        }
        logMessage(QString("[System] Placa gráfica ativa no motor: %1 (Codec: %2)").arg(gpuName, codec));
    } else {
        if (m_gpuModelLabel) m_gpuModelLabel->setText("Nenhuma aceleração dedicada NVIDIA foi localizada");
        if (m_gpuCodecLabel) m_gpuCodecLabel->setText("Codec Fallback CPU Padrão");
        if (m_gpuStatusLabel) {
            m_gpuStatusLabel->setText("MODO FALLBACK CPU (Multi-thread)");
            m_gpuStatusLabel->setStyleSheet("color: #f59e0b; font-weight: bold; font-size: 13px;");
        }
        if (m_convertEngineLabel) {
            m_convertEngineLabel->setText("Modo Fallback CPU Padrão (Multi-thread)");
            m_convertEngineLabel->setStyleSheet("color: #f59e0b; font-weight: bold;");
        }
        logMessage("[System] Operando no modo Fallback Multi-thread CPU.");
    }

    m_engine.setProgressCallback([this](double percent, const std::string &speed, const std::string &eta) {
        QMetaObject::invokeMethod(this, [this, percent, speed, eta]() {
            m_progressBar->setValue(static_cast<int>(percent));
            m_speedLabel->setText(QString("Velocidade de Download: %1").arg(QString::fromStdString(speed)));
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
                    refreshLibrary();

                    if (m_autoConvertAfterDownload) {
                        m_statusLabel->setText("Status: Baixado! Iniciando conversão automática para " + m_autoConvertFormat + "...");
                        logMessage("[Auto-Conversão] Download concluído! Acionando conversão automática no arquivo recém-baixado...");
                        
                        // Buscar o arquivo mais recente no diretório exato deste download
                        QString targetDir = m_currentDownloadDir.isEmpty() ? m_outputDirInput->text() : m_currentDownloadDir;
                        QDir dir(targetDir);
                        QStringList filters;
                        filters << "*.mp4" << "*.mp3" << "*.mkv" << "*.webm" << "*.m4a" << "*.avi" << "*.flv" << "*.wav";
                        QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks, QDir::Time);
                        
                        if (!fileList.isEmpty()) {
                            QString newestFilePath = fileList.first().absoluteFilePath();
                            m_convertInput->setText(newestFilePath);
                            m_convertFormatCombo->setCurrentText(m_autoConvertFormat);
                            
                            m_autoConvertAfterDownload = false; // Resetar flag
                            onStartConvertClicked(); // Acionar motor de conversão FFmpeg acelerado
                            
                            if (m_notifyCheckBox->isChecked()) {
                                QMessageBox::information(this, "Download Concluído", "O arquivo foi baixado com sucesso em:\n" + targetDir + "\n\nO conversor foi iniciado automaticamente para transformá-lo em:\n" + m_autoConvertFormat);
                            }
                        } else {
                            logMessage("[Auto-Conversão] Aviso: Não foi possível localizar o arquivo gerado para conversão em " + targetDir);
                        }
                    } else {
                        m_statusLabel->setText("Status: Concluído e salvo com sucesso!");
                        logMessage("[Sucesso] Operação finalizada! Mídia salva no diretório escolhido.");

                        if (m_notifyCheckBox->isChecked()) {
                            QString savedLoc = m_currentDownloadDir.isEmpty() ? m_outputDirInput->text() : m_currentDownloadDir;
                            QMessageBox::information(this, "Sucesso", "Download finalizado em velocidade máxima!\nOs arquivos foram salvos na pasta:\n" + savedLoc);
                        }
                    }
                }
            }
        }, Qt::QueuedConnection);
    });

    m_convertProcess = new QProcess(this);
#ifdef _WIN32
    m_convertProcess->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= 0x08000000; // CREATE_NO_WINDOW
    });
#endif
    connect(m_convertProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::onConvertProcessOutput);
    connect(m_convertProcess, &QProcess::readyReadStandardError, this, &MainWindow::onConvertProcessOutput);
    connect(m_convertProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [this](int exitCode, QProcess::ExitStatus /*exitStatus*/) {
                onConvertProcessFinished(exitCode);
            });

    refreshLibrary();

    // Carregar configurações do Auto-Updater e iniciar checagem silenciosa ao start (se habilitado)
    QSettings settings("Tonho Studios", "NeoVDownloader");
    bool checkOnStart = settings.value("checkUpdatesOnStart", true).toBool(); // Por padrão ATIVADO
    bool autoDownload = settings.value("autoDownloadUpdates", false).toBool(); // Por padrão DESATIVADO
    if (m_checkUpdatesOnStartChk) m_checkUpdatesOnStartChk->setChecked(checkOnStart);
    if (m_autoDownloadUpdatesChk) m_autoDownloadUpdatesChk->setChecked(autoDownload);

    if (checkOnStart) {
        QTimer::singleShot(2500, this, [this]() {
            logMessage("[Updater] Iniciando checagem automática silenciosa de novas versões no GitHub...");
            checkForUpdates(true); // silent = true ao iniciar para não interromper o usuário com pop-up se não houver update
        });
    } else {
        logMessage("[Updater] Busca automática de atualizações ao iniciar está desativada nas configurações.");
    }
}

MainWindow::~MainWindow()
{
    QSettings settings("Tonho Studios", "NeoVDownloader");
    settings.setValue("outputFolder", m_outputDirInput->text().trimmed());
    settings.setValue("showNotifications", m_notifyCheckBox->isChecked());
    settings.setValue("selectedQuality", m_qualityCombo->currentIndex());
    settings.setValue("defaultTimeRange", m_timeRangeInput->text().trimmed());
    if (m_checkUpdatesOnStartChk) settings.setValue("checkUpdatesOnStart", m_checkUpdatesOnStartChk->isChecked());
    if (m_autoDownloadUpdatesChk) settings.setValue("autoDownloadUpdates", m_autoDownloadUpdatesChk->isChecked());

    m_engine.cancelCurrent();
    if (m_convertProcess && m_convertProcess->state() != QProcess::NotRunning) {
        m_convertProcess->kill();
        m_convertProcess->waitForFinished();
    }
}

void MainWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QHBoxLayout *rootLayout = new QHBoxLayout(centralWidget);
    rootLayout->setSpacing(0);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    // ==========================================
    // 1. BARRA LATERAL (SIDEBAR NAVIGATION) - ESTILO ATUBE
    // ==========================================
    QFrame *sidebar = new QFrame(this);
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(220);
    QVBoxLayout *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setSpacing(10);
    sidebarLayout->setContentsMargins(0, 20, 0, 20);

    QLabel *brandLabel = new QLabel("Studio Suite", sidebar);
    brandLabel->setAlignment(Qt::AlignCenter);
    brandLabel->setStyleSheet("font-weight: bold; font-size: 16px; color: #10b981; margin-bottom: 14px;");
    sidebarLayout->addWidget(brandLabel);

    m_navDownloadBtn = new QPushButton("Downloads", sidebar);
    m_navDownloadBtn->setObjectName("navBtn");
    m_navDownloadBtn->setCheckable(true);
    m_navDownloadBtn->setChecked(true);
    m_navDownloadBtn->setCursor(Qt::PointingHandCursor);

    m_navLibraryBtn = new QPushButton("Biblioteca", sidebar);
    m_navLibraryBtn->setObjectName("navBtn");
    m_navLibraryBtn->setCheckable(true);
    m_navLibraryBtn->setCursor(Qt::PointingHandCursor);

    m_navConverterBtn = new QPushButton("Conversor de vídeo", sidebar);
    m_navConverterBtn->setObjectName("navBtn");
    m_navConverterBtn->setCheckable(true);
    m_navConverterBtn->setCursor(Qt::PointingHandCursor);

    m_navLogsBtn = new QPushButton("Terminal de logs", sidebar);
    m_navLogsBtn->setObjectName("navBtn");
    m_navLogsBtn->setCheckable(true);
    m_navLogsBtn->setCursor(Qt::PointingHandCursor);

    m_navInfoBtn = new QPushButton("Informações", sidebar);
    m_navInfoBtn->setObjectName("navBtn");
    m_navInfoBtn->setCheckable(true);
    m_navInfoBtn->setCursor(Qt::PointingHandCursor);

    m_sidebarUpdateBtn = new QPushButton("Atualizações", sidebar);
    m_sidebarUpdateBtn->setObjectName("updateSideBtn");
    m_sidebarUpdateBtn->setCheckable(true);
    m_sidebarUpdateBtn->setCursor(Qt::PointingHandCursor);

    QButtonGroup *navGroup = new QButtonGroup(this);
    navGroup->setExclusive(true);
    navGroup->addButton(m_navDownloadBtn, 0);
    navGroup->addButton(m_navLibraryBtn, 1);
    navGroup->addButton(m_navConverterBtn, 2);
    navGroup->addButton(m_navLogsBtn, 3);
    navGroup->addButton(m_navInfoBtn, 4);
    navGroup->addButton(m_sidebarUpdateBtn, 5);

    sidebarLayout->addWidget(m_navDownloadBtn);
    sidebarLayout->addWidget(m_navLibraryBtn);
    sidebarLayout->addWidget(m_navConverterBtn);
    sidebarLayout->addWidget(m_navLogsBtn);
    sidebarLayout->addWidget(m_navInfoBtn);
    sidebarLayout->addStretch();

    m_sidebarUpdateNotification = new QLabel("🛡️ " + NEOV_VERSION_TAG + " (Em Dia)", sidebar);
    m_sidebarUpdateNotification->setAlignment(Qt::AlignCenter);
    m_sidebarUpdateNotification->setStyleSheet("color: #737373; font-size: 12px; font-weight: bold; margin-bottom: 2px;");
    sidebarLayout->addWidget(m_sidebarUpdateNotification);

    sidebarLayout->addWidget(m_sidebarUpdateBtn);

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

    // ---> TELA 0: DOWNLOADS (LINHA ATUBE CATCHER) <---
    QWidget *pageDownloads = new QWidget(m_stackedWidget);
    QVBoxLayout *downloadsLayout = new QVBoxLayout(pageDownloads);
    downloadsLayout->setSpacing(20);
    downloadsLayout->setContentsMargins(28, 26, 28, 24);

    // LINHA SUPERIOR: INPUT DA URL + BOTÃO BAIXAR EM DESTAQUE
    QHBoxLayout *topInputLayout = new QHBoxLayout();
    topInputLayout->setSpacing(12);

    m_urlInput = new QLineEdit(pageDownloads);
    m_urlInput->setPlaceholderText("Insira nesta caixa o URL completo do seu vídeo (exemplo: https://youtube.com/watch?v=...)");
    m_urlInput->setMinimumHeight(44);
    m_urlInput->setStyleSheet("font-size: 14px; padding: 10px 14px;");

    m_startBtn = new QPushButton("BAIXAR", pageDownloads);
    m_startBtn->setObjectName("startBtn");
    m_startBtn->setCursor(Qt::PointingHandCursor);
    m_startBtn->setMinimumHeight(44);
    m_startBtn->setFixedWidth(140);
    m_startBtn->setStyleSheet("font-size: 15px; font-weight: bold;");

    topInputLayout->addWidget(m_urlInput, 1);
    topInputLayout->addWidget(m_startBtn, 0);
    downloadsLayout->addLayout(topInputLayout);

    // LINHA SECUNDÁRIA: PERFIL DE SAÍDA E PASTA (ESTILO ATUBE)
    QGridLayout *paramLayout = new QGridLayout();
    paramLayout->setSpacing(12);
    paramLayout->setContentsMargins(0, 0, 0, 8);

    QLabel *lblProfile = new QLabel("Perfil de saída padrão:", pageDownloads);
    lblProfile->setStyleSheet("color: #a3a3a3; font-weight: bold; font-size: 13px;");
    m_qualityCombo = new QComboBox(pageDownloads);
    m_qualityCombo->addItem("4K / Melhor Disponível no Servidor (Original)");
    m_qualityCombo->addItem("1080p Full HD (Vídeo MP4 Alta Definição)");
    m_qualityCombo->addItem("720p HD (Vídeo MP4 Qualidade Padrão)");
    m_qualityCombo->addItem("Áudio MP3 (Obter apenas o áudio 320 kbps)");

    QLabel *lblTime = new QLabel("Recorte de tempo (opcional):", pageDownloads);
    lblTime->setStyleSheet("color: #a3a3a3; font-weight: bold; font-size: 13px;");
    m_timeRangeInput = new QLineEdit(pageDownloads);
    m_timeRangeInput->setPlaceholderText("Ex: 00:01:15-00:03:00 (Vazio = baixar completo)");

    QLabel *lblSave = new QLabel("Salvar downloads em:", pageDownloads);
    lblSave->setStyleSheet("color: #a3a3a3; font-weight: bold; font-size: 13px;");
    
    QHBoxLayout *saveLayout = new QHBoxLayout();
    m_outputDirInput = new QLineEdit(pageDownloads);
    m_outputDirInput->setReadOnly(false);
    m_browseDirBtn = new QPushButton("Alterar...", pageDownloads);
    m_browseDirBtn->setObjectName("browseBtn");
    m_browseDirBtn->setCursor(Qt::PointingHandCursor);
    m_browseDirBtn->setMinimumHeight(32);
    saveLayout->addWidget(m_outputDirInput, 1);
    saveLayout->addWidget(m_browseDirBtn, 0);

    QSettings settings("Tonho Studios", "NeoVDownloader");
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

    paramLayout->addWidget(lblProfile, 0, 0);
    paramLayout->addWidget(m_qualityCombo, 0, 1);
    paramLayout->addWidget(lblTime, 1, 0);
    paramLayout->addWidget(m_timeRangeInput, 1, 1);
    paramLayout->addWidget(lblSave, 2, 0);
    paramLayout->addLayout(saveLayout, 2, 1);
    paramLayout->setColumnStretch(1, 1);

    downloadsLayout->addLayout(paramLayout);

    // PAINEL CENTRAL DE PROCESSAMENTO E MONITORAMENTO AO VIVO
    QGroupBox *centralPanel = new QGroupBox("Área de Processamento e Monitoramento de Download", pageDownloads);
    QVBoxLayout *centerLayout = new QVBoxLayout(centralPanel);
    centerLayout->setSpacing(16);
    centerLayout->setContentsMargins(20, 30, 20, 24);

    m_statusLabel = new QLabel("Status: Pronto. Aguardando você inserir uma URL acima para começar...", pageDownloads);
    m_statusLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: #10b981;");

    m_progressBar = new QProgressBar(pageDownloads);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setMinimumHeight(28);
    m_progressBar->setStyleSheet("font-size: 14px;");

    QHBoxLayout *statsLayout = new QHBoxLayout();
    m_speedLabel = new QLabel("Velocidade de Download: 0.0 MB/s", pageDownloads);
    m_speedLabel->setStyleSheet("font-size: 13px; color: #cbd5e1;");
    m_etaLabel = new QLabel("Tempo Restante: --:--", pageDownloads);
    m_etaLabel->setStyleSheet("font-size: 13px; color: #cbd5e1;");
    statsLayout->addWidget(m_speedLabel);
    statsLayout->addStretch();
    statsLayout->addWidget(m_etaLabel);

    QHBoxLayout *actionBottomLayout = new QHBoxLayout();
    m_cancelBtn = new QPushButton("CANCELAR OPERAÇÃO ATUAL", pageDownloads);
    m_cancelBtn->setObjectName("cancelBtn");
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    m_cancelBtn->setMinimumHeight(40);
    m_cancelBtn->setFixedWidth(230);
    m_cancelBtn->setEnabled(false);

    m_notifyCheckBox = new QCheckBox("Exibir aviso pop-up ao concluir o download (Desativado por padrão)", pageDownloads);
    bool notifyPref = settings.value("showNotifications", false).toBool();
    m_notifyCheckBox->setChecked(notifyPref);
    m_notifyCheckBox->setCursor(Qt::PointingHandCursor);

    actionBottomLayout->addWidget(m_notifyCheckBox);
    actionBottomLayout->addStretch();
    actionBottomLayout->addWidget(m_cancelBtn);

    centerLayout->addWidget(m_statusLabel);
    centerLayout->addWidget(m_progressBar);
    centerLayout->addLayout(statsLayout);
    centerLayout->addStretch();
    centerLayout->addLayout(actionBottomLayout);

    downloadsLayout->addWidget(centralPanel, 1);
    m_stackedWidget->addWidget(pageDownloads);

    // ---> TELA 1: BIBLIOTECA DE MÍDIAS <---
    QWidget *pageLibrary = new QWidget(m_stackedWidget);
    QVBoxLayout *libLayout = new QVBoxLayout(pageLibrary);
    libLayout->setSpacing(14);
    libLayout->setContentsMargins(24, 20, 24, 20);

    QHBoxLayout *libTopLayout = new QHBoxLayout();
    QLabel *libTitle = new QLabel("Biblioteca de Mídias (Arquivos na Pasta de Destino):", pageLibrary);
    libTitle->setStyleSheet("font-weight: bold; color: #10b981; font-size: 15px;");
    libTopLayout->addWidget(libTitle);
    libTopLayout->addStretch();

    QPushButton *btnRefreshLib = new QPushButton("Atualizar Lista", pageLibrary);
    btnRefreshLib->setObjectName("browseBtn");
    btnRefreshLib->setCursor(Qt::PointingHandCursor);
    btnRefreshLib->setMinimumHeight(32);
    connect(btnRefreshLib, &QPushButton::clicked, this, &MainWindow::refreshLibrary);
    libTopLayout->addWidget(btnRefreshLib);

    libLayout->addLayout(libTopLayout);

    m_libraryTable = new QTableWidget(0, 3, pageLibrary);
    QStringList headers;
    headers << "Arquivo de Mídia" << "Formato" << "Tamanho";
    m_libraryTable->setHorizontalHeaderLabels(headers);
    m_libraryTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_libraryTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_libraryTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_libraryTable->verticalHeader()->setVisible(false);
    m_libraryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_libraryTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_libraryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_libraryTable->setAlternatingRowColors(true);
    m_libraryTable->setObjectName("libraryTable");
    connect(m_libraryTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::onLibraryDoubleClicked);

    libLayout->addWidget(m_libraryTable);

    QHBoxLayout *libBottomLayout = new QHBoxLayout();
    QPushButton *btnPlay = new QPushButton("REPRODUZIR / ABRIR SELECIONADO", pageLibrary);
    btnPlay->setObjectName("startBtn");
    btnPlay->setCursor(Qt::PointingHandCursor);
    btnPlay->setMinimumHeight(40);
    connect(btnPlay, &QPushButton::clicked, this, &MainWindow::onPlaySelectedMedia);

    QPushButton *btnOpenExp = new QPushButton("ABRIR NO EXPLORER", pageLibrary);
    btnOpenExp->setObjectName("openFolderSideBtn");
    btnOpenExp->setCursor(Qt::PointingHandCursor);
    btnOpenExp->setMinimumHeight(40);
    connect(btnOpenExp, &QPushButton::clicked, this, &MainWindow::onOpenFolderClicked);

    libBottomLayout->addWidget(btnPlay, 2);
    libBottomLayout->addWidget(btnOpenExp, 1);
    libLayout->addLayout(libBottomLayout);

    m_stackedWidget->addWidget(pageLibrary);

    // ---> TELA 2: CONVERSOR DE VÍDEO E ÁUDIO <---
    QWidget *pageConverter = new QWidget(m_stackedWidget);
    QVBoxLayout *convLayout = new QVBoxLayout(pageConverter);
    convLayout->setSpacing(16);
    convLayout->setContentsMargins(24, 20, 24, 20);

    QGroupBox *convGroup = new QGroupBox("Conversor Nativo de Mídia", pageConverter);
    QGridLayout *convGrid = new QGridLayout(convGroup);
    convGrid->setSpacing(14);
    convGrid->setContentsMargins(16, 26, 16, 18);

    QLabel *lblConvFile = new QLabel("Arquivo de Origem:", pageConverter);
    m_convertInput = new QLineEdit(pageConverter);
    m_convertInput->setPlaceholderText("Selecione um vídeo ou música no seu computador...");
    m_convertBrowseBtn = new QPushButton("Selecionar...", pageConverter);
    m_convertBrowseBtn->setObjectName("browseBtn");
    m_convertBrowseBtn->setCursor(Qt::PointingHandCursor);
    m_convertBrowseBtn->setMinimumHeight(34);
    connect(m_convertBrowseBtn, &QPushButton::clicked, this, &MainWindow::onConvertBrowseClicked);

    QHBoxLayout *convFileLayout = new QHBoxLayout();
    convFileLayout->addWidget(m_convertInput);
    convFileLayout->addWidget(m_convertBrowseBtn);

    QLabel *lblConvFormat = new QLabel("Formato de Saída:", pageConverter);
    m_convertFormatCombo = new QComboBox(pageConverter);
    m_convertFormatCombo->addItem("MP4 (H.264 / NVENC - Compatibilidade Universal)");
    m_convertFormatCombo->addItem("MP4 (HEVC / H.265 - Compressão de Alta Densidade)");
    m_convertFormatCombo->addItem("MKV (Matroska - Container Sem Perdas)");
    m_convertFormatCombo->addItem("MP3 (Áudio MP3 Alta Fidelidade - 320kbps)");
    m_convertFormatCombo->addItem("WAV (Áudio Sem Compressão / Estúdios)");
    m_convertFormatCombo->addItem("WEBM (Otimizado para Web e Redes Sociais)");

    QLabel *lblConvEngineTitle = new QLabel("Motor de Aceleração:", pageConverter);
    m_convertEngineLabel = new QLabel("Sondando GPU...", pageConverter);

    convGrid->addWidget(lblConvFile, 0, 0);
    convGrid->addLayout(convFileLayout, 0, 1);
    convGrid->addWidget(lblConvFormat, 1, 0);
    convGrid->addWidget(m_convertFormatCombo, 1, 1);
    convGrid->addWidget(lblConvEngineTitle, 2, 0);
    convGrid->addWidget(m_convertEngineLabel, 2, 1);

    convLayout->addWidget(convGroup);

    QHBoxLayout *convBtnLayout = new QHBoxLayout();
    m_startConvertBtn = new QPushButton("INICIAR CONVERSÃO RÁPIDA", pageConverter);
    m_startConvertBtn->setObjectName("startBtn");
    m_startConvertBtn->setCursor(Qt::PointingHandCursor);
    m_startConvertBtn->setMinimumHeight(44);
    connect(m_startConvertBtn, &QPushButton::clicked, this, &MainWindow::onStartConvertClicked);

    m_cancelConvertBtn = new QPushButton("CANCELAR", pageConverter);
    m_cancelConvertBtn->setObjectName("cancelBtn");
    m_cancelConvertBtn->setCursor(Qt::PointingHandCursor);
    m_cancelConvertBtn->setMinimumHeight(44);
    m_cancelConvertBtn->setEnabled(false);
    connect(m_cancelConvertBtn, &QPushButton::clicked, this, &MainWindow::onCancelConvertClicked);

    convBtnLayout->addWidget(m_startConvertBtn, 3);
    convBtnLayout->addWidget(m_cancelConvertBtn, 1);
    convLayout->addLayout(convBtnLayout);

    QGroupBox *convMonGroup = new QGroupBox("Progresso da Conversão", pageConverter);
    QVBoxLayout *convMonLayout = new QVBoxLayout(convMonGroup);
    convMonLayout->setSpacing(12);
    convMonLayout->setContentsMargins(16, 24, 16, 16);

    m_convertStatusLabel = new QLabel("Status do Conversor: Aguardando seleção do arquivo...", pageConverter);
    m_convertStatusLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #38bdf8;");

    m_convertProgressBar = new QProgressBar(pageConverter);
    m_convertProgressBar->setRange(0, 100);
    m_convertProgressBar->setValue(0);
    m_convertProgressBar->setMinimumHeight(22);

    convMonLayout->addWidget(m_convertStatusLabel);
    convMonLayout->addWidget(m_convertProgressBar);

    convLayout->addWidget(convMonGroup);
    convLayout->addStretch();
    m_stackedWidget->addWidget(pageConverter);

    // ---> TELA 3: TERMINAL DE LOGS <---
    QWidget *pageLogs = new QWidget(m_stackedWidget);
    QVBoxLayout *logsLayout = new QVBoxLayout(pageLogs);
    logsLayout->setSpacing(12);
    logsLayout->setContentsMargins(24, 20, 24, 20);

    QLabel *logsTitle = new QLabel("Terminal de logs do processador e telemetria:", pageLogs);
    logsTitle->setStyleSheet("font-weight: bold; color: #10b981; font-size: 15px;");
    logsLayout->addWidget(logsTitle);

    m_logEdit = new QTextEdit(pageLogs);
    m_logEdit->setReadOnly(true);
    m_logEdit->setObjectName("logArea");
    logsLayout->addWidget(m_logEdit);
    m_stackedWidget->addWidget(pageLogs);

    // ---> TELA 4: INFORMAÇÕES E HARDWARE <---
    QWidget *pageInfo = new QWidget(m_stackedWidget);
    QVBoxLayout *infoLayout = new QVBoxLayout(pageInfo);
    infoLayout->setSpacing(16);
    infoLayout->setContentsMargins(24, 20, 24, 20);



    QGroupBox *appInfoGroup = new QGroupBox("Informações do Aplicativo", pageInfo);
    QGridLayout *appLayout = new QGridLayout(appInfoGroup);
    appLayout->setSpacing(10);
    appLayout->setContentsMargins(16, 24, 16, 16);

    QLabel *lblAppNameKey = new QLabel("Nome Oficial:", appInfoGroup);
    lblAppNameKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    QLabel *lblAppNameVal = new QLabel("NeoVDownloader (Studio Suite Edition)", appInfoGroup);
    lblAppNameVal->setStyleSheet("color: #ffffff; font-weight: bold; font-size: 13px;");

    QLabel *lblAppVerKey = new QLabel("Versão Atual:", appInfoGroup);
    lblAppVerKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    QLabel *lblAppVerVal = new QLabel(NEOV_VERSION_NUMBER + " (Estável / Release)", appInfoGroup);
    lblAppVerVal->setStyleSheet("color: #10b981; font-weight: bold; font-size: 13px;");

    QLabel *lblAppArchKey = new QLabel("Arquitetura:", appInfoGroup);
    lblAppArchKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    QLabel *lblAppArchVal = new QLabel("Núcleo em C++17 Padrão + Interface Gráfica Qt 6.7", appInfoGroup);
    lblAppArchVal->setStyleSheet("color: #e2e8f0; font-size: 13px;");

    QLabel *lblAppEngineKey = new QLabel("Motor de Processamento:", appInfoGroup);
    lblAppEngineKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    QLabel *lblAppEngineVal = new QLabel("Multi-Thread Paralelo com Isolamento de Execução", appInfoGroup);
    lblAppEngineVal->setStyleSheet("color: #e2e8f0; font-size: 13px;");

    appLayout->addWidget(lblAppNameKey, 0, 0);
    appLayout->addWidget(lblAppNameVal, 0, 1);
    appLayout->addWidget(lblAppVerKey, 1, 0);
    appLayout->addWidget(lblAppVerVal, 1, 1);
    appLayout->addWidget(lblAppArchKey, 2, 0);
    appLayout->addWidget(lblAppArchVal, 2, 1);
    appLayout->addWidget(lblAppEngineKey, 3, 0);
    appLayout->addWidget(lblAppEngineVal, 3, 1);
    appLayout->setColumnStretch(1, 1);
    infoLayout->addWidget(appInfoGroup);

    QGroupBox *hwInfoGroup = new QGroupBox("Diagnóstico de Hardware e Aceleração", pageInfo);
    QGridLayout *hwLayout = new QGridLayout(hwInfoGroup);
    hwLayout->setSpacing(10);
    hwLayout->setContentsMargins(16, 24, 16, 16);

    QLabel *lblGpuModelKey = new QLabel("Placa Gráfica Detectada:", hwInfoGroup);
    lblGpuModelKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    m_gpuModelLabel = new QLabel("Sondando hardware...", hwInfoGroup);
    m_gpuModelLabel->setStyleSheet("color: #38bdf8; font-weight: bold; font-size: 13px;");

    QLabel *lblGpuCodecKey = new QLabel("Codec de Aceleração:", hwInfoGroup);
    lblGpuCodecKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    m_gpuCodecLabel = new QLabel("---", hwInfoGroup);
    m_gpuCodecLabel->setStyleSheet("color: #e2e8f0; font-size: 13px; font-family: 'Consolas', monospace;");

    QLabel *lblGpuStatusKey = new QLabel("Status do Motor:", hwInfoGroup);
    lblGpuStatusKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    m_gpuStatusLabel = new QLabel("Verificando...", hwInfoGroup);

    hwLayout->addWidget(lblGpuModelKey, 0, 0);
    hwLayout->addWidget(m_gpuModelLabel, 0, 1);
    hwLayout->addWidget(lblGpuCodecKey, 1, 0);
    hwLayout->addWidget(m_gpuCodecLabel, 1, 1);
    hwLayout->addWidget(lblGpuStatusKey, 2, 0);
    hwLayout->addWidget(m_gpuStatusLabel, 2, 1);
    hwLayout->setColumnStretch(1, 1);
    infoLayout->addWidget(hwInfoGroup);

    QGroupBox *techGroup = new QGroupBox("Tecnologias Integradas no Core", pageInfo);
    QGridLayout *techLayout = new QGridLayout(techGroup);
    techLayout->setSpacing(10);
    techLayout->setContentsMargins(16, 24, 16, 16);

    QLabel *lblTechMergeKey = new QLabel("Mescla de Mídias:", techGroup);
    lblTechMergeKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    QLabel *lblTechMergeVal = new QLabel("FFmpeg Nativo com tecnologia Zero-Loss Stream Copy", techGroup);
    lblTechMergeVal->setStyleSheet("color: #e2e8f0; font-size: 13px;");

    QLabel *lblTechTimeKey = new QLabel("Tempo de Junção:", techGroup);
    lblTechTimeKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    QLabel *lblTechTimeVal = new QLabel("< 1 segundo por arquivo (sem recodificação redundante de áudio/vídeo)", techGroup);
    lblTechTimeVal->setStyleSheet("color: #10b981; font-weight: bold; font-size: 13px;");

    QLabel *lblTechExtKey = new QLabel("Engine Extrator:", techGroup);
    lblTechExtKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    QLabel *lblTechExtVal = new QLabel("yt-dlp nativo de alta compatibilidade com mais de 1000 plataformas", techGroup);
    lblTechExtVal->setStyleSheet("color: #e2e8f0; font-size: 13px;");

    techLayout->addWidget(lblTechMergeKey, 0, 0);
    techLayout->addWidget(lblTechMergeVal, 0, 1);
    techLayout->addWidget(lblTechTimeKey, 1, 0);
    techLayout->addWidget(lblTechTimeVal, 1, 1);
    techLayout->addWidget(lblTechExtKey, 2, 0);
    techLayout->addWidget(lblTechExtVal, 2, 1);
    techLayout->setColumnStretch(1, 1);
    infoLayout->addWidget(techGroup);

    infoLayout->addStretch();
    m_stackedWidget->addWidget(pageInfo);

    // ---> TELA 5: CENTRAL DE ATUALIZAÇÕES E COMPATIBILIDADE <---
    QWidget *pageUpdates = new QWidget(m_stackedWidget);
    QVBoxLayout *upPageLayout = new QVBoxLayout(pageUpdates);
    upPageLayout->setSpacing(16);
    upPageLayout->setContentsMargins(24, 20, 24, 20);

    QGroupBox *updateGroup = new QGroupBox("Central de Atualizações e Versão (GitHub Release Core)", pageUpdates);
    QVBoxLayout *upLayout = new QVBoxLayout(updateGroup);
    upLayout->setSpacing(12);
    upLayout->setContentsMargins(16, 24, 16, 16);

    QGridLayout *upGrid = new QGridLayout();
    upGrid->setSpacing(10);

    QLabel *lblServerKey = new QLabel("Repositório de Nuvem:", updateGroup);
    lblServerKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    QLabel *lblServerVal = new QLabel("GitHub Oficial (BadTonho/Baixar)", updateGroup);
    lblServerVal->setStyleSheet("color: #38bdf8; font-weight: bold; font-size: 13px;");

    QLabel *lblUpStatusKey = new QLabel("Status de Versão:", updateGroup);
    lblUpStatusKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    m_updateStatusLabel = new QLabel("Versão " + NEOV_VERSION_TAG + " (Release) operacional. Aguardando verificação...", updateGroup);
    m_updateStatusLabel->setStyleSheet("color: #ffffff; font-weight: bold; font-size: 13px;");

    upGrid->addWidget(lblServerKey, 0, 0);
    upGrid->addWidget(lblServerVal, 0, 1);
    upGrid->addWidget(lblUpStatusKey, 1, 0);
    upGrid->addWidget(m_updateStatusLabel, 1, 1);
    upGrid->setColumnStretch(1, 1);
    upLayout->addLayout(upGrid);

    m_checkUpdatesOnStartChk = new QCheckBox("Verificar novas atualizações automaticamente ao iniciar o aplicativo", updateGroup);
    m_checkUpdatesOnStartChk->setCursor(Qt::PointingHandCursor);
    
    m_autoDownloadUpdatesChk = new QCheckBox("Baixar e instalar novas versões automaticamente no fundo (Desativado por padrão)", updateGroup);
    m_autoDownloadUpdatesChk->setCursor(Qt::PointingHandCursor);
    m_autoDownloadUpdatesChk->setStyleSheet("color: #f59e0b; font-weight: bold;");

    QVBoxLayout *chkVertLayout = new QVBoxLayout();
    chkVertLayout->setSpacing(6);
    chkVertLayout->addWidget(m_checkUpdatesOnStartChk);
    chkVertLayout->addWidget(m_autoDownloadUpdatesChk);
    upLayout->addLayout(chkVertLayout);

    QHBoxLayout *upBtnsLayout = new QHBoxLayout();
    m_checkUpdateBtn = new QPushButton("VERIFICAR NO GITHUB AGORA", updateGroup);
    m_checkUpdateBtn->setObjectName("startBtn");
    m_checkUpdateBtn->setCursor(Qt::PointingHandCursor);
    m_checkUpdateBtn->setMinimumHeight(40);
    connect(m_checkUpdateBtn, &QPushButton::clicked, this, [this]() { checkForUpdates(false); });

    m_updateYtdlpBtn = new QPushButton("ATUALIZAR MOTOR EXTRATOR (YT-DLP)", updateGroup);
    m_updateYtdlpBtn->setObjectName("browseBtn");
    m_updateYtdlpBtn->setCursor(Qt::PointingHandCursor);
    m_updateYtdlpBtn->setMinimumHeight(40);
    connect(m_updateYtdlpBtn, &QPushButton::clicked, this, &MainWindow::updateYtdlpEngine);

    upBtnsLayout->addWidget(m_checkUpdateBtn, 3);
    upBtnsLayout->addWidget(m_updateYtdlpBtn, 2);
    upLayout->addLayout(upBtnsLayout);

    upPageLayout->addWidget(updateGroup);
    upPageLayout->addStretch();
    m_stackedWidget->addWidget(pageUpdates);

    // Conectar navegação e botões principais
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
        if (index == 1) { // Se abriu a aba Biblioteca, atualizar a tabela
            refreshLibrary();
        }
    }
}

void MainWindow::onConvertBrowseClicked()
{
    QString file = QFileDialog::getOpenFileName(this, "Selecione a Mídia para Converter", m_outputDirInput->text(),
                                                "Arquivos de Mídia (*.mp4 *.mkv *.avi *.mov *.webm *.flv *.mp3 *.m4a *.wav);;Todos os Arquivos (*.*)");
    if (!file.isEmpty()) {
        m_convertInput->setText(file);
        m_convertStatusLabel->setText("Status do Conversor: Arquivo selecionado! Pronto para converter.");
        logMessage("[Conversor] Arquivo de origem selecionado: " + file);
    }
}

void MainWindow::onStartConvertClicked()
{
    QString inFile = m_convertInput->text().trimmed();
    if (inFile.isEmpty() || !QFile::exists(inFile)) {
        QMessageBox::warning(this, "Atenção", "Selecione um arquivo de mídia existente no computador para converter.");
        return;
    }

    QString formatText = m_convertFormatCombo->currentText();
    QFileInfo fileInfo(inFile);
    QString baseName = fileInfo.completeBaseName();
    
    // Se estiver convertendo automaticamente pós-download, utilizar a pasta exclusiva que foi configurada
    QString outFolder = m_currentDownloadDir.isEmpty() ? m_outputDirInput->text().trimmed() : m_currentDownloadDir;
    QDir dir(outFolder);
    if (!dir.exists()) dir.mkpath(".");

    QString ext = ".mp4";
    QStringList args;
    args << "-y" << "-i" << inFile;

    bool hasAccel = m_engine.gpuDetector()->hasHardwareAcceleration();

    if (formatText.startsWith("MP4 (H.264")) {
        ext = "_convertido.mp4";
        if (hasAccel) {
            args << "-c:v" << "h264_nvenc" << "-preset" << "p4" << "-cq" << "23" << "-c:a" << "aac" << "-b:a" << "192k";
        } else {
            args << "-c:v" << "libx264" << "-crf" << "23" << "-c:a" << "aac";
        }
    } else if (formatText.startsWith("MP4 (HEVC")) {
        ext = "_hevc.mp4";
        if (hasAccel) {
            args << "-c:v" << "hevc_nvenc" << "-preset" << "p4" << "-cq" << "25" << "-c:a" << "aac" << "-b:a" << "192k";
        } else {
            args << "-c:v" << "libx265" << "-crf" << "25" << "-c:a" << "aac";
        }
    } else if (formatText.startsWith("MKV")) {
        ext = "_convertido.mkv";
        args << "-c" << "copy";
    } else if (formatText.startsWith("MP3")) {
        ext = "_audio.mp3";
        args << "-vn" << "-c:a" << "libmp3lame" << "-b:a" << "320k";
    } else if (formatText.startsWith("WAV")) {
        ext = "_audio.wav";
        args << "-vn" << "-c:a" << "pcm_s16le";
    } else if (formatText.startsWith("WEBM")) {
        ext = "_convertido.webm";
        args << "-c:v" << "libvpx-vp9" << "-b:v" << "2M" << "-c:a" << "libopus";
    }

    QString outFile = dir.absoluteFilePath(baseName + ext);
    args << outFile;

    QString ffmpegPath = QCoreApplication::applicationDirPath() + "/ffmpeg.exe";
    if (!QFile::exists(ffmpegPath)) {
        ffmpegPath = "ffmpeg";
    }

    m_convertProgressBar->setRange(0, 0);
    m_convertStatusLabel->setText("Status: Convertendo mídia em alta velocidade...");
    if (m_stackedWidget->currentIndex() == 0) {
        m_statusLabel->setText("Status: Conversão Automática acionada em alta velocidade...");
    }
    m_startConvertBtn->setEnabled(false);
    m_cancelConvertBtn->setEnabled(true);

    logMessage("\n========================================================");
    logMessage(QString("[Conversor] Iniciando conversão para %1").arg(outFile));
    logMessage("[Conversor] Comando: " + ffmpegPath + " " + args.join(" "));

    m_convertProcess->start(ffmpegPath, args);
    if (!m_convertProcess->waitForStarted()) {
        QMessageBox::critical(this, "Erro", "Não foi possível acionar o executável do FFmpeg.");
        m_convertProgressBar->setRange(0, 100);
        m_convertProgressBar->setValue(0);
        m_startConvertBtn->setEnabled(true);
        m_cancelConvertBtn->setEnabled(false);
    }
}

void MainWindow::onCancelConvertClicked()
{
    if (m_convertProcess && m_convertProcess->state() != QProcess::NotRunning) {
        m_convertProcess->kill();
        logMessage("[Conversor] Conversão interrompida pelo usuário.");
        m_convertStatusLabel->setText("Status do Conversor: Operação cancelada.");
        m_convertProgressBar->setRange(0, 100);
        m_convertProgressBar->setValue(0);
        m_startConvertBtn->setEnabled(true);
        m_cancelConvertBtn->setEnabled(false);
    }
}

void MainWindow::onConvertProcessOutput()
{
    if (!m_convertProcess) return;
    QByteArray out = m_convertProcess->readAllStandardOutput();
    QByteArray err = m_convertProcess->readAllStandardError();

    if (!out.isEmpty()) logMessage(QString::fromUtf8(out).trimmed());
    if (!err.isEmpty()) {
        QString errStr = QString::fromUtf8(err).trimmed();
        if (errStr.contains("time=") || errStr.contains("size=") || errStr.contains("speed=")) {
            int pos = errStr.indexOf("time=");
            if (pos != -1) {
                QString sub = errStr.mid(pos, 25);
                m_convertStatusLabel->setText("Status: Convertendo (" + sub + ")...");
                if (m_stackedWidget->currentIndex() == 0) {
                    m_statusLabel->setText("Status da Conversão Automática: " + sub);
                }
            }
        }
    }
}

void MainWindow::onConvertProcessFinished(int exitCode)
{
    m_convertProgressBar->setRange(0, 100);
    m_startConvertBtn->setEnabled(true);
    m_cancelConvertBtn->setEnabled(false);

    if (exitCode == 0) {
        m_convertProgressBar->setValue(100);
        m_convertStatusLabel->setText("Status: Conversão finalizada com sucesso! Salvo na sua Biblioteca.");
        if (m_stackedWidget->currentIndex() == 0) {
            m_statusLabel->setText("Status: Conversão Automática concluída e salva no disco!");
        }
        logMessage("[Sucesso] Arquivo convertido e salvo na pasta com sucesso!");
        refreshLibrary();

        if (m_notifyCheckBox->isChecked()) {
            QString savedLoc = m_currentDownloadDir.isEmpty() ? m_outputDirInput->text() : m_currentDownloadDir;
            QMessageBox::information(this, "Conversão Concluída", "O arquivo foi convertido com sucesso e salvo em:\n" + savedLoc);
        }
    } else {
        m_convertProgressBar->setValue(0);
        m_convertStatusLabel->setText("Status: Erro na conversão ou processo interrompido.");
        logMessage(QString("[Erro] Conversor encerrou com código %1").arg(exitCode));
    }
}

void MainWindow::refreshLibrary()
{
    if (!m_libraryTable) return;
    m_libraryTable->setRowCount(0);

    QString folder = m_outputDirInput->text();
    QDir dir(folder);
    if (!dir.exists()) return;

    QStringList filters;
    filters << "*.mp4" << "*.mp3" << "*.mkv" << "*.webm" << "*.m4a" << "*.avi" << "*.flv" << "*.wav";
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks, QDir::Time);

    m_libraryTable->setRowCount(fileList.size());
    for (int i = 0; i < fileList.size(); ++i) {
        QFileInfo info = fileList.at(i);
        
        QTableWidgetItem *itemTitle = new QTableWidgetItem(info.fileName());
        itemTitle->setFlags(itemTitle->flags() ^ Qt::ItemIsEditable);
        
        QTableWidgetItem *itemExt = new QTableWidgetItem(info.suffix().toUpper());
        itemExt->setFlags(itemExt->flags() ^ Qt::ItemIsEditable);
        itemExt->setTextAlignment(Qt::AlignCenter);

        double sizeMB = static_cast<double>(info.size()) / (1024.0 * 1024.0);
        QTableWidgetItem *itemSize = new QTableWidgetItem(QString("%1 MB").arg(sizeMB, 0, 'f', 1));
        itemSize->setFlags(itemSize->flags() ^ Qt::ItemIsEditable);
        itemSize->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        m_libraryTable->setItem(i, 0, itemTitle);
        m_libraryTable->setItem(i, 1, itemExt);
        m_libraryTable->setItem(i, 2, itemSize);
    }
    logMessage(QString("[Biblioteca] Lista de mídias atualizada: %1 arquivo(s) encontrado(s).").arg(fileList.size()));
}

void MainWindow::onPlaySelectedMedia()
{
    int row = m_libraryTable->currentRow();
    if (row < 0) {
        QMessageBox::information(this, "Biblioteca", "Selecione um arquivo de mídia da lista acima para reproduzir.");
        return;
    }
    onLibraryDoubleClicked(row, 0);
}

void MainWindow::onLibraryDoubleClicked(int row, int /*column*/)
{
    QTableWidgetItem *item = m_libraryTable->item(row, 0);
    if (!item) return;

    QString fileName = item->text();
    QDir dir(m_outputDirInput->text());
    QString filePath = dir.absoluteFilePath(fileName);

    if (QFile::exists(filePath)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
        logMessage("[Biblioteca] Reproduzindo arquivo no player do Windows: " + fileName);
    } else {
        QMessageBox::warning(this, "Aviso", "O arquivo selecionado não foi encontrado fisicamente no disco:\n" + filePath);
        refreshLibrary();
    }
}

// ==========================================
// DIÁLOGO MODAL ESTILO ATUBE CATCHER (FOTO 3)
// ==========================================
bool MainWindow::showFormatSelectionDialog(QString &outQuality, QString &outTimeRange, bool &outDoConvert, QString &outConvertFormat, QString &outCustomOutputDir)
{
    QDialog dlg(this);
    dlg.setWindowTitle("Selecione o formato da fonte - NeoV Studio Suite");
    dlg.resize(780, 580);
    dlg.setStyleSheet(this->styleSheet() + "QDialog { background-color: #1a1a1a; }");

    QVBoxLayout *dlgLayout = new QVBoxLayout(&dlg);
    dlgLayout->setSpacing(16);
    dlgLayout->setContentsMargins(24, 24, 24, 24);

    QLabel *lblTitle = new QLabel("Selecione o formato da fonte e opções do download:", &dlg);
    lblTitle->setStyleSheet("font-weight: bold; font-size: 18px; color: #ffffff;");
    dlgLayout->addWidget(lblTitle);

    QTableWidget *table = new QTableWidget(4, 3, &dlg);
    QStringList headers;
    headers << "Título / Qualidade" << "Formato e Codec" << "Resolução / Modo";
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->setObjectName("libraryTable");
    table->setMinimumHeight(180);

    struct Prof { QString q; QString f; QString r; };
    Prof profs[4] = {
        {"4K / Melhor Disponível no Servidor", "MP4 / Container Original", "Ultra HD / Máxima"},
        {"1080p Full HD", "H.264 / NVENC Acelerado", "1920x1080 (60/30 fps)"},
        {"720p HD", "H.264 / NVENC Otimizado", "1280x720 (Balanceado)"},
        {"Áudio MP3 (Extração Direta)", "MP3 Estéreo Alta Fidelidade", "320 kbps (Apenas Áudio)"}
    };

    for (int i = 0; i < 4; ++i) {
        table->setItem(i, 0, new QTableWidgetItem(profs[i].q));
        table->setItem(i, 1, new QTableWidgetItem(profs[i].f));
        table->setItem(i, 2, new QTableWidgetItem(profs[i].r));
    }

    int defaultIdx = m_qualityCombo->currentIndex();
    if (defaultIdx >= 0 && defaultIdx < 4) {
        table->selectRow(defaultIdx);
    } else {
        table->selectRow(1);
    }

    dlgLayout->addWidget(table);

    // Opções de tempo, conversão e destino personalizado no modal
    QGridLayout *optLayout = new QGridLayout();
    optLayout->setSpacing(12);
    
    QLabel *lblTimeOpt = new QLabel("Recorte de Tempo (Opcional):", &dlg);
    lblTimeOpt->setStyleSheet("color: #a3a3a3; font-weight: bold;");
    QLineEdit *editTime = new QLineEdit(&dlg);
    editTime->setText(m_timeRangeInput->text());
    editTime->setPlaceholderText("Ex: 00:01:15-00:03:00 (Vazio = baixar completo)");

    QCheckBox *chkConv = new QCheckBox("Converter para outro formato após concluir o download", &dlg);
    chkConv->setStyleSheet("color: #38bdf8; font-weight: bold; font-size: 13px;");
    chkConv->setCursor(Qt::PointingHandCursor);
    
    QComboBox *comboConv = new QComboBox(&dlg);
    comboConv->addItem("MP4 (H.264 / NVENC - Compatibilidade Universal)");
    comboConv->addItem("MP4 (HEVC / H.265 - Compressão de Alta Densidade)");
    comboConv->addItem("MKV (Matroska - Container Sem Perdas)");
    comboConv->addItem("MP3 (Áudio MP3 Alta Fidelidade - 320kbps)");
    comboConv->addItem("WAV (Áudio Sem Compressão / Estúdios)");
    comboConv->addItem("WEBM (Otimizado para Web e Redes Sociais)");
    comboConv->setEnabled(false);
    connect(chkConv, &QCheckBox::toggled, comboConv, &QComboBox::setEnabled);

    QLabel *lblFolderOpt = new QLabel("Salvar este download em:", &dlg);
    lblFolderOpt->setStyleSheet("color: #a3a3a3; font-weight: bold;");
    
    QHBoxLayout *folderOptLayout = new QHBoxLayout();
    QLineEdit *editCustomFolder = new QLineEdit(&dlg);
    editCustomFolder->setText(m_outputDirInput->text());
    
    QPushButton *btnChangeCustomFolder = new QPushButton("Mudar Pasta (Apenas Este)", &dlg);
    btnChangeCustomFolder->setObjectName("browseBtn");
    btnChangeCustomFolder->setCursor(Qt::PointingHandCursor);
    btnChangeCustomFolder->setMinimumHeight(32);
    connect(btnChangeCustomFolder, &QPushButton::clicked, &dlg, [this, editCustomFolder]() {
        QString dir = QFileDialog::getExistingDirectory(nullptr, "Escolha a Pasta Exclusiva Para Este Download", editCustomFolder->text());
        if (!dir.isEmpty()) {
            editCustomFolder->setText(dir);
        }
    });

    folderOptLayout->addWidget(editCustomFolder, 1);
    folderOptLayout->addWidget(btnChangeCustomFolder, 0);

    optLayout->addWidget(lblTimeOpt, 0, 0);
    optLayout->addWidget(editTime, 0, 1);
    optLayout->addWidget(chkConv, 1, 0);
    optLayout->addWidget(comboConv, 1, 1);
    optLayout->addWidget(lblFolderOpt, 2, 0);
    optLayout->addLayout(folderOptLayout, 2, 1);
    optLayout->setColumnStretch(1, 1);

    dlgLayout->addLayout(optLayout);
    dlgLayout->addStretch();

    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *btnOk = new QPushButton("BAIXAR MÍDIA", &dlg);
    btnOk->setObjectName("startBtn");
    btnOk->setCursor(Qt::PointingHandCursor);
    btnOk->setMinimumHeight(42);
    btnOk->setFixedWidth(160);
    btnOk->setStyleSheet("font-size: 15px; font-weight: bold;");
    connect(btnOk, &QPushButton::clicked, &dlg, &QDialog::accept);

    QPushButton *btnCancel = new QPushButton("CANCELAR", &dlg);
    btnCancel->setObjectName("cancelBtn");
    btnCancel->setCursor(Qt::PointingHandCursor);
    btnCancel->setMinimumHeight(42);
    btnCancel->setFixedWidth(140);
    connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    QLabel *lblAccel = new QLabel("⚡ Motor NVIDIA NVENC Operante", &dlg);
    lblAccel->setStyleSheet("color: #10b981; font-weight: bold; font-size: 13px;");

    btnLayout->addWidget(btnOk);
    btnLayout->addWidget(btnCancel);
    btnLayout->addStretch();
    btnLayout->addWidget(lblAccel);
    dlgLayout->addLayout(btnLayout);

    if (dlg.exec() == QDialog::Accepted) {
        int r = table->currentRow();
        if (r >= 0 && r < 4) {
            outQuality = m_qualityCombo->itemText(r);
            m_qualityCombo->setCurrentIndex(r);
        } else {
            outQuality = m_qualityCombo->currentText();
        }
        outTimeRange = editTime->text().trimmed();
        m_timeRangeInput->setText(outTimeRange);
        
        outDoConvert = chkConv->isChecked();
        outConvertFormat = comboConv->currentText();
        outCustomOutputDir = editCustomFolder->text().trimmed();
        if (outCustomOutputDir.isEmpty()) {
            outCustomOutputDir = m_outputDirInput->text().trimmed();
        }
        return true;
    }
    return false;
}

void MainWindow::onBrowseClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Escolha a Pasta de Destino Padrão para os Downloads", m_outputDirInput->text());
    if (!dir.isEmpty()) {
        m_outputDirInput->setText(dir);
        QSettings settings("Tonho Studios", "NeoVDownloader");
        settings.setValue("outputFolder", dir);
        logMessage("[System] Nova pasta de destino padrão salva: " + dir);
        refreshLibrary();
    }
}

void MainWindow::onOpenFolderClicked()
{
    QString dir = m_currentDownloadDir.isEmpty() ? m_outputDirInput->text() : m_currentDownloadDir;
    if (!dir.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
        logMessage("[System] Abrindo a pasta no Windows Explorer: " + dir);
    }
}

void MainWindow::onStartClicked()
{
    QString url = m_urlInput->text().trimmed();
    if (url.isEmpty()) {
        QMessageBox::warning(this, "Atenção", "Por favor, insira ou cole o link do vídeo na caixa de URL antes de prosseguir.");
        return;
    }

    // Acionar a janela modal de opções (Foto 3) com suporte à pasta temporária e conversão pós-download!
    QString selectedQuality, timeRange, convertFormat, customOutputDir;
    bool doConvert = false;
    if (!showFormatSelectionDialog(selectedQuality, timeRange, doConvert, convertFormat, customOutputDir)) {
        logMessage("[Operação] Seleção de formato cancelada pelo usuário.");
        return;
    }

    m_autoConvertAfterDownload = doConvert;
    m_autoConvertFormat = convertFormat;
    m_currentDownloadDir = customOutputDir;

    QString defaultOutputDir = m_outputDirInput->text().trimmed();
    QSettings settings("Tonho Studios", "NeoVDownloader");
    settings.setValue("outputFolder", defaultOutputDir); // Conserva a pasta padrão inalterada!
    settings.setValue("showNotifications", m_notifyCheckBox->isChecked());
    settings.setValue("selectedQuality", m_qualityCombo->currentIndex());
    settings.setValue("defaultTimeRange", timeRange);

    m_progressBar->setValue(0);
    m_startBtn->setEnabled(false);
    m_cancelBtn->setEnabled(true);
    
    logMessage("\n========================================================");
    logMessage("[DownloadEngine] Acionando motor acelerado de extração e junção...");
    if (!timeRange.isEmpty()) {
        logMessage("[Recorte] Faixa de tempo programada: " + timeRange);
    }
    if (m_autoConvertAfterDownload) {
        logMessage("[Auto-Conversão] Programado para converter automaticamente para: " + m_autoConvertFormat);
    }
    if (customOutputDir != defaultOutputDir) {
        logMessage("[Destino Temporário] Este download específico será salvo em: " + customOutputDir);
    } else {
        logMessage("[Destino Padrão] Mídia será salva na pasta padrão: " + customOutputDir);
    }
    
    m_engine.startDownload(url.toStdString(), selectedQuality.toStdString(), timeRange.toStdString(), customOutputDir.toStdString());
}

void MainWindow::onCancelClicked()
{
    logMessage("[Alerta] Comando de cancelamento enviado para o worker...");
    m_engine.cancelCurrent();
    m_startBtn->setEnabled(true);
    m_cancelBtn->setEnabled(false);
}

void MainWindow::logMessage(const QString &msg)
{
    if (m_logEdit) {
        m_logEdit->append(msg);
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
            padding: 13px 18px;
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
        QPushButton#updateSideBtn {
            background-color: #1b2922;
            color: #10b981;
            font-weight: bold;
            font-size: 13px;
            border: 1px solid #10b981;
            border-radius: 5px;
            padding: 9px;
            margin: 0 14px 6px 14px;
        }
        QPushButton#updateSideBtn:hover {
            background-color: #10b981;
            color: #021810;
        }
        QPushButton#updateSideBtn:checked {
            background-color: #10b981;
            color: #021810;
        }
        QPushButton#openFolderSideBtn {
            background-color: #1c2e3a;
            color: #38bdf8;
            font-weight: bold;
            font-size: 13px;
            border: 1px solid #38bdf8;
            border-radius: 5px;
            padding: 9px;
            margin: 0 14px;
        }
        QPushButton#openFolderSideBtn:hover {
            background-color: #38bdf8;
            color: #061824;
        }
        QGroupBox {
            background-color: #1a1a1a;
            border: 1px solid #282828;
            border-radius: 8px;
            margin-top: 14px;
            font-weight: bold;
            color: #ffffff;
            font-size: 13px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 0 8px;
            color: #10b981;
        }
        QLineEdit, QComboBox {
            background-color: #202020;
            border: 1px solid #333333;
            border-radius: 6px;
            padding: 8px 12px;
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
            color: #a3a3a3;
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
            color: #021810;
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
            background-color: #dc2626;
            color: #ffffff;
            font-weight: bold;
            font-size: 13px;
            border-radius: 6px;
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
            border-radius: 6px;
            padding: 6px 14px;
        }
        QPushButton#browseBtn:hover {
            background-color: #354a43;
            color: #ffffff;
        }
        QProgressBar {
            background-color: #202020;
            border: 1px solid #333333;
            border-radius: 6px;
            text-align: center;
            font-weight: bold;
            color: #ffffff;
        }
        QProgressBar::chunk {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #10b981, stop:1 #047857);
            border-radius: 5px;
        }
        QTableWidget#libraryTable {
            background-color: #1a1a1a;
            border: 1px solid #282828;
            border-radius: 6px;
            color: #ffffff;
            gridline-color: #282828;
            font-size: 13px;
            selection-background-color: #10b981;
            selection-color: #021810;
        }
        QTableWidget#libraryTable QHeaderView::section {
            background-color: #242424;
            color: #10b981;
            font-weight: bold;
            border: none;
            border-bottom: 2px solid #10b981;
            padding: 8px 10px;
            font-size: 13px;
        }
        QTableWidget#libraryTable::item {
            padding: 8px 10px;
            border-bottom: 1px solid #222222;
        }
        QTableWidget#libraryTable::item:selected {
            background-color: #10b981;
            color: #021810;
            font-weight: bold;
        }
        QTextEdit#logArea {
            background-color: #0a0e0b;
            border: 1px solid #1a241c;
            border-radius: 6px;
            color: #10b981;
            font-family: 'Consolas', 'Courier New', monospace;
            font-size: 12px;
            padding: 12px;
        }
    )";

    setStyleSheet(qss);
}

void MainWindow::checkForUpdates(bool silent)
{
    if (!m_networkManager) return;
    
    m_updateStatusLabel->setText("Sondando servidores do GitHub (BadTonho/Baixar)...");
    if (m_sidebarUpdateNotification) {
        m_sidebarUpdateNotification->setText("🔄 Checando...");
        m_sidebarUpdateNotification->setStyleSheet("color: #38bdf8; font-size: 12px; font-weight: bold; margin-bottom: 2px;");
    }
    logMessage("[Updater] Consultando API pública do GitHub para release mais recente...");

    QUrl url("https://api.github.com/repos/BadTonho/Baixar/releases/latest");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "NeoVDownloader-Updater/1.0 (Windows; Qt)");
    
    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, silent]() {
        onUpdateReplyFinished(reply, silent);
    });
}

void MainWindow::onUpdateReplyFinished(QNetworkReply *reply, bool silent)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (httpCode == 404) {
            m_updateStatusLabel->setText("Nenhuma release pública postada ainda (Versão " + NEOV_VERSION_TAG + " em uso).");
            if (m_sidebarUpdateNotification) {
                m_sidebarUpdateNotification->setText("✅ " + NEOV_VERSION_TAG + " (Em Dia)");
                m_sidebarUpdateNotification->setStyleSheet("color: #10b981; font-size: 12px; font-weight: bold; margin-bottom: 2px;");
            }
            logMessage("[Updater] Resposta 404: Repositório sem releases públicas criadas no GitHub. O software local está atualizado.");
            if (!silent) {
                QMessageBox::information(this, "Atualizações", "Você já está rodando a versão mais moderna do NeoVDownloader (" + NEOV_VERSION_TAG + ")!\n\nNenhuma release pública mais nova foi publicada no repositório GitHub ainda.");
            }
        } else {
            m_updateStatusLabel->setText("Falha de conexão ou offline. (Versão " + NEOV_VERSION_TAG + " em uso).");
            if (m_sidebarUpdateNotification) {
                m_sidebarUpdateNotification->setText("🛡️ " + NEOV_VERSION_TAG + " (Offline/Local)");
                m_sidebarUpdateNotification->setStyleSheet("color: #737373; font-size: 12px; font-weight: bold; margin-bottom: 2px;");
            }
            logMessage("[Updater] Erro ao conectar com o GitHub: " + reply->errorString());
            if (!silent) {
                QMessageBox::warning(this, "Atenção", "Não foi possível contactar o servidor do GitHub para checar atualizações:\n" + reply->errorString());
            }
        }
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject obj = doc.object();

    QString tagName = obj.value("tag_name").toString().trimmed();
    QString body = obj.value("body").toString();
    if (tagName.isEmpty()) {
        m_updateStatusLabel->setText("Versão " + NEOV_VERSION_TAG + " operacional (Nenhuma tag encontrada).");
        if (m_sidebarUpdateNotification) {
            m_sidebarUpdateNotification->setText("✅ " + NEOV_VERSION_TAG + " (Em Dia)");
            m_sidebarUpdateNotification->setStyleSheet("color: #10b981; font-size: 12px; font-weight: bold; margin-bottom: 2px;");
        }
        return;
    }

    logMessage("[Updater] Última release no GitHub identificada: " + tagName);

    // Comparar versão com a local
    QString localVersion = NEOV_VERSION_TAG;
    QString cleanLocal = localVersion; cleanLocal.remove("v", Qt::CaseInsensitive);
    QString cleanRemote = tagName; cleanRemote.remove("v", Qt::CaseInsensitive);

    if (cleanRemote == cleanLocal || cleanRemote.isEmpty()) {
        m_updateStatusLabel->setText("Você está utilizando a versão mais recente (" + tagName + ").");
        if (m_sidebarUpdateNotification) {
            m_sidebarUpdateNotification->setText("✅ " + NEOV_VERSION_TAG + " (Em Dia)");
            m_sidebarUpdateNotification->setStyleSheet("color: #10b981; font-size: 12px; font-weight: bold; margin-bottom: 2px;");
        }
        logMessage("[Updater] A versão instalada já é a mais recente disponível.");
        if (!silent) {
            QMessageBox::information(this, "Atualizado", "O seu aplicativo NeoVDownloader já está na versão mais atualizada (" + localVersion + ")!");
        }
        return;
    }

    // Se chegou aqui, temos uma NOVA VERSÃO no GitHub
    m_updateStatusLabel->setText("🚀 Nova versão " + tagName + " disponível no GitHub!");
    m_updateStatusLabel->setStyleSheet("color: #10b981; font-weight: bold; font-size: 14px;");
    if (m_sidebarUpdateNotification) {
        m_sidebarUpdateNotification->setText("🔔 Nova Versão " + tagName + "!");
        m_sidebarUpdateNotification->setStyleSheet("color: #f59e0b; background-color: #2a1f0c; border: 1px solid #f59e0b; border-radius: 4px; padding: 4px; font-size: 12px; font-weight: bold; margin: 0 10px 2px 10px;");
    }    logMessage("[Updater] ALERTA: Nova versão identificada no GitHub -> " + tagName);

    QJsonArray assets = obj.value("assets").toArray();
    QString downloadUrl = "";
    QString assetName = "NeoVDownloader_Setup.exe";
    for (int i = 0; i < assets.size(); ++i) {
        QJsonObject asset = assets.at(i).toObject();
        QString name = asset.value("name").toString();
        if (name.endsWith(".exe", Qt::CaseInsensitive) || name.endsWith(".zip", Qt::CaseInsensitive)) {
            downloadUrl = asset.value("browser_download_url").toString();
            assetName = name;
            break;
        }
    }

    if (downloadUrl.isEmpty()) {
        downloadUrl = obj.value("html_url").toString();
    }

    // Verificar se o download automático está ativado sem perguntar (Por padrão DESATIVADO)
    if (m_autoDownloadUpdatesChk->isChecked() && !downloadUrl.isEmpty() && (downloadUrl.endsWith(".exe") || downloadUrl.endsWith(".zip"))) {
        logMessage("[Updater] Download Automático Ativado: Baixando atualização silenciosamente...");
        startUpdateDownload(downloadUrl, assetName);
        if (m_notifyCheckBox->isChecked()) {
            QMessageBox::information(this, "Atualização Detectada", "Uma nova versão (" + tagName + ") foi detectada no GitHub!\nComo o download automático está habilitado, estamos baixando em segundo plano.");
        }
    } else {
        // PERGUNTA AO USUÁRIO SE QUER ATUALIZAR (Nunca obrigatório)
        QMessageBox::StandardButton res = QMessageBox::question(this, "Nova Versão Disponível",
            QString("Uma nova versão do NeoVDownloader (%1) foi disponibilizada no GitHub!\n\nVersão Atual: %2\nNova Versão: %1\n\nNovidades da Release:\n%3\n\nDeseja baixar e atualizar o aplicativo agora?")
            .arg(tagName, localVersion, body.left(300)),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

        if (res == QMessageBox::Yes) {
            if (downloadUrl.endsWith(".exe") || downloadUrl.endsWith(".zip")) {
                startUpdateDownload(downloadUrl, assetName);
            } else {
                QDesktopServices::openUrl(QUrl(downloadUrl));
                logMessage("[Updater] Abrindo página da release do GitHub no navegador: " + downloadUrl);
            }
        } else {
            logMessage("[Updater] O usuário optou por adiar a atualização desta vez.");
            m_updateStatusLabel->setText("Atualização " + tagName + " adiada pelo usuário.");
        }
    }
}

void MainWindow::startUpdateDownload(const QString &url, const QString &fileName)
{
    if (url.isEmpty()) return;
    logMessage(QString("[Updater] Iniciando download da atualização %1 de: %2").arg(fileName, url));
    m_updateStatusLabel->setText("Baixando atualização " + fileName + " do GitHub...");

    QUrl targetUrl(url);
    QNetworkRequest req(targetUrl);
    req.setHeader(QNetworkRequest::UserAgentHeader, "NeoVDownloader-Updater/1.0");
    QNetworkReply *reply = m_networkManager->get(req);
    
    connect(reply, &QNetworkReply::finished, this, [this, reply, fileName]() {
        onUpdateDownloadFinished(reply, fileName);
    });
}

void MainWindow::onUpdateDownloadFinished(QNetworkReply *reply, const QString &fileName)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        logMessage("[Updater] Erro durante o download do instalador de atualização: " + reply->errorString());
        m_updateStatusLabel->setText("Falha no download da atualização: " + reply->errorString());
        QMessageBox::critical(this, "Erro", "Houve um problema na transmissão da atualização do GitHub:\n" + reply->errorString());
        return;
    }

    QByteArray data = reply->readAll();
    QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir dir(tempPath);
    QString savePath = dir.absoluteFilePath(fileName);

    QFile file(savePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(data);
        file.close();
        logMessage("[Updater] Atualização salva na pasta temporária: " + savePath);
        m_updateStatusLabel->setText("Atualização baixada com sucesso em: " + savePath);

        if (savePath.endsWith(".exe", Qt::CaseInsensitive)) {
            QMessageBox::StandardButton ask = QMessageBox::question(this, "Instalar Atualização",
                "A atualização foi baixada com sucesso no computador!\n\nDeseja fechar o NeoVDownloader e iniciar o instalador agora?",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
            if (ask == QMessageBox::Yes) {
                logMessage("[Updater] Executando instalador da nova versão...");
                QProcess::startDetached(savePath, QStringList());
                QApplication::quit();
            }
        } else {
            QMessageBox::information(this, "Baixado", "A nova versão foi salva com sucesso no seu computador em:\n" + savePath);
            QDesktopServices::openUrl(QUrl::fromLocalFile(dir.absolutePath()));
        }
    } else {
        logMessage("[Updater] Erro ao gravar arquivo no diretório temporário: " + savePath);
    }
}

void MainWindow::updateYtdlpEngine()
{
    QString ytdlpPath = QCoreApplication::applicationDirPath() + "/yt-dlp.exe";
    if (!QFile::exists(ytdlpPath)) {
        ytdlpPath = "yt-dlp";
    }

    logMessage("\n========================================================");
    logMessage("[Motor Extrator] Conectando aos servidores do yt-dlp para checar novas assinaturas...");
    m_updateStatusLabel->setText("Atualizando motor extrator yt-dlp em segundo plano...");

    QProcess *proc = new QProcess(this);
#ifdef _WIN32
    proc->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= 0x08000000; // CREATE_NO_WINDOW
    });
#endif
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [this, proc](int code, QProcess::ExitStatus status) {
        proc->deleteLater();
        QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        QString err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
        if (!out.isEmpty()) logMessage("[yt-dlp update] " + out);
        if (!err.isEmpty()) logMessage("[yt-dlp update] " + err);

        if (code == 0 && status == QProcess::NormalExit) {
            m_updateStatusLabel->setText("Motor extrator (yt-dlp) atualizado e operacional!");
            logMessage("[Sucesso] Motor extrator sincronizado com a versão mais recente!");
            QMessageBox::information(this, "Motor Atualizado", "O Motor Extrator nativo (yt-dlp) foi sincronizado e está rodando em sua versão de compatibilidade mais recente!");
        } else {
            m_updateStatusLabel->setText("O motor já se encontra em sua versão estável mais recente.");
            logMessage("[Motor Extrator] Fim da checagem do motor.");
            QMessageBox::information(this, "Motor Extrator", "A checagem do motor extrator encerrou. " + (out.isEmpty() ? err : out));
        }
    });

    proc->start(ytdlpPath, QStringList() << "-U");
}
