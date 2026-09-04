#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
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

#ifdef Q_OS_LINUX
    if (arguments.contains("--windows-filenames")) {
        return 6;
    }
#endif

    if (arguments.contains("--print")) {
        const int destinationIndex = arguments.indexOf("-P");
        if (destinationIndex < 0 || destinationIndex + 1 >= arguments.size()) {
            return 2;
        }
    QString identifier = QUrl(arguments.constLast()).path().section('/', -1);
    if (identifier.isEmpty()) identifier = "media";
    if (identifier == "format-selector") {
        const int formatIndex = arguments.indexOf("-f");
        if (formatIndex < 0 || formatIndex + 1 >= arguments.size()
            || arguments.at(formatIndex + 1) != "video137+audio140") {
            return 7;
        }
    }
    const QString result = QDir(arguments.at(destinationIndex + 1))
                               .absoluteFilePath("Fake [" + identifier + "].mp4");
        const bool relativePath = identifier == "relative";
        const bool jsonPath = identifier == "json";
        const bool stalePath = identifier == "stale";
        output << "[download] 10.0% of 1.00MiB at 1.00MiB/s ETA 00:01" << Qt::endl;
        QThread::msleep(300);
        if (!createFile(result)) return 3;
        output << "[download] 100.0% of 1.00MiB at 1.00MiB/s ETA 00:00" << Qt::endl;
        if (stalePath) {
            output << "[Merger] Merging formats into: "
                   << QDir::toNativeSeparators(result) << Qt::endl;
            output << "__PRISM_OUTPUT__"
                   << QDir::toNativeSeparators(QDir(arguments.at(destinationIndex + 1))
                                                   .absoluteFilePath("not-the-file.mp4"))
                   << Qt::endl;
        } else if (relativePath) {
            output << "__PRISM_OUTPUT__" << QFileInfo(result).fileName() << Qt::endl;
        } else if (jsonPath) {
            const QByteArray encoded = QJsonDocument(QJsonArray{result})
                                           .toJson(QJsonDocument::Compact);
            output << "__PRISM_OUTPUT__"
                   << QString::fromUtf8(encoded.mid(1, encoded.size() - 2)) << Qt::endl;
        } else {
            output << "__PRISM_OUTPUT__" << QDir::toNativeSeparators(result) << Qt::endl;
        }
        return 0;
    }

    if (arguments.isEmpty()) return 4;
    if (arguments.contains("-progress")) {
        output << "Duration: 00:00:02.00" << Qt::endl;
        output << "out_time_ms=1000000" << Qt::endl;
        output << "progress=continue" << Qt::endl;
    }
    QThread::msleep(220);
    return createFile(arguments.constLast()) ? 0 : 5;
}
