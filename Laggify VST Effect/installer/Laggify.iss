#define AppName "Laggify"
#define AppVersion "0.1"
#define BundleDir "..\..\build\laggify\LaggifyFx_artefacts\Release\VST3\Laggify.vst3"

[Setup]
AppId={{096E7B0E-1C14-4696-B9A7-6402B2571F91}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=VANTA
DefaultDirName={autopf}\VANTA\Laggify
DefaultGroupName=VANTA\Laggify
UninstallDisplayName=Laggify {#AppVersion}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
DisableProgramGroupPage=yes
OutputDir=..\..\installer-output
OutputBaseFilename=Laggify-0.1-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Files]
; The effect is a separate mixer insert. No VANTA synth files are touched.
Source: "{#BundleDir}\*"; DestDir: "{commoncf64}\VST3\Laggify.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\third-party\*"; DestDir: "{app}\third-party"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Uninstall Laggify"; Filename: "{uninstallexe}"
