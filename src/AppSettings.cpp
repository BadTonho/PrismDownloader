#include "AppSettings.h"

#include <QSettings>

AppSettings AppSettings::load()
{
    QSettings settings("Tonho Studios", "PrismDownloader");
    AppSettings result;
    result.outputFolder = settings.value("outputFolder").toString();
    result.selectedQualityIndex = settings.value("selectedQuality", 1).toInt();
    result.defaultTimeRange = settings.value("defaultTimeRange").toString();
    result.showNotifications = settings.value("showNotifications", false).toBool();
    result.checkUpdatesOnStart = settings.value("checkUpdatesOnStart", true).toBool();
    result.autoDownloadUpdates = settings.value("autoDownloadUpdates", false).toBool();
    result.maxConcurrentDownloads = qBound(
        1, settings.value("maxConcurrentDownloads", 2).toInt(), 5);
    return result;
}

void AppSettings::save() const
{
    QSettings settings("Tonho Studios", "PrismDownloader");
    settings.setValue("outputFolder", outputFolder);
    settings.setValue("showNotifications", showNotifications);
    settings.setValue("selectedQuality", selectedQualityIndex);
    settings.setValue("defaultTimeRange", defaultTimeRange);
    settings.setValue("checkUpdatesOnStart", checkUpdatesOnStart);
    settings.setValue("maxConcurrentDownloads", maxConcurrentDownloads);
    settings.setValue("autoDownloadUpdates", autoDownloadUpdates);
    settings.sync();
}
