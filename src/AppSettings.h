#ifndef PRISM_APP_SETTINGS_H
#define PRISM_APP_SETTINGS_H

#include <QString>

struct AppSettings {
    QString outputFolder;
    int selectedQualityIndex{1};
    QString defaultTimeRange;
    bool showNotifications{false};
    bool checkUpdatesOnStart{true};
    bool autoDownloadUpdates{false};
    int maxConcurrentDownloads{2};

    static AppSettings load();
    void save() const;
};

#endif // PRISM_APP_SETTINGS_H
