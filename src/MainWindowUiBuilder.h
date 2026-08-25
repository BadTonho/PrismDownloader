#ifndef MAINWINDOW_UI_BUILDER_H
#define MAINWINDOW_UI_BUILDER_H

#include <QString>

#include "AppSettings.h"

class MainWindow;

class MainWindowUiBuilder final {
public:
    static void build(MainWindow *window,
                      const QString &versionTag,
                      const QString &versionNumber,
                      const AppSettings &settings);
};

#endif // MAINWINDOW_UI_BUILDER_H
