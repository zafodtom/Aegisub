#ifndef MyAppVersion
  #define MyAppVersion "1.0.0"
#endif

#ifndef SourceDir
  #define SourceDir "..\dist\SRTune-1.0.0-x64"
#endif

#ifndef OutputDir
  #define OutputDir "..\dist"
#endif

#ifndef OutputBaseFilename
  #define OutputBaseFilename "SRTune-" + MyAppVersion + "-Setup-x64"
#endif

#define MyAppName "SRTune"
#define MyAppExeName "SRTune.exe"

[Setup]
AppId={{9DA1AFE7-287A-44A5-84DB-4F9ADB5A988C}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=SRTune Project
DefaultDirName={localappdata}\Programs\SRTune
DefaultGroupName=SRTune
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename={#OutputBaseFilename}
SetupIconFile=..\winui\Aegisub.WinUI\Aegisub.WinUI\Assets\SRTune.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
VersionInfoVersion={#MyAppVersion}
VersionInfoProductName=SRTune
VersionInfoDescription=SRTune subtitle editor

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\SRTune"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\SRTune"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Vytvořit ikonu na ploše"; GroupDescription: "Další možnosti:"; Flags: unchecked

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Spustit SRTune"; Flags: nowait postinstall skipifsilent
