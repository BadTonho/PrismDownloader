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

private:
    void setupUI();
    void setupStyles();
    void logMessage(const QString &msg);

    // Componentes de Entrada e Configuração
    QLineEdit *m_urlInput;
    QComboBox *m_qualityCombo;
    QLineEdit *m_timeRangeInput;
    QLineEdit *m_outputDirInput;
    QPushButton *m_browseDirBtn;
    QCheckBox *m_notifyCheckBox;
    
    // Botões de Ação
    QPushButton *m_startBtn;
    QPushButton *m_cancelBtn;
    QPushButton *m_openFolderBtn;

    // Indicadores e Monitoramento ao vivo
    QLabel *m_gpuBanner;
    QProgressBar *m_progressBar;
    QLabel *m_speedLabel;
    QLabel *m_etaLabel;
    QLabel *m_statusLabel;
    QTextEdit *m_logArea;

    // Motor Central nativo C++
    DownloadEngine m_engine;
};

#endif // MAINWINDOW_H
