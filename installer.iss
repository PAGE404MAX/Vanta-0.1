#define AppName "Vanta Beta 0.13"
#define AppVersion "0.13.0"
#define CurrentBeta 13
#define PluginBundle "build\VantaBeta_artefacts\Release\VST3\Vanta Beta 0.13.vst3"
#define StandaloneExe "build\VantaBeta_artefacts\Release\Standalone\Vanta Beta 0.13.exe"

[Setup]
; Keep this beta's uninstall entry separate from earlier betas.
AppId={{86F8C91C-4DD4-467B-919A-8CD576ED4AA9}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Vanta
DefaultDirName={autopf}\Vanta\Vanta Beta 0.13
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir=installer-output
OutputBaseFilename=Vanta-Beta-0.13-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\Vanta Beta 0.13.exe

[Files]
Source: "{#PluginBundle}\*"; DestDir: "{commoncf64}\VST3\Vanta Beta 0.13.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StandaloneExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{commonprograms}\Vanta\Vanta Beta 0.13"; Filename: "{app}\Vanta Beta 0.13.exe"; WorkingDir: "{app}"
Name: "{commonprograms}\Vanta\Uninstall Vanta Beta 0.13"; Filename: "{uninstallexe}"; Comment: "Uninstall VANTA and optionally remove older VANTA plugin copies"

[Run]
Filename: "{app}\Vanta Beta 0.13.exe"; Description: "Launch Vanta Beta 0.13"; Flags: nowait postinstall skipifsilent unchecked

; The normal Inno uninstall log removes files installed by this setup. Do NOT
; recursively delete the VST3 folder: users may have added their own files there.

[Code]
#include "scripts\vanta-cleanup.iss"
