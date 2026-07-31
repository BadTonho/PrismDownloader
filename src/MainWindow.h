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
#include <QDialog>
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
    void onInfoClicked();
    void onLogsClicked();

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
    
    // Botões de Ação Principal e Ferramentas
    QPushButton *m_startBtn;
    QPushButton *m_cancelBtn;
    QPushButton *m_openFolderBtn;
    QPushButton *m_infoBtn;
    QPushButton *m_logsBtn;

    // Indicadores e Monitoramento ao vivo
    QProgressBar *m_progressBar;
    QLabel *m_speedLabel;
    QLabel *m_etaLabel;
    QLabel *m_statusLabel;

    // Janela flutuante dedicada para o Terminal de Logs
    QDialog *m_logDialog;
    QTextEdit *m_logEdit;

    // Dados de Hardware memorizados
    QString m_hardwareInfoText;

    // Motor Central nativo C++
    DownloadEngine m_engine;
};

#endif // MAINWINDOW_H
