#define AppName "Vanta Suite"
#define AppVersion "0.14.0"
; 0.13 is the stable bundle path, not the displayed product version.
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
#define MarkerHash GetSHA256OfFile("scripts\suite-marker.txt")
#define NoticesHash GetSHA256OfFile("THIRD_PARTY_NOTICES.md")
#define LaggifyNoticesHash GetSHA256OfFile("Laggify VST Effect\THIRD_PARTY_NOTICES.md")
#define SidechainReadmeHash GetSHA256OfFile("Sidechain VST Effect\README.md")
#define JuceLicenseHash GetSHA256OfFile("Laggify VST Effect\third-party\JUCE-LICENSE.md")
#define VstLicenseHash GetSHA256OfFile("Laggify VST Effect\third-party\VST3-SDK-LICENSE.txt")

; Trust the previous published suite only when its exact bytes still match.
; Fixture builds use deliberately different old files; shipping builds use
; hashes from the released suite 0.13.1 (Actions run 41577087672).
#ifdef SuiteTestLegacyFixture
#define LegacySynthBinaryHash GetSHA256OfFile("build\legacy-fixture\Vanta Beta 0.13.vst3\Contents\x86_64-win\Vanta Beta 0.13.vst3")
#define LegacySynthInfoHash GetSHA256OfFile("build\legacy-fixture\Vanta Beta 0.13.vst3\Contents\Resources\moduleinfo.json")
#define LegacyStandaloneHash GetSHA256OfFile("build\legacy-fixture\Vanta Beta 0.13.exe")
#define LegacyLaggifyBinaryHash GetSHA256OfFile("build\legacy-fixture\Laggify.vst3\Contents\x86_64-win\Laggify.vst3")
#define LegacyLaggifyInfoHash GetSHA256OfFile("build\legacy-fixture\Laggify.vst3\Contents\Resources\moduleinfo.json")
#define LegacySidechainBinaryHash GetSHA256OfFile("build\legacy-fixture\VANTA Sidechain.vst3\Contents\x86_64-win\VANTA Sidechain.vst3")
#define LegacySidechainInfoHash GetSHA256OfFile("build\legacy-fixture\VANTA Sidechain.vst3\Contents\Resources\moduleinfo.json")
#else
#define LegacySynthBinaryHash "A279C0BEB663747F3786F7D372EE25406CD98CB14B11105C0A8384845D4F4998"
#define LegacySynthInfoHash "FB0222EF8CEFB9BA48A4771987FED5A12A1D417C9E492DA8BEA1CFCE9CB07DE6"
#define LegacyStandaloneHash "C78E447B47256481B39772B94FF423685D07E49D83884DFF47F177430FF15C13"
#define LegacyLaggifyBinaryHash "5769A354E93A3474FCF02C6A97D63DA55276EC251446A260905B6C3402CD40BB"
#define LegacyLaggifyInfoHash "5DAFC97D89045C9089F689A107A86901B88FCD3BD3E64C8C1B50FFCD94D1C727"
#define LegacySidechainBinaryHash "41747C0E5F9134F955527C3BDFEDBCD9B6C92303A542574CD986CDDAF47A5C2A"
#define LegacySidechainInfoHash "CD94406B84D69121B87B127828CBD08547B8AFC2CEBCC952BD0020CA1C44509E"
#endif

; The 0.13.2 release was built separately from 0.13.1; its exact published
; binaries must be independently verified. Until the diagnostic build can run,
; blank hashes deliberately fail closed (the old suite can still be safely
; uninstalled before a fresh install). Never accept unverified replacement files.
#define Legacy132SynthBinaryHash ""
#define Legacy132SynthInfoHash ""
#define Legacy132StandaloneHash ""
#define Legacy132LaggifyBinaryHash ""
#define Legacy132LaggifyInfoHash ""
#define Legacy132SidechainBinaryHash ""
#define Legacy132SidechainInfoHash ""

[Setup]
; This intentionally upgrades the existing VANTA 0.13 setup, rather than
; creating a second uninstaller that would own the same synth files.
AppId={{DD6ACECB-F464-4CE3-82E6-4040DBCBD598}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Vanta
DefaultDirName={autopf}\Vanta\Vanta Beta 0.13
DisableProgramGroupPage=yes
DisableStartupPrompt=yes
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
