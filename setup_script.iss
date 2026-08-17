#include "version.iss"
#define MyAppName "Prism Downloader"
#define MyAppVersion PRISM_VERSION
#define MyAppPublisher "Tonho Studios"
#define MyAppExeName "PrismDownloader.exe"

[Setup]
; AppId único para o sistema de desinstalação e registro do Windows
AppId={{A924158F-3A1D-4C58-9E4B-8E7D11432A9C}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
SetupIconFile=app_icon.ico
OutputDir=dist
OutputBaseFilename=PrismDownloader_v{#MyAppVersion}_Setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64

[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: checkablealone

[Files]
; Falhar no empacotamento caso os motores obrigatórios não estejam presentes.
Source: "build\Release\yt-dlp.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\Release\ffmpeg.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\Release\*"; DestDir: "{app}"; Excludes: "yt-dlp.exe,ffmpeg.exe,PrismDownloaderTests.exe,PrismMediaToolResolverTests.exe,PrismYtDlpUpdateServiceTests.exe,PrismFakeMediaTool.exe,PrismQueueManagerTests.exe"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppName}"
Name: "{group}\Desinstalar {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppName}"; Description: "Executar o {#MyAppName} agora"; Flags: nowait postinstall skipifsilent
