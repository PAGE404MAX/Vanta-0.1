#define AppName "Vanta Beta 0.1"
#define AppVersion "0.1"
#define PluginBundle "build\VantaBeta_artefacts\Release\VST3\Vanta Beta 0.1.vst3"

[Setup]
AppId={{FC8DD907-A3C0-4DFB-90CC-F4DB3291A0ED}
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
OutputBaseFilename=Vanta-Beta-0.1-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName}

[Files]
Source: "{#PluginBundle}\*"; DestDir: "{app}\Vanta Beta 0.1.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{app}\Vanta Beta 0.1.vst3"
