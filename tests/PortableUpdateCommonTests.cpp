#include "PortableUpdateCommon.h"

#include <QCoreApplication>

#include <iostream>

namespace {

bool check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    bool success = check(PortableUpdateCommon::isSafeArchiveEntry(QStringLiteral("platforms/qwindows.dll")),
                         "normal archive entry is accepted")
        && check(!PortableUpdateCommon::isSafeArchiveEntry(QStringLiteral("../outside.exe")),
                 "parent traversal is rejected")
        && check(!PortableUpdateCommon::isSafeArchiveEntry(QStringLiteral("C:/outside.exe")),
                 "drive-qualified entry is rejected")
        && check(!PortableUpdateCommon::isSafeArchiveEntry(QStringLiteral("/outside.exe")),
                 "rooted entry is rejected")
        && check(!PortableUpdateCommon::isSafeArchiveEntry(QStringLiteral("folder/..hidden/file.exe")),
                 "ambiguous traversal entry is rejected")
        ;
    return success ? 0 : 1;
}
