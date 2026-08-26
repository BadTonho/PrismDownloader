#include "MainWindowUiBuilder.h"

#include "LibraryView.h"
#include "LogHighlighter.h"
#include "MainWindow.h"

#include <QAbstractItemView>
#include <QClipboard>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QObject>

namespace {
constexpr int kMaximumLogEntries = 5000;
}

void MainWindowUiBuilder::build(MainWindow *window,
                                const QString &versionTag,
                                const QString &versionNumber,
                                const AppSettings &settings)
{
    // These aliases keep the UI builder independent from the rest of the
    // window logic while preserving the existing widget ownership model.
#define m_allLogs window->m_allLogs
#define m_autoDownloadUpdatesChk window->m_autoDownloadUpdatesChk
#define m_browseDirBtn window->m_browseDirBtn
#define m_cancelAllBtn window->m_cancelAllBtn
#define m_cancelBtn window->m_cancelBtn
#define m_cancelConvertBtn window->m_cancelConvertBtn
#define m_checkUpdateBtn window->m_checkUpdateBtn
#define m_checkUpdatesOnStartChk window->m_checkUpdatesOnStartChk
#define m_clearLogsBtn window->m_clearLogsBtn
#define m_concurrencyCombo window->m_concurrencyCombo
#define m_convertBrowseBtn window->m_convertBrowseBtn
#define m_convertEngineLabel window->m_convertEngineLabel
#define m_convertFormatCombo window->m_convertFormatCombo
#define m_convertInput window->m_convertInput
#define m_convertProgressBar window->m_convertProgressBar
#define m_convertStatusLabel window->m_convertStatusLabel
#define m_copyLogsBtn window->m_copyLogsBtn
#define m_downloadsQueueTable window->m_downloadsQueueTable
#define m_etaLabel window->m_etaLabel
#define m_filterAllBtn window->m_filterAllBtn
#define m_filterErrorsBtn window->m_filterErrorsBtn
#define m_filterGeneralBtn window->m_filterGeneralBtn
#define m_filterProcessesBtn window->m_filterProcessesBtn
#define m_gpuCodecLabel window->m_gpuCodecLabel
#define m_gpuModelLabel window->m_gpuModelLabel
#define m_gpuStatusLabel window->m_gpuStatusLabel
#define m_libraryView window->m_libraryView
#define m_logEdit window->m_logEdit
#define m_logSearchInput window->m_logSearchInput
#define m_logSummaryLabel window->m_logSummaryLabel
#define m_navConverterBtn window->m_navConverterBtn
#define m_navDownloadBtn window->m_navDownloadBtn
#define m_navInfoBtn window->m_navInfoBtn
#define m_navLibraryBtn window->m_navLibraryBtn
#define m_navLogsBtn window->m_navLogsBtn
#define m_notifyCheckBox window->m_notifyCheckBox
#define m_openFolderBtn window->m_openFolderBtn
#define m_outputDirInput window->m_outputDirInput
#define m_progressBar window->m_progressBar
#define m_sidebarUpdateBtn window->m_sidebarUpdateBtn
#define m_sidebarUpdateNotification window->m_sidebarUpdateNotification
#define m_speedLabel window->m_speedLabel
#define m_stackedWidget window->m_stackedWidget
#define m_startBtn window->m_startBtn
#define m_startConvertBtn window->m_startConvertBtn
#define m_statusLabel window->m_statusLabel
#define m_updateAppBtn window->m_updateAppBtn
#define m_updateProgressBar window->m_updateProgressBar
#define m_updateStatusLabel window->m_updateStatusLabel
#define m_updateYtdlpBtn window->m_updateYtdlpBtn
#define m_urlInput window->m_urlInput
#define m_ytdlpStatusLabel window->m_ytdlpStatusLabel

    QWidget *centralWidget = new QWidget(window);
    window->setCentralWidget(centralWidget);

    QHBoxLayout *rootLayout = new QHBoxLayout(centralWidget);
    rootLayout->setSpacing(0);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    // ==========================================
    // 1. BARRA LATERAL (SIDEBAR NAVIGATION)
    // ==========================================
    QFrame *sidebar = new QFrame(window);
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

    QButtonGroup *navGroup = new QButtonGroup(window);
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

    m_sidebarUpdateNotification = new QLabel(versionTag + " (Em Dia)", sidebar);
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
    m_stackedWidget = new QStackedWidget(window);
    m_stackedWidget->setObjectName("mainArea");
    rootLayout->addWidget(m_stackedWidget);

    // ---> TELA 0: DOWNLOADS <---
    QWidget *pageDownloads = new QWidget(m_stackedWidget);
    pageDownloads->setObjectName("downloadPage");
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

    m_startBtn = new QPushButton("ADICIONAR À FILA", pageDownloads);
    m_startBtn->setObjectName("startBtn");
    m_startBtn->setCursor(Qt::PointingHandCursor);
    m_startBtn->setMinimumHeight(44);
    m_startBtn->setFixedWidth(185);
    m_startBtn->setStyleSheet("font-size: 15px; font-weight: bold;");

    topInputLayout->addWidget(m_urlInput, 1);
    topInputLayout->addWidget(m_startBtn, 0);
    downloadsLayout->addLayout(topInputLayout);

    // LINHA SECUNDÁRIA: PASTA DE DESTINO DOS DOWNLOADS
    QHBoxLayout *saveLayout = new QHBoxLayout();
    saveLayout->setSpacing(12);
    saveLayout->setContentsMargins(0, 0, 0, 8);

    QLabel *lblSave = new QLabel("Salvar downloads em:", pageDownloads);
    lblSave->setStyleSheet("color: #a3a3a3; font-weight: bold; font-size: 13px;");

    m_outputDirInput = new QLineEdit(pageDownloads);
    m_outputDirInput->setReadOnly(false);
    m_browseDirBtn = new QPushButton("Alterar...", pageDownloads);
    m_browseDirBtn->setObjectName("browseBtn");
    m_browseDirBtn->setCursor(Qt::PointingHandCursor);
    m_browseDirBtn->setMinimumHeight(32);

    QString savedFolder = settings.outputFolder;
    if (savedFolder.isEmpty() || !QDir(savedFolder).exists()) {
        savedFolder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (savedFolder.isEmpty()) savedFolder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    }
    m_outputDirInput->setText(savedFolder);

    saveLayout->addWidget(lblSave);
    saveLayout->addWidget(m_outputDirInput, 1);
    saveLayout->addWidget(m_browseDirBtn, 0);

    downloadsLayout->addLayout(saveLayout);

    // PAINEL CENTRAL DE PROCESSAMENTO E MONITORAMENTO AO VIVO
    QGroupBox *centralPanel = new QGroupBox("Área de Processamento e Monitoramento de Download", pageDownloads);
    QVBoxLayout *centerLayout = new QVBoxLayout(centralPanel);
    centerLayout->setSpacing(16);
    centerLayout->setContentsMargins(20, 30, 20, 24);

    m_statusLabel = new QLabel("Status: Pronto. Aguardando você inserir uma URL acima para começar...", centralPanel);
    m_statusLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: #10b981;");

    m_progressBar = new QProgressBar(centralPanel);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setMinimumHeight(28);
    m_progressBar->setStyleSheet("font-size: 14px;");

    QHBoxLayout *statsLayout = new QHBoxLayout();
    m_speedLabel = new QLabel("Velocidade de Download: 0.0 MB/s", centralPanel);
    m_speedLabel->setStyleSheet("font-size: 13px; color: #cbd5e1;");
    m_etaLabel = new QLabel("Tempo Restante: --:--", centralPanel);
    m_etaLabel->setStyleSheet("font-size: 13px; color: #cbd5e1;");
    statsLayout->addWidget(m_speedLabel);
    statsLayout->addStretch();
    statsLayout->addWidget(m_etaLabel);

    QHBoxLayout *actionBottomLayout = new QHBoxLayout();
    m_cancelBtn = new QPushButton("CANCELAR SELECIONADO", centralPanel);
    m_cancelBtn->setObjectName("cancelBtn");
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    m_cancelBtn->setMinimumHeight(40);
    m_cancelBtn->setFixedWidth(210);
    m_cancelBtn->setEnabled(false);

    m_cancelAllBtn = new QPushButton("CANCELAR TODOS", centralPanel);
    m_cancelAllBtn->setObjectName("cancelBtn");
    m_cancelAllBtn->setCursor(Qt::PointingHandCursor);
    m_cancelAllBtn->setMinimumHeight(40);
    m_cancelAllBtn->setFixedWidth(160);
    m_cancelAllBtn->setEnabled(false);

    m_notifyCheckBox = new QCheckBox("Exibir resumo quando toda a fila terminar", centralPanel);
    m_notifyCheckBox->setChecked(settings.showNotifications);
    m_notifyCheckBox->setCursor(Qt::PointingHandCursor);

    QLabel *concurrencyLabel = new QLabel("Downloads simultâneos:", centralPanel);
    concurrencyLabel->setStyleSheet("color: #dedede; font-weight: bold; font-size: 13px; margin-left: 20px; margin-right: 4px;");

    m_concurrencyCombo = new QComboBox(centralPanel);
    m_concurrencyCombo->addItem("1", 1);
    m_concurrencyCombo->addItem("2", 2);
    m_concurrencyCombo->addItem("3", 3);
    m_concurrencyCombo->addItem("4", 4);
    m_concurrencyCombo->addItem("5", 5);
    const int defaultConcurrency = settings.maxConcurrentDownloads > 0 ? settings.maxConcurrentDownloads : 2;
    const int foundIdx = m_concurrencyCombo->findData(defaultConcurrency);
    if (foundIdx >= 0) {
        m_concurrencyCombo->setCurrentIndex(foundIdx);
    }
    m_concurrencyCombo->setToolTip("Quantidade máxima de downloads ativos ao mesmo tempo");
    m_concurrencyCombo->setFixedWidth(64);
    m_concurrencyCombo->setMinimumHeight(32);
    m_concurrencyCombo->setCursor(Qt::PointingHandCursor);

    actionBottomLayout->addWidget(m_notifyCheckBox);
    actionBottomLayout->addWidget(concurrencyLabel);
    actionBottomLayout->addWidget(m_concurrencyCombo);
    actionBottomLayout->addStretch();
    actionBottomLayout->addWidget(m_cancelBtn);
    actionBottomLayout->addWidget(m_cancelAllBtn);

    m_downloadsQueueTable = new QTableWidget(0, 6, centralPanel);
    QStringList queueHeaders;
    queueHeaders << "Vídeo / Arquivo" << "Formato" << "Progresso" << "Velocidade / ETA" << "Tamanho" << "Status";
    m_downloadsQueueTable->setHorizontalHeaderLabels(queueHeaders);
    m_downloadsQueueTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_downloadsQueueTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_downloadsQueueTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_downloadsQueueTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_downloadsQueueTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_downloadsQueueTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_downloadsQueueTable->verticalHeader()->setVisible(false);
    m_downloadsQueueTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_downloadsQueueTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_downloadsQueueTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_downloadsQueueTable->setAlternatingRowColors(true);
    m_downloadsQueueTable->setObjectName("libraryTable");
    m_downloadsQueueTable->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(m_downloadsQueueTable, &QTableWidget::customContextMenuRequested, window, &MainWindow::showQueueContextMenu);
    QObject::connect(m_downloadsQueueTable, &QTableWidget::cellDoubleClicked, window, &MainWindow::onDownloadQueueDoubleClicked);
    QObject::connect(m_downloadsQueueTable, &QTableWidget::itemSelectionChanged, window, &MainWindow::onQueueSelectionChanged);

    centerLayout->addWidget(m_statusLabel);
    centerLayout->addWidget(m_progressBar);
    centerLayout->addLayout(statsLayout);
    centerLayout->addWidget(m_downloadsQueueTable, 1);
    centerLayout->addLayout(actionBottomLayout);

    downloadsLayout->addWidget(centralPanel, 1);
    m_stackedWidget->addWidget(pageDownloads);

    // ---> TELA 1: BIBLIOTECA DE MÍDIAS <---
    m_libraryView = new LibraryView(m_stackedWidget);
    QObject::connect(m_libraryView, &LibraryView::openFileRequested,
            window, &MainWindow::openLibraryFile);
    QObject::connect(m_libraryView, &LibraryView::openFolderRequested,
            window, &MainWindow::onOpenFolderClicked);
    QObject::connect(m_libraryView, &LibraryView::logMessageRequested,
            window, &MainWindow::logMessage);
    m_stackedWidget->addWidget(m_libraryView);

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
    QObject::connect(m_convertBrowseBtn, &QPushButton::clicked, window, &MainWindow::onConvertBrowseClicked);

    QHBoxLayout *convFileLayout = new QHBoxLayout();
    convFileLayout->addWidget(m_convertInput);
    convFileLayout->addWidget(m_convertBrowseBtn);

    QLabel *lblConvFormat = new QLabel("Formato de Saída:", pageConverter);
    m_convertFormatCombo = new QComboBox(pageConverter);
    m_convertFormatCombo->addItem("MP4 (H.264 / Aceleração quando disponível)");
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
    QObject::connect(m_startConvertBtn, &QPushButton::clicked, window, &MainWindow::onStartConvertClicked);

    m_cancelConvertBtn = new QPushButton("CANCELAR", pageConverter);
    m_cancelConvertBtn->setObjectName("cancelBtn");
    m_cancelConvertBtn->setCursor(Qt::PointingHandCursor);
    m_cancelConvertBtn->setMinimumHeight(44);
    m_cancelConvertBtn->setEnabled(false);
    QObject::connect(m_cancelConvertBtn, &QPushButton::clicked, window, &MainWindow::onCancelConvertClicked);

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

    auto *logsHeaderLayout = new QHBoxLayout();
    QLabel *logsTitle = new QLabel("Terminal de logs do processador e telemetria:", pageLogs);
    logsTitle->setStyleSheet("font-weight: bold; color: #10b981; font-size: 15px;");
    logsHeaderLayout->addWidget(logsTitle);
    logsHeaderLayout->addStretch();
    m_logSearchInput = new QLineEdit(pageLogs);
    m_logSearchInput->setObjectName("logSearchInput");
    m_logSearchInput->setPlaceholderText("Buscar nos logs...");
    m_logSearchInput->setClearButtonEnabled(true);
    m_logSearchInput->setMaximumWidth(220);
    logsHeaderLayout->addWidget(m_logSearchInput);
    m_logSummaryLabel = new QLabel("Total: 0 | Visíveis: 0 | Erros: 0 | Alertas: 0", pageLogs);
    m_logSummaryLabel->setObjectName("logSummaryLabel");
    m_logSummaryLabel->setToolTip("Os logs da sessão também são gravados em arquivo.");
    logsHeaderLayout->addWidget(m_logSummaryLabel);
    logsLayout->addLayout(logsHeaderLayout);

    QHBoxLayout *logFilterLayout = new QHBoxLayout();
    logFilterLayout->setSpacing(8);

    m_filterAllBtn = new QPushButton("Todos os Logs", pageLogs);
    m_filterProcessesBtn = new QPushButton("Apenas Processos", pageLogs);
    m_filterErrorsBtn = new QPushButton("Erros e Alertas", pageLogs);
    m_filterGeneralBtn = new QPushButton("Sistema & Gerais", pageLogs);
    m_copyLogsBtn = new QPushButton("Copiar visíveis", pageLogs);
    m_clearLogsBtn = new QPushButton("Limpar Terminal", pageLogs);

    m_filterAllBtn->setCursor(Qt::PointingHandCursor);
    m_filterProcessesBtn->setCursor(Qt::PointingHandCursor);
    m_filterErrorsBtn->setCursor(Qt::PointingHandCursor);
    m_filterGeneralBtn->setCursor(Qt::PointingHandCursor);
    m_copyLogsBtn->setCursor(Qt::PointingHandCursor);
    m_clearLogsBtn->setCursor(Qt::PointingHandCursor);

    logFilterLayout->addWidget(m_filterAllBtn);
    logFilterLayout->addWidget(m_filterProcessesBtn);
    logFilterLayout->addWidget(m_filterErrorsBtn);
    logFilterLayout->addWidget(m_filterGeneralBtn);
    logFilterLayout->addStretch();
    logFilterLayout->addWidget(m_copyLogsBtn);
    logFilterLayout->addWidget(m_clearLogsBtn);
    logsLayout->addLayout(logFilterLayout);

    QObject::connect(m_filterAllBtn, &QPushButton::clicked, window, [window]() { window->updateLogFilter(0); });
    QObject::connect(m_filterProcessesBtn, &QPushButton::clicked, window, [window]() { window->updateLogFilter(1); });
    QObject::connect(m_filterErrorsBtn, &QPushButton::clicked, window, [window]() { window->updateLogFilter(2); });
    QObject::connect(m_filterGeneralBtn, &QPushButton::clicked, window, [window]() { window->updateLogFilter(3); });
    QObject::connect(m_logSearchInput, &QLineEdit::textChanged, window, [window](const QString &) {
        window->refreshLogDisplay();
    });
    QObject::connect(m_copyLogsBtn, &QPushButton::clicked, window, [window]() {
        if (m_logEdit) {
            QApplication::clipboard()->setText(m_logEdit->toPlainText());
        }
    });
    QObject::connect(m_clearLogsBtn, &QPushButton::clicked, window, [window]() {
        m_allLogs.clear();
        window->refreshLogDisplay();
    });

    m_logEdit = new QPlainTextEdit(pageLogs);
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumBlockCount(kMaximumLogEntries);
    m_logEdit->setObjectName("logArea");
    new LogHighlighter(m_logEdit->document());
    logsLayout->addWidget(m_logEdit);
    m_stackedWidget->addWidget(pageLogs);

    window->updateLogFilter(0);

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
    QLabel *lblAppNameVal = new QLabel("Prism Downloader (Studio Suite Edition)", appInfoGroup);
    lblAppNameVal->setStyleSheet("color: #ffffff; font-weight: bold; font-size: 13px;");

    QLabel *lblAppVerKey = new QLabel("Versão Atual:", appInfoGroup);
    lblAppVerKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    QLabel *lblAppVerVal = new QLabel(versionNumber + " (Estável / Release)", appInfoGroup);
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
    QLabel *lblServerVal = new QLabel("GitHub Oficial (BadTonho/PrismDownloader)", updateGroup);
    lblServerVal->setStyleSheet("color: #38bdf8; font-weight: bold; font-size: 13px;");

    QLabel *lblUpStatusKey = new QLabel("Status de Versão:", updateGroup);
    lblUpStatusKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    m_updateStatusLabel = new QLabel("Versão " + versionTag + " (Release) operacional. Aguardando verificação...", updateGroup);
    m_updateStatusLabel->setStyleSheet("color: #ffffff; font-weight: bold; font-size: 13px;");

    QLabel *lblYtdlpStatusKey = new QLabel("Motor yt-dlp:", updateGroup);
    lblYtdlpStatusKey->setStyleSheet("color: #8c8c8c; font-weight: bold;");
    m_ytdlpStatusLabel = new QLabel("Localizando versão e origem do motor...", updateGroup);
    m_ytdlpStatusLabel->setWordWrap(true);
    m_ytdlpStatusLabel->setStyleSheet("color: #ffffff; font-weight: bold; font-size: 13px;");

    upGrid->addWidget(lblServerKey, 0, 0);
    upGrid->addWidget(lblServerVal, 0, 1);
    upGrid->addWidget(lblUpStatusKey, 1, 0);
    upGrid->addWidget(m_updateStatusLabel, 1, 1);
    upGrid->addWidget(lblYtdlpStatusKey, 2, 0);
    upGrid->addWidget(m_ytdlpStatusLabel, 2, 1);
    upGrid->setColumnStretch(1, 1);
    upLayout->addLayout(upGrid);

    m_checkUpdatesOnStartChk = new QCheckBox("Verificar novas atualizações automaticamente ao iniciar o aplicativo", updateGroup);
    m_checkUpdatesOnStartChk->setCursor(Qt::PointingHandCursor);

    m_autoDownloadUpdatesChk = new QCheckBox("Baixar, validar por assinatura e instalar automaticamente atualizações do aplicativo", updateGroup);
    m_autoDownloadUpdatesChk->setCursor(Qt::PointingHandCursor);
    m_autoDownloadUpdatesChk->setStyleSheet("color: #f59e0b; font-weight: bold;");

    QVBoxLayout *chkVertLayout = new QVBoxLayout();
    chkVertLayout->setSpacing(6);
    chkVertLayout->addWidget(m_checkUpdatesOnStartChk);
    chkVertLayout->addWidget(m_autoDownloadUpdatesChk);
    upLayout->addLayout(chkVertLayout);

    QHBoxLayout *upBtnsLayout = new QHBoxLayout();
    m_updateAppBtn = new QPushButton("BAIXAR E ATUALIZAR AGORA", updateGroup);
    m_updateAppBtn->setObjectName("startBtn");
    m_updateAppBtn->setCursor(Qt::PointingHandCursor);
    m_updateAppBtn->setMinimumHeight(40);
    m_updateAppBtn->setVisible(false);
    QObject::connect(m_updateAppBtn, &QPushButton::clicked, window, &MainWindow::requestAppUpdate);

    m_checkUpdateBtn = new QPushButton("VERIFICAR NO GITHUB AGORA", updateGroup);
    m_checkUpdateBtn->setObjectName("startBtn");
    m_checkUpdateBtn->setCursor(Qt::PointingHandCursor);
    m_checkUpdateBtn->setMinimumHeight(40);
    QObject::connect(m_checkUpdateBtn, &QPushButton::clicked, window, [window]() {
        window->checkForUpdates(false);
        window->checkYtDlpUpdates(false);
    });

    m_updateYtdlpBtn = new QPushButton("VERIFICAR YT-DLP NIGHTLY", updateGroup);
    m_updateYtdlpBtn->setObjectName("browseBtn");
    m_updateYtdlpBtn->setCursor(Qt::PointingHandCursor);
    m_updateYtdlpBtn->setMinimumHeight(40);
    QObject::connect(m_updateYtdlpBtn, &QPushButton::clicked, window, &MainWindow::updateYtdlpEngine);

    upBtnsLayout->addWidget(m_updateAppBtn, 3);
    upBtnsLayout->addWidget(m_checkUpdateBtn, 2);
    upBtnsLayout->addWidget(m_updateYtdlpBtn, 2);
    upLayout->addLayout(upBtnsLayout);

    m_updateProgressBar = new QProgressBar(updateGroup);
    m_updateProgressBar->setRange(0, 100);
    m_updateProgressBar->setValue(0);
    m_updateProgressBar->setTextVisible(true);
    m_updateProgressBar->setStyleSheet("QProgressBar { background-color: #171717; border: 1px solid #333333; border-radius: 6px; color: #ffffff; font-weight: bold; height: 24px; text-align: center; } QProgressBar::chunk { background-color: #10b981; border-radius: 5px; }");
    m_updateProgressBar->setVisible(false);
    upLayout->addWidget(m_updateProgressBar);

    upPageLayout->addWidget(updateGroup);
    upPageLayout->addStretch();
    m_stackedWidget->addWidget(pageUpdates);

    // Conectar navegação e botões principais
    QObject::connect(navGroup, &QButtonGroup::idClicked, window, &MainWindow::switchPage);
    QObject::connect(m_startBtn, &QPushButton::clicked, window, &MainWindow::onStartClicked);
    QObject::connect(m_cancelBtn, &QPushButton::clicked, window, &MainWindow::onCancelClicked);
    QObject::connect(m_cancelAllBtn, &QPushButton::clicked, window, &MainWindow::onCancelAllClicked);
    QObject::connect(m_concurrencyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), window, &MainWindow::onConcurrencyChanged);
    QObject::connect(m_browseDirBtn, &QPushButton::clicked, window, &MainWindow::onBrowseClicked);
    QObject::connect(m_openFolderBtn, &QPushButton::clicked, window, &MainWindow::onOpenFolderClicked);

#undef m_allLogs
#undef m_autoDownloadUpdatesChk
#undef m_browseDirBtn
#undef m_cancelAllBtn
#undef m_cancelBtn
#undef m_cancelConvertBtn
#undef m_checkUpdateBtn
#undef m_checkUpdatesOnStartChk
#undef m_clearLogsBtn
#undef m_concurrencyCombo
#undef m_convertBrowseBtn
#undef m_convertEngineLabel
#undef m_convertFormatCombo
#undef m_convertInput
#undef m_convertProgressBar
#undef m_convertStatusLabel
#undef m_copyLogsBtn
#undef m_downloadsQueueTable
#undef m_etaLabel
#undef m_filterAllBtn
#undef m_filterErrorsBtn
#undef m_filterGeneralBtn
#undef m_filterProcessesBtn
#undef m_gpuCodecLabel
#undef m_gpuModelLabel
#undef m_gpuStatusLabel
#undef m_libraryView
#undef m_logEdit
#undef m_logSearchInput
#undef m_logSummaryLabel
#undef m_navConverterBtn
#undef m_navDownloadBtn
#undef m_navInfoBtn
#undef m_navLibraryBtn
#undef m_navLogsBtn
#undef m_notifyCheckBox
#undef m_openFolderBtn
#undef m_outputDirInput
#undef m_progressBar
#undef m_qualityCombo
#undef m_sidebarUpdateBtn
#undef m_sidebarUpdateNotification
#undef m_speedLabel
#undef m_stackedWidget
#undef m_startBtn
#undef m_startConvertBtn
#undef m_statusLabel
#undef m_updateAppBtn
#undef m_updateProgressBar
#undef m_updateStatusLabel
#undef m_updateYtdlpBtn
#undef m_urlInput
#undef m_ytdlpStatusLabel
}
