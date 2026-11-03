#define AppName "Vanta Suite"
#define AppVersion "0.13.1"
#define CurrentBeta 13
#define SuiteBuild
#define PluginBundle "build\VantaBeta_artefacts\Release\VST3\Vanta Beta 0.13.vst3"
#define StandaloneExe "build\VantaBeta_artefacts\Release\Standalone\Vanta Beta 0.13.exe"
#define LaggifyBundle "build\laggify\LaggifyFx_artefacts\Release\VST3\Laggify.vst3"
#define SidechainBundle "build\sidechain\VantaSidechainFx_artefacts\Release\VST3\VANTA Sidechain.vst3"
; Embed hashes of the files this specific build owns. The uninstaller can
; then refuse to delete an unrelated replacement at the same path.
#define SynthBinaryHash GetSHA256OfFile(PluginBundle + "\Contents\x86_64-win\Vanta Beta 0.13.vst3")
#define SynthInfoHash GetSHA256OfFile(PluginBundle + "\Contents\Resources\moduleinfo.json")
#define StandaloneHash GetSHA256OfFile(StandaloneExe)
#define LaggifyBinaryHash GetSHA256OfFile(LaggifyBundle + "\Contents\x86_64-win\Laggify.vst3")
#define LaggifyInfoHash GetSHA256OfFile(LaggifyBundle + "\Contents\Resources\moduleinfo.json")
#define SidechainBinaryHash GetSHA256OfFile(SidechainBundle + "\Contents\x86_64-win\VANTA Sidechain.vst3")
#define SidechainInfoHash GetSHA256OfFile(SidechainBundle + "\Contents\Resources\moduleinfo.json")

[Setup]
; This intentionally upgrades the existing VANTA 0.13 setup, rather than
; creating a second uninstaller that would own the same synth files.
AppId={{DD6ACECB-F464-4CE3-82E6-4040DBCBD598}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Vanta
DefaultDirName={autopf}\Vanta\Vanta Beta 0.13
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir=installer-output
OutputBaseFilename=Vanta-Suite-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=VANTA Suite (Synth, Laggify and Sidechain)
UninstallDisplayIcon={app}\Vanta Beta 0.13.exe

[Files]
Source: "{#PluginBundle}\*"; DestDir: "{commoncf64}\VST3\Vanta Beta 0.13.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StandaloneExe}"; DestDir: "{app}"; Flags: ignoreversion
; Write the marker only after conflict checks, before new effect files, so an interrupted install can be repaired.
Source: "scripts\suite-marker.txt"; DestDir: "{app}"; DestName: "Vanta-suite-installed.txt"; Flags: ignoreversion
Source: "{#LaggifyBundle}\*"; DestDir: "{commoncf64}\VST3\Laggify.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SidechainBundle}\*"; DestDir: "{commoncf64}\VST3\VANTA Sidechain.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "Laggify VST Effect\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; DestName: "Laggify-third-party-notices.md"; Flags: ignoreversion
Source: "Sidechain VST Effect\README.md"; DestDir: "{app}"; DestName: "Sidechain-README.md"; Flags: ignoreversion
Source: "Laggify VST Effect\third-party\JUCE-LICENSE.md"; DestDir: "{app}\third-party"; Flags: ignoreversion
Source: "Laggify VST Effect\third-party\VST3-SDK-LICENSE.txt"; DestDir: "{app}\third-party"; Flags: ignoreversion

[Icons]
Name: "{commonprograms}\Vanta\Vanta Beta 0.13"; Filename: "{app}\Vanta Beta 0.13.exe"; WorkingDir: "{app}"
Name: "{commonprograms}\Vanta\Uninstall Vanta Suite"; Filename: "{uninstallexe}"; Comment: "Uninstall VANTA, Laggify and Sidechain"

[Run]
Filename: "{app}\Vanta Beta 0.13.exe"; Description: "Launch Vanta Beta 0.13"; Flags: nowait postinstall skipifsilent unchecked

; Inno's uninstall log only removes files installed by setup. Never recursively
; delete VST3 folders: projects, presets and unknown user files must remain.
[Code]
#include "scripts\vanta-cleanup.iss"
