#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QThread>
#include <QUrl>

namespace {

bool createFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write("fake media data");
    return true;
}

}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    const QStringList arguments = application.arguments().mid(1);
    QTextStream output(stdout);

    if (arguments.contains("--print")) {
        const int destinationIndex = arguments.indexOf("-P");
        if (destinationIndex < 0 || destinationIndex + 1 >= arguments.size()) {
            return 2;
        }
        QString identifier = QUrl(arguments.constLast()).path().section('/', -1);
        if (identifier.isEmpty()) identifier = "media";
        const QString result = QDir(arguments.at(destinationIndex + 1))
                                   .absoluteFilePath("Fake [" + identifier + "].mp4");
        output << "[download] 10.0% of 1.00MiB at 1.00MiB/s ETA 00:01" << Qt::endl;
        QThread::msleep(300);
        if (!createFile(result)) return 3;
        output << "[download] 100.0% of 1.00MiB at 1.00MiB/s ETA 00:00" << Qt::endl;
        output << "__PRISM_OUTPUT__" << QDir::toNativeSeparators(result) << Qt::endl;
        return 0;
    }

    if (arguments.isEmpty()) return 4;
    QThread::msleep(220);
    return createFile(arguments.constLast()) ? 0 : 5;
}
