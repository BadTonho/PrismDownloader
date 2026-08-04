#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QComboBox>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QCheckBox>
#include <QStackedWidget>
#include <QButtonGroup>
#include <QFrame>
#include <QTableWidget>
#include <QHeaderView>
#include <QFileInfoList>
#include <QDialog>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QHash>
#include <QSet>
#include <QSpinBox>
#include "DownloadManager.h"
#include "ConversionManager.h"
#include "GPUDetector.h"

class QCloseEvent;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onStartClicked();
    void onCancelClicked();
    void onCancelAllClicked();
    void onQueueSelectionChanged();
    void onConcurrencyChanged(int value);
    void onBrowseClicked();
    void onOpenFolderClicked();
    void switchPage(int index);
    
    // Slots da Biblioteca de Mídia
    void refreshLibrary();
    void onPlaySelectedMedia();
    void onLibraryDoubleClicked(int row, int column);
    void onDownloadQueueDoubleClicked(int row, int column);

    // Slots do Conversor de Mídia
    void onConvertBrowseClicked();
    void onStartConvertClicked();
    void onCancelConvertClicked();

    void onDownloadProgress(DownloadId id, double percent, const QString &speed, const QString &eta);
    void onDownloadStatus(DownloadId id, DownloadStatus status, const QString &message);
    void onDownloadCompleted(DownloadId id, const QString &filePath);
    void onDownloadQueueStateChanged(int active, int pending);
    void onConversionStatus(ConversionId id, DownloadId ownerDownloadId, const QString &message);
    void onConversionCompleted(ConversionId id, DownloadId ownerDownloadId, const QString &outputFile);
    void onConversionFailed(ConversionId id, DownloadId ownerDownloadId, const QString &message);
    void onConversionCancelled(ConversionId id, DownloadId ownerDownloadId);

    // Slots do Sistema de Atualização via GitHub e yt-dlp
    void checkForUpdates(bool silent = false);
    void onUpdateReplyFinished(QNetworkReply *reply, bool silent);
    void updateYtdlpEngine();

private:
    void setupUI();
    void setupStyles();
    void logMessage(const QString &msg);
    void updateLogFilter(int mode);
    void refreshLogDisplay();
    bool shouldShowLogLine(const QString &line) const;
    bool showFormatSelectionDialog(QString &outQuality, QString &outTimeRange, bool &outDoConvert, QString &outConvertFormat, QString &outCustomOutputDir);
    int findDownloadRow(DownloadId id) const;
    DownloadId selectedDownloadId() const;
    void updateJobRow(DownloadId id);
    void updateSelectedMonitor();
    void maybeShowQueueSummary();

    // Estrutura de Navegação Lateral (Sidebar + StackedWidget)
    QStackedWidget *m_stackedWidget;
    QPushButton *m_navDownloadBtn;
    QPushButton *m_navLibraryBtn;
    QPushButton *m_navConverterBtn;
    QPushButton *m_navLogsBtn;
    QPushButton *m_navInfoBtn;
    QPushButton *m_openFolderBtn;
    QPushButton *m_sidebarUpdateBtn;
    QLabel *m_sidebarUpdateNotification;

    // Componentes da Tela de Downloads (Página 0 - Estilo aTube Catcher)
    QLineEdit *m_urlInput;
    QComboBox *m_qualityCombo;
    QLineEdit *m_timeRangeInput;
    QLineEdit *m_outputDirInput;
    QPushButton *m_browseDirBtn;
    QCheckBox *m_notifyCheckBox;
    
    // Botões de Ação na Tela de Downloads
    QPushButton *m_startBtn;
    QPushButton *m_cancelBtn;
    QPushButton *m_cancelAllBtn;
    QSpinBox *m_concurrencySpin;

    // Indicadores e Monitoramento ao vivo
    QProgressBar *m_progressBar;
    QLabel *m_speedLabel;
    QLabel *m_etaLabel;
    QLabel *m_statusLabel;
    QTableWidget *m_downloadsQueueTable{nullptr};

    struct UiDownloadJob {
        DownloadRequest request;
        bool autoConvert{false};
        QString conversionFormat;
        QString filePath;
        QString speed;
        QString eta;
        QString statusText{"Aguardando"};
        double progress{0.0};
        DownloadStatus status{DownloadStatus::Queued};
        ConversionId conversionId{0};
        bool terminal{false};
    };

    QHash<DownloadId, UiDownloadJob> m_downloadJobs;
    QSet<DownloadId> m_currentBatchJobs;
    QString m_currentDownloadDir;

    // Tela de Biblioteca de Mídias (Página 1)
    QTableWidget *m_libraryTable;

    // Tela de Conversão de Vídeo (Página 2)
    QLineEdit *m_convertInput;
    QComboBox *m_convertFormatCombo;
    QPushButton *m_convertBrowseBtn;
    QPushButton *m_startConvertBtn;
    QPushButton *m_cancelConvertBtn;
    QProgressBar *m_convertProgressBar;
    QLabel *m_convertStatusLabel;
    QLabel *m_convertEngineLabel;
    ConversionId m_manualConversionId{0};

    // Tela de Terminal de Logs (Página 3)
    QPlainTextEdit *m_logEdit{nullptr};
    QStringList m_allLogs;
    int m_logFilterMode{0}; // 0=Todos, 1=Processos, 2=Erros, 3=Geral
    QPushButton *m_filterAllBtn{nullptr};
    QPushButton *m_filterProcessesBtn{nullptr};
    QPushButton *m_filterErrorsBtn{nullptr};
    QPushButton *m_filterGeneralBtn{nullptr};
    QPushButton *m_clearLogsBtn{nullptr};

    // Componentes da Tela de Informações e Hardware (Página 4)
    QLabel *m_gpuModelLabel;
    QLabel *m_gpuCodecLabel;
    QLabel *m_gpuStatusLabel;

    // Módulo de Rede e Central de Atualizações (GitHub Core)
    QNetworkAccessManager *m_networkManager;
    QCheckBox *m_checkUpdatesOnStartChk;
    QCheckBox *m_autoDownloadUpdatesChk;
    QLabel *m_updateStatusLabel;
    QProgressBar *m_updateProgressBar{nullptr};
    QPushButton *m_checkUpdateBtn;
    QPushButton *m_updateYtdlpBtn;

    DownloadManager *m_downloadManager;
    ConversionManager *m_conversionManager;
    GPUDetector m_gpuDetector;
    bool m_closing{false};
};

#endif // MAINWINDOW_H
