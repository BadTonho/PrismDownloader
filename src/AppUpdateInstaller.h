#ifndef PRISM_APP_UPDATE_INSTALLER_H
#define PRISM_APP_UPDATE_INSTALLER_H

#include <QObject>
#include <QString>

class QProcess;

class AppUpdateInstaller final : public QObject {
    Q_OBJECT

public:
    explicit AppUpdateInstaller(QObject *parent = nullptr);

    bool isInstalling() const;
    void install(const QString &version,
                 const QString &packagePath,
                 bool installedWindowsCopy);

signals:
    void installingChanged(bool installing);
    void logMessage(const QString &message);
    void statusMessage(const QString &message);
    void failed(const QString &message);
    void restartRequested();

private:
    void setInstalling(bool installing);
    void fail(const QString &message, const QString &packagePath);

    QProcess *m_process{nullptr};
    QString m_version;
    QString m_packagePath;
    bool m_installing{false};
};

#endif // PRISM_APP_UPDATE_INSTALLER_H
