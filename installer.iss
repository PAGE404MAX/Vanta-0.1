#define AppName "Vanta Beta 0.6"
#define AppVersion "0.6.0"
#define PluginBundle "build\VantaBeta_artefacts\Release\VST3\Vanta Beta 0.6.vst3"
#define StandaloneExe "build\VantaBeta_artefacts\Release\Standalone\Vanta Beta 0.6.exe"

[Setup]
; Keep this beta's uninstall entry separate from earlier betas.
AppId={{BB5FAC9C-152F-4E29-8720-EF74C2AFFFF6}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Vanta
DefaultDirName={autopf}\Vanta\Vanta Beta 0.6
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir=installer-output
OutputBaseFilename=Vanta-Beta-0.6-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\Vanta Beta 0.6.exe

[Files]
Source: "{#PluginBundle}\*"; DestDir: "{commoncf64}\VST3\Vanta Beta 0.6.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StandaloneExe}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{commonprograms}\Vanta\Vanta Beta 0.6"; Filename: "{app}\Vanta Beta 0.6.exe"; WorkingDir: "{app}"

[Run]
Filename: "{app}\Vanta Beta 0.6.exe"; Description: "Launch Vanta Beta 0.6"; Flags: nowait postinstall skipifsilent unchecked

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Vanta Beta 0.6.vst3"
