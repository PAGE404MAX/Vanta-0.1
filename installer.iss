#define AppName "Vanta Beta 0.4"
#define AppVersion "0.4.0"
#define PluginBundle "build\VantaBeta_artefacts\Release\VST3\Vanta Beta 0.4.vst3"

[Setup]
; A separate uninstall entry leaves the previous beta removable on its own.
AppId={{548E4883-D7D7-4D32-987B-AC48A9979870}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Vanta
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir=installer-output
OutputBaseFilename=Vanta-Beta-0.4-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName}

[Files]
Source: "{#PluginBundle}\*"; DestDir: "{app}\Vanta Beta 0.4.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{app}\Vanta Beta 0.4.vst3"
