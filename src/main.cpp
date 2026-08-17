#include <QApplication>
#include <QIcon>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Tonho Studios"));
    app.setApplicationName(QStringLiteral("PrismDownloader"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/prism-downloader.png")));

    MainWindow window;
    window.show();

    return app.exec();
}
