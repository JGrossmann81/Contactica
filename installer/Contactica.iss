; Contactica - Windows installer (Inno Setup). Installs for the current user, so no administrator password is needed.
#ifndef Version
  #define Version "0.1.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\build\Release"
#endif

[Setup]
AppId={{4C8E2F10-9A37-4B6D-A1E5-7D20C3B8F946}
AppName=Contactica
AppVersion={#Version}
AppPublisher=3lite Studios
DefaultDirName={autopf}\Contactica
DefaultGroupName=Contactica
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=..\dist
OutputBaseFilename=Contactica-Setup-{#Version}
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\Contactica.exe
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "Put a Contactica icon on the desktop"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE-THIRD-PARTY.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Contactica"; Filename: "{app}\Contactica.exe"
Name: "{group}\Uninstall Contactica"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Contactica"; Filename: "{app}\Contactica.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Contactica.exe"; Description: "Start Contactica"; Flags: nowait postinstall skipifsilent
