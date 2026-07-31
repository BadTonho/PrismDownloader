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
    void refreshLibrary();
    void onPlaySelectedMedia();
    void onLibraryDoubleClicked(int row, int column);

private:
    void setupUI();
    void setupStyles();
    void logMessage(const QString &msg);

    // Estrutura de Navegação Lateral (Sidebar + StackedWidget)
    QStackedWidget *m_stackedWidget;
    QPushButton *m_navDownloadBtn;
    QPushButton *m_navLibraryBtn;
    QPushButton *m_navLogsBtn;
    QPushButton *m_navInfoBtn;
    QPushButton *m_openFolderBtn;

    // Componentes da Tela de Downloads
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

    // Tela de Biblioteca de Mídias (Página 1 do StackedWidget)
    QTableWidget *m_libraryTable;

    // Tela de Terminal de Logs (Página 2 do StackedWidget)
    QTextEdit *m_logEdit;

    // Componentes da Tela de Informações (Página 3 do StackedWidget)
    QLabel *m_gpuModelLabel;
    QLabel *m_gpuCodecLabel;
    QLabel *m_gpuStatusLabel;

    // Motor Central nativo C++
    DownloadEngine m_engine;
};

#endif // MAINWINDOW_H
