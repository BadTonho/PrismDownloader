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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("NeoVDownloader - Turbo Edition");
    resize(960, 580);

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
        logMessage(QString("[System] Placa gráfica ativa no motor: %1 (Codec: %2)").arg(gpuName, codec));
    } else {
        if (m_gpuModelLabel) m_gpuModelLabel->setText("Nenhuma aceleração dedicada NVIDIA foi localizada");
        if (m_gpuCodecLabel) m_gpuCodecLabel->setText("Codec Fallback CPU Padrão");
        if (m_gpuStatusLabel) {
            m_gpuStatusLabel->setText("MODO FALLBACK CPU (Multi-thread)");
            m_gpuStatusLabel->setStyleSheet("color: #f59e0b; font-weight: bold; font-size: 13px;");
        }
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
                    refreshLibrary(); // Atualizar a biblioteca automaticamente ao terminar o download!

                    if (m_notifyCheckBox->isChecked()) {
                        QMessageBox::information(this, "Sucesso", "Download finalizado em velocidade máxima!\nOs arquivos foram salvos na pasta de destino.");
                    }
                }
            }
        }, Qt::QueuedConnection);
    });

    // Carregar a biblioteca inicial
    refreshLibrary();
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

    m_navLibraryBtn = new QPushButton("Biblioteca", sidebar);
    m_navLibraryBtn->setObjectName("navBtn");
    m_navLibraryBtn->setCheckable(true);
    m_navLibraryBtn->setCursor(Qt::PointingHandCursor);

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
    navGroup->addButton(m_navLibraryBtn, 1);
    navGroup->addButton(m_navLogsBtn, 2);
    navGroup->addButton(m_navInfoBtn, 3);

    sidebarLayout->addWidget(m_navDownloadBtn);
    sidebarLayout->addWidget(m_navLibraryBtn);
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

    // ---> TELA 1: BIBLIOTECA DE MÍDIAS <---
    QWidget *pageLibrary = new QWidget(m_stackedWidget);
    QVBoxLayout *libLayout = new QVBoxLayout(pageLibrary);
    libLayout->setSpacing(14);
    libLayout->setContentsMargins(24, 20, 24, 20);

    QHBoxLayout *libTopLayout = new QHBoxLayout();
    QLabel *libTitle = new QLabel("Minha Biblioteca (Arquivos na Pasta de Destino):", pageLibrary);
    libTitle->setStyleSheet("font-weight: bold; color: #10b981; font-size: 14px;");
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

    // ---> TELA 2: TERMINAL DE LOGS <---
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

    // ---> TELA 3: INFORMAÇÕES E HARDWARE COM DESIGN MODERNO <---
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
    QLabel *lblAppNameVal = new QLabel("NeoVDownloader (Turbo GPU Edition)", appInfoGroup);
    lblAppNameVal->setStyleSheet("color: #ffffff; font-weight: bold; font-size: 13px;");

    QLabel *lblAppVerKey = new QLabel("Versão Atual:", appInfoGroup);
    lblAppVerKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    QLabel *lblAppVerVal = new QLabel("1.0.0 (Estável / Release)", appInfoGroup);
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

    // Conectar navegação e botões
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
        if (index == 1) { // Se abriu a aba Biblioteca, revigorar a lista de vídeos baixados!
            refreshLibrary();
        }
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
        QTableWidget#libraryTable {
            background-color: #1a1a1a;
            border: 1px solid #262626;
            border-radius: 6px;
            color: #ffffff;
            gridline-color: #262626;
            font-size: 13px;
            selection-background-color: #10b981;
            selection-color: #031c12;
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
            padding: 6px 10px;
            border-bottom: 1px solid #222222;
        }
        QTableWidget#libraryTable::item:selected {
            background-color: #10b981;
            color: #031c12;
            font-weight: bold;
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
        refreshLibrary();
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
