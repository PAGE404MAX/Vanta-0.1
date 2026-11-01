#define AppName "Vanta Beta 0.5"
#define AppVersion "0.5.0"
#define PluginBundle "build\VantaBeta_artefacts\Release\VST3\Vanta Beta 0.5.vst3"
#define StandaloneExe "build\VantaBeta_artefacts\Release\Standalone\Vanta Beta 0.5.exe"

[Setup]
; Keep this beta's uninstall entry separate from earlier betas.
AppId={{86E724C7-B9C9-4DFE-92A6-6D0B77D74491}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Vanta
DefaultDirName={autopf}\Vanta\Vanta Beta 0.5
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir=installer-output
OutputBaseFilename=Vanta-Beta-0.5-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\Vanta Beta 0.5.exe

[Files]
Source: "{#PluginBundle}\*"; DestDir: "{commoncf64}\VST3\Vanta Beta 0.5.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StandaloneExe}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{commonprograms}\Vanta\Vanta Beta 0.5"; Filename: "{app}\Vanta Beta 0.5.exe"; WorkingDir: "{app}"

[Run]
Filename: "{app}\Vanta Beta 0.5.exe"; Description: "Launch Vanta Beta 0.5"; Flags: nowait postinstall skipifsilent unchecked

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Vanta Beta 0.5.vst3"
