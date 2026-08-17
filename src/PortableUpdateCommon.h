#ifndef PORTABLEUPDATECOMMON_H
#define PORTABLEUPDATECOMMON_H

#include <QString>

namespace PortableUpdateCommon {

bool isSafeArchiveEntry(const QString &entryName);
QString extractionScript(const QString &archivePath, const QString &stagingPath);

}

#endif // PORTABLEUPDATECOMMON_H
