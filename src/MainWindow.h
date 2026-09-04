#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QByteArray>
#include <QLineEdit>
#include <QComboBox>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QFile>
#include <QCheckBox>
#include <QStackedWidget>
#include <QButtonGroup>
#include <QFrame>
#include <QTableWidget>
#include <QHeaderView>
#include <QListWidget>
#include <QPixmap>
#include <QFileInfoList>
#include <QDialog>
#include <QTimer>
#include <QHash>
#include <QSet>
#include <QList>
#include <QStringList>
#include <QSpinBox>
#include <memory>
#include "DownloadManager.h"
#include "ConversionManager.h"
#include "AppSettings.h"
#include "GPUDetector.h"
#include "MediaMetadata.h"
#include "PlaylistItem.h"
#include "PlaylistPreviewService.h"

class QCloseEvent;
class QProcess;
class QThread;
class QProgressDialog;
class QNetworkAccessManager;
class LibraryView;
class MainWindowUiBuilder;
class MainWindowUpdateCoordinator;
class DownloadQueueWorkflow;
class YtDlpMetadataService;

class MainWindow : public QMainWindow {
    Q_OBJECT

    friend class MainWindowUiBuilder;
    friend class MainWindowUpdateCoordinator;


public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void onStartClicked();
    void onCancelClicked();
    void onCancelAllClicked();
    void onQueueSelectionChanged();
    void onConcurrencyChanged(int index);
    void onBrowseClicked();
    void onOpenFolderClicked();
    void switchPage(int index);
    void showQueueContextMenu(const QPoint &pos);
    
    // Slots da Biblioteca de Mídia
    void refreshLibrary();
    void onDownloadQueueDoubleClicked(int row, int column);
    void onConvertBrowseClicked();
    void onStartConvertClicked();
    void onCancelConvertClicked();

    void onDownloadProgress(DownloadId id, double percent, const QString &speed, const QString &eta);
    void onDownloadStatus(DownloadId id, DownloadStatus status, const QString &message);
    void onDownloadCompleted(DownloadId id, const QString &filePath);
    void onDownloadQueueStateChanged(int active, int pending);
    void onConversionStatus(ConversionId id, DownloadId ownerDownloadId, const QString &message);
    void onConversionProgress(ConversionId id, DownloadId ownerDownloadId, double percent);
    void onConversionCompleted(ConversionId id, DownloadId ownerDownloadId, const QString &outputFile);
    void onConversionFailed(ConversionId id, DownloadId ownerDownloadId, const QString &message);
    void onConversionCancelled(ConversionId id, DownloadId ownerDownloadId);

    // Slots do Sistema de Atualização via GitHub e yt-dlp
    void checkForUpdates(bool silent = false);
    void requestAppUpdate();
    void checkYtDlpUpdates(bool silent = false);
    void updateYtdlpEngine();

private:
    void setupUI();
    void setupStyles();
    void logMessage(const QString &msg);
    void updateLogFilter(int mode);
    void refreshLogDisplay();
    void initializeLogFile();
    void updateLogSummary();
    bool shouldShowLogLine(const QString &line) const;
    bool showFormatSelectionDialog(const MediaMetadata &metadata, int itemCount,
                                   QString &outQuality, QString &outTimeRange,
                                   QString &outFormatSelector, bool &outDoConvert,
                                   QString &outConvertFormat,
                                   QString &outCustomOutputDir);
    int findDownloadRow(DownloadId id) const;
    DownloadId selectedDownloadId() const;
    void updateJobRow(DownloadId id);
    void updateSelectedMonitor();
    void maybeShowQueueSummary();
    void showDownloadsPage();
    void startPlaylistPreview(const QUrl &url);
    void closePlaylistPreviewDialog();
    void handlePlaylistPreviewReady(const QList<PlaylistItem> &items,
                                    int exitCode,
                                    bool truncated,
                                    const QString &errorOutput);
    void continueDownload(const QList<PlaylistItem> &items);
    void startMetadataLookup(const QList<PlaylistItem> &items);
    void continueDownloadWithMetadata(const QList<PlaylistItem> &items,
                                      const MediaMetadata &metadata);
    bool showPlaylistSelectionDialog(const QList<PlaylistItem> &items,
                                     QList<PlaylistItem> &selectedItems);
    void showPlaylistItemDetailsDialog(const PlaylistItem &item);
    void openLibraryFile(const QString &filePath);

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

    // Componentes da Tela de Downloads (popup principal)
    QLineEdit *m_urlInput;
    QLineEdit *m_outputDirInput;
    QPushButton *m_browseDirBtn;
    QCheckBox *m_notifyCheckBox;
    
    // Botões de Ação na Tela de Downloads
    QPushButton *m_startBtn;
    QPushButton *m_cancelBtn;
    QPushButton *m_cancelAllBtn;
    QComboBox *m_concurrencyCombo;

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
        qint64 lastUiRefreshMs{0};
    };

    QHash<DownloadId, UiDownloadJob> m_downloadJobs;
    QHash<DownloadId, QTableWidgetItem *> m_downloadRowItems;
    QSet<DownloadId> m_currentBatchJobs;
    QString m_currentDownloadDir;

    // Tela de Biblioteca de Mídias (Página 1)
    LibraryView *m_libraryView{nullptr};

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
    QLineEdit *m_logSearchInput{nullptr};
    QLabel *m_logSummaryLabel{nullptr};
    QStringList m_allLogs;
    QStringList m_pendingLogLines;
    QTimer *m_logFlushTimer{nullptr};
    int m_logFilterMode{0}; // 0=Todos, 1=Processos, 2=Erros, 3=Geral
    QPushButton *m_filterAllBtn{nullptr};
    QPushButton *m_filterProcessesBtn{nullptr};
    QPushButton *m_filterErrorsBtn{nullptr};
    QPushButton *m_filterGeneralBtn{nullptr};
    QPushButton *m_copyLogsBtn{nullptr};
    QPushButton *m_clearLogsBtn{nullptr};
    QFile m_logFile;
    QString m_logFilePath;

    // Componentes da Tela de Informações e Hardware (Página 4)
    QLabel *m_gpuModelLabel;
    QLabel *m_gpuCodecLabel;
    QLabel *m_gpuStatusLabel;

    // Módulo de Rede e Central de Atualizações (GitHub Core)
    QCheckBox *m_checkUpdatesOnStartChk;
    QCheckBox *m_autoDownloadUpdatesChk;
    QLabel *m_updateStatusLabel;
    QLabel *m_ytdlpStatusLabel{nullptr};
    QProgressBar *m_updateProgressBar{nullptr};
    QPushButton *m_checkUpdateBtn;
    QPushButton *m_updateAppBtn{nullptr};
    QPushButton *m_updateYtdlpBtn;
    std::unique_ptr<MainWindowUpdateCoordinator> m_updateCoordinator;

    void flushLogBuffer();
    void startGpuProbe();
    void applyGpuProbeResult();

    AppSettings m_settings;
    DownloadManager *m_downloadManager;
    ConversionManager *m_conversionManager;
    std::unique_ptr<DownloadQueueWorkflow> m_downloadQueueWorkflow;
    PlaylistPreviewService *m_playlistPreviewService{nullptr};
    YtDlpMetadataService *m_metadataService{nullptr};
    QNetworkAccessManager *m_thumbnailNetwork{nullptr};
    QThread *m_gpuProbeThread{nullptr};
    GPUDetector *m_gpuProbeResult{nullptr};
    GPUDetector m_gpuDetector;
    bool m_closing{false};
};

#endif // MAINWINDOW_H
