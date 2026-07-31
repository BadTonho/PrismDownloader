#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QComboBox>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QCheckBox>
#include <QStackedWidget>
#include <QButtonGroup>
#include <QFrame>
#include <QTableWidget>
#include <QHeaderView>
#include <QFileInfoList>
#include <QProcess>
#include <QDialog>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include "DownloadEngine.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onStartClicked();
    void onCancelClicked();
    void onBrowseClicked();
    void onOpenFolderClicked();
    void switchPage(int index);
    
    // Slots da Biblioteca de Mídia
    void refreshLibrary();
    void onPlaySelectedMedia();
    void onLibraryDoubleClicked(int row, int column);

    // Slots do Conversor de Mídia
    void onConvertBrowseClicked();
    void onStartConvertClicked();
    void onCancelConvertClicked();
    void onConvertProcessOutput();
    void onConvertProcessFinished(int exitCode);

    // Slots do Sistema de Atualização via GitHub e yt-dlp
    void checkForUpdates(bool silent = false);
    void onUpdateReplyFinished(QNetworkReply *reply, bool silent);
    void updateYtdlpEngine();
    void onUpdateDownloadFinished(QNetworkReply *reply, const QString &fileName);

private:
    void setupUI();
    void setupStyles();
    void logMessage(const QString &msg);
    void updateLogFilter(int mode);
    void refreshLogDisplay();
    bool shouldShowLogLine(const QString &line) const;
    bool showFormatSelectionDialog(QString &outQuality, QString &outTimeRange, bool &outDoConvert, QString &outConvertFormat, QString &outCustomOutputDir);
    void startUpdateDownload(const QString &url, const QString &fileName);

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

    // Indicadores e Monitoramento ao vivo
    QProgressBar *m_progressBar;
    QLabel *m_speedLabel;
    QLabel *m_etaLabel;
    QLabel *m_statusLabel;

    // Estado de Conversão Automática e Pasta Temporária pós-download
    bool m_autoConvertAfterDownload;
    QString m_autoConvertFormat;
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
    QProcess *m_convertProcess;

    // Tela de Terminal de Logs (Página 3)
    QTextEdit *m_logEdit{nullptr};
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
    QPushButton *m_checkUpdateBtn;
    QPushButton *m_updateYtdlpBtn;

    // Motor Central nativo C++
    DownloadEngine m_engine;
};

#endif // MAINWINDOW_H
