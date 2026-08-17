#include "PortableUpdateCommon.h"

#include <QByteArray>

namespace {

QString encoded(const QString &value)
{
    return QString::fromLatin1(value.toUtf8().toBase64());
}

}

bool PortableUpdateCommon::isSafeArchiveEntry(const QString &entryName)
{
    const QString normalized = entryName;
    return !normalized.isEmpty()
        && !normalized.startsWith(QLatin1Char('/'))
        && !normalized.startsWith(QLatin1Char('\\'))
        && !normalized.contains(QLatin1Char(':'))
        && !normalized.contains(QStringLiteral(".."));
}

QString PortableUpdateCommon::extractionScript(const QString &archivePath, const QString &stagingPath)
{
    return QStringLiteral(R"($archive=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('%1'))
$staging=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('%2'))
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root=[IO.Path]::GetFullPath($staging).TrimEnd([IO.Path]::DirectorySeparatorChar,[IO.Path]::AltDirectorySeparatorChar)+[IO.Path]::DirectorySeparatorChar
$zip=[IO.Compression.ZipFile]::OpenRead($archive)
try {
  foreach($entry in $zip.Entries) {
    $name=$entry.FullName
    if([String]::IsNullOrWhiteSpace($name) -or [IO.Path]::IsPathRooted($name) -or $name -match '(^|[\\/])\.\.([\\/]|$)' -or $name.Contains(':')) { throw "Unsafe archive entry: $name" }
    $target=[IO.Path]::GetFullPath([IO.Path]::Combine($staging,$name))
    if(-not $target.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)) { throw "Archive entry escaped staging: $name" }
    if($name.EndsWith('/') -or $name.EndsWith('\')) { [IO.Directory]::CreateDirectory($target) | Out-Null; continue }
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
    [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target,$true)
  }
} finally { $zip.Dispose() })")
        .arg(encoded(archivePath), encoded(stagingPath));
}
