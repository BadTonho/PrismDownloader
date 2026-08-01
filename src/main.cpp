#include <QApplication>
#include <QIcon>
#include <QFile>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    if (QFile::exists("app_icon.png")) {
        app.setWindowIcon(QIcon("app_icon.png"));
    } else if (QFile::exists("app_icon.ico")) {
        app.setWindowIcon(QIcon("app_icon.ico"));
    }

    MainWindow window;
    window.show();

    return app.exec();
}
