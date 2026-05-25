#define MyAppName "LabTester"
#define MyAppExeName "LabTester.exe"
#define MyAppPublisher "LabTester Team"
#define MyAppURL "https://github.com/JustF32/LabTester"
#ifndef MyAppVersion
  #define MyAppVersion "0.2v"
#endif
#ifndef MyStageDir
  #define MyStageDir "..\\build\\installer_stage"
#endif
#ifndef MyOutputDir
  #define MyOutputDir "..\\dist"
#endif

[Setup]
AppId={{EE5A4EA2-2C4B-442D-AB2B-2F6CF7A8A99A}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\LabTester
DefaultGroupName=LabTester
AllowNoIcons=yes
OutputDir={#MyOutputDir}
OutputBaseFilename=LabTester-Setup-{#MyAppVersion}
SetupIconFile=..\resources\icons\labtester_icon.ico
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\{#MyAppExeName}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DisableProgramGroupPage=yes
PrivilegesRequired=admin

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#MyStageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Dirs]
Name: "{userappdata}\LabTester\LabTester\labs"

[Icons]
Name: "{autoprograms}\LabTester"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\LabTester"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Установка Microsoft Visual C++ Redistributable..."; Check: FileExists(ExpandConstant('{app}\vc_redist.x64.exe'))
Filename: "{app}\{#MyAppExeName}"; Description: "Запустить LabTester"; Flags: nowait postinstall skipifsilent
