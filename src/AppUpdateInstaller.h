#ifndef PRISM_APP_UPDATE_INSTALLER_H
#define PRISM_APP_UPDATE_INSTALLER_H

#include "AppUpdateService.h"

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
                 const QString &expectedSha256,
                 AppUpdatePackageKind packageKind);

    // Remove pacotes e helpers de atualizações anteriores que ficaram em %TEMP%.
    static void cleanupStaleFiles();

signals:
    void installingChanged(bool installing);
    void logMessage(const QString &message);
    void statusMessage(const QString &message);
    void failed(const QString &message);
    void restartRequested();

private:
    static bool fileMatchesSha256(const QString &path, const QString &expectedSha256);
    void installWithPackageManager(const QString &packagePath, const QString &expectedSha256,
                                   AppUpdatePackageKind packageKind);
    void installAppImage(const QString &packagePath, const QString &expectedSha256);
    void installTarball(const QString &packagePath, const QString &expectedSha256);
    void relaunchAfterInstall(const QString &executable);
    void setInstalling(bool installing);
    void fail(const QString &message, const QString &packagePath);

    QProcess *m_process{nullptr};
    QString m_version;
    QString m_packagePath;
    bool m_installing{false};
};

#endif // PRISM_APP_UPDATE_INSTALLER_H
