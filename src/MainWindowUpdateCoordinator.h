#ifndef MAINWINDOW_UPDATE_COORDINATOR_H
#define MAINWINDOW_UPDATE_COORDINATOR_H

#include <QString>

class MainWindow;
class AppUpdateInstaller;
class AppUpdateService;
class YtDlpUpdateService;

class MainWindowUpdateCoordinator final {
public:
    MainWindowUpdateCoordinator(MainWindow *window,
                                QString versionTag,
                                QString versionNumber);

    void checkForUpdates(bool silent);
    void requestAppUpdate();
    void checkYtDlpUpdates(bool silent);
    void updateYtdlpEngine();
    void tryStartPendingAppUpdate();
    bool isInstallingAppUpdate() const;

private:
    bool canInstallAppUpdate() const;
    bool isInstalledWindowsCopy() const;
    void installVerifiedAppPackage(const QString &version, const QString &packagePath);

    MainWindow *m_window;
    QString m_versionTag;
    QString m_versionNumber;
    AppUpdateInstaller *m_appUpdateInstaller{nullptr};
    AppUpdateService *m_appUpdateService{nullptr};
    YtDlpUpdateService *m_ytdlpUpdateService{nullptr};
    bool m_ytdlpCheckSilent{true};
    bool m_appUpdateCheckSilent{true};
    bool m_appUpdatePending{false};
};

#endif // MAINWINDOW_UPDATE_COORDINATOR_H
