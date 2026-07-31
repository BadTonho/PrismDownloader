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

private:
    void setupUI();
    void setupStyles();
    void logMessage(const QString &msg);

    // Estrutura de Navegação Lateral (Sidebar + StackedWidget)
    QStackedWidget *m_stackedWidget;
    QPushButton *m_navDownloadBtn;
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

    // Tela de Terminal de Logs
    QTextEdit *m_logEdit;

    // Componentes da Tela de Informações
    QLabel *m_gpuModelLabel;
    QLabel *m_gpuCodecLabel;
    QLabel *m_gpuStatusLabel;

    // Motor Central nativo C++
    DownloadEngine m_engine;
};

#endif // MAINWINDOW_H
