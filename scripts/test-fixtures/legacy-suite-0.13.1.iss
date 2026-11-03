; Disposable migration fixture only. Its files differ from the current fixture,
; while its identity and installed paths match the released 0.13.1 suite.
[Setup]
AppId={{DD6ACECB-F464-4CE3-82E6-4040DBCBD598}
AppName=Vanta Suite
AppVersion=0.13.1
AppPublisher=Vanta
DefaultDirName={autopf}\Vanta\Vanta Beta 0.13
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir=..\..\installer-output
OutputBaseFilename=Legacy-Suite-0.13.1-Fixture
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=VANTA Suite (Synth, Laggify and Sidechain)

[Files]
Source: "..\..\build\legacy-fixture\Vanta Beta 0.13.vst3\*"; DestDir: "{commoncf64}\VST3\Vanta Beta 0.13.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\..\build\legacy-fixture\Vanta Beta 0.13.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\suite-marker.txt"; DestDir: "{app}"; DestName: "Vanta-suite-installed.txt"; Flags: ignoreversion
Source: "..\..\build\legacy-fixture\Laggify.vst3\*"; DestDir: "{commoncf64}\VST3\Laggify.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\..\build\legacy-fixture\VANTA Sidechain.vst3\*"; DestDir: "{commoncf64}\VST3\VANTA Sidechain.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\Laggify VST Effect\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; DestName: "Laggify-third-party-notices.md"; Flags: ignoreversion
Source: "..\..\Sidechain VST Effect\README.md"; DestDir: "{app}"; DestName: "Sidechain-README.md"; Flags: ignoreversion
Source: "..\..\Laggify VST Effect\third-party\JUCE-LICENSE.md"; DestDir: "{app}\third-party"; Flags: ignoreversion
Source: "..\..\Laggify VST Effect\third-party\VST3-SDK-LICENSE.txt"; DestDir: "{app}\third-party"; Flags: ignoreversion
