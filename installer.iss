#define AppName "Vanta Beta 0.8"
#define AppVersion "0.8.0"
#define PluginBundle "build\VantaBeta_artefacts\Release\VST3\Vanta Beta 0.8.vst3"
#define StandaloneExe "build\VantaBeta_artefacts\Release\Standalone\Vanta Beta 0.8.exe"

[Setup]
; Keep this beta's uninstall entry separate from earlier betas.
AppId={{BB5FAC9C-152F-4E29-8720-EF74C2AFFFF8}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Vanta
DefaultDirName={autopf}\Vanta\Vanta Beta 0.8
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir=installer-output
OutputBaseFilename=Vanta-Beta-0.8-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\Vanta Beta 0.8.exe

[Files]
Source: "{#PluginBundle}\*"; DestDir: "{commoncf64}\VST3\Vanta Beta 0.8.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StandaloneExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{commonprograms}\Vanta\Vanta Beta 0.8"; Filename: "{app}\Vanta Beta 0.8.exe"; WorkingDir: "{app}"

[Run]
Filename: "{app}\Vanta Beta 0.8.exe"; Description: "Launch Vanta Beta 0.8"; Flags: nowait postinstall skipifsilent unchecked

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Vanta Beta 0.8.vst3"

[Code]
function InitializeSetup(): Boolean;
var
  Beta: Integer;
  EarlierBundle: String;
begin
  Result := True;
  for Beta := 1 to 7 do
  begin
    EarlierBundle := ExpandConstant('{commoncf64}\VST3\Vanta Beta 0.' + IntToStr(Beta) + '.vst3');
    if DirExists(EarlierBundle) or FileExists(EarlierBundle) then
    begin
      Log('Earlier VANTA VST3 detected: ' + EarlierBundle);
      if not WizardSilent then
        Result := MsgBox('An earlier VANTA beta was found in the standard VST3 folder. These betas share a plugin ID, so duplicate copies can cause FL Studio to load the wrong version.' + #13#10#13#10 +
          'Recommended: back up your projects, cancel setup, uninstall older VANTA betas in Windows Settings, then run this installer again.' + #13#10#13#10 +
          'Install anyway?', mbConfirmation, MB_YESNO) = IDYES;
      Exit;
    end;
  end;
end;
