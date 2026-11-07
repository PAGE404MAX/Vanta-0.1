// VANTA Beta 0.13 cleanup. Only exact, historical names are considered. It
// removes the known plugin binary and generated moduleinfo files, never a
// whole non-empty tree. Unrecognised files, presets and projects are left in
// place. Linked/junction plugin paths are skipped rather than followed.
const
  CurrentBetaNumber = {#CurrentBeta};
  MaxOldCopies = 64;  { enough for both standard and optional roots }
  AttributeDirectory = $10;
  AttributeReparsePoint = $400;

var
  ReviewFolderEdit: TNewEdit;
  ReviewMemo: TNewMemo;
  ReviewSummary: TNewStaticText;
  ReviewRemoveButton: TNewButton;
  OldPaths, OldRoots: array[0..63] of String;
  OldVersions: array[0..63] of Integer;
  OldCount: Integer;
  ScanIssues, CleanupFailures, PreservedFolders, SelectedExtraRoot: String;
  CleanupRequested, CleanupFailed, CleanupIsSilent: Boolean;

function WinFileAttributes(const Name: String): Integer;
  external 'GetFileAttributesW@kernel32.dll stdcall';

function HasSwitch(const Switch: String): Boolean;
var
  I: Integer;
begin
  Result := False;
  for I := 1 to ParamCount do
    if CompareText(ParamStr(I), Switch) = 0 then
    begin
      Result := True;
      Exit;
    end;
end;

function SwitchValue(const Prefix: String): String;
var
  I: Integer;
  Value: String;
begin
  Result := '';
  for I := 1 to ParamCount do
  begin
    Value := ParamStr(I);
    if CompareText(Copy(Value, 1, Length(Prefix)), Prefix) = 0 then
      Result := Copy(Value, Length(Prefix) + 1, Length(Value));
  end;
end;

procedure AppendLine(var Lines: String; const Line: String);
begin
  if Lines <> '' then Lines := Lines + #13#10;
  Lines := Lines + Line;
end;

function PathExists(const Path: String): Boolean;
begin
  Result := WinFileAttributes(Path) <> -1;
end;

{ Check every existing directory, not just the final component. This also
  protects an optional scan folder or custom app path beneath a junction. }
function DirectoryChainIsSafe(const Path: String): Boolean;
var
  Current, Parent: String;
  Attr, ErrorCode: Integer;
begin
  Result := False;
  if Trim(Path) = '' then Exit;
  Current := RemoveBackslashUnlessRoot(ExpandFileName(Path));
  while Current <> '' do
  begin
    Attr := WinFileAttributes(Current);
    if Attr = -1 then
    begin
      ErrorCode := DLLGetLastError;
      if (ErrorCode <> 2) and (ErrorCode <> 3) then Exit;
    end
    else if ((Attr and AttributeDirectory) = 0) or ((Attr and AttributeReparsePoint) <> 0) then Exit;
    if (Length(Current) = 3) and (Copy(Current, 2, 2) = ':\') then Break;
    Parent := RemoveBackslashUnlessRoot(ExtractFileDir(Current));
    if (Parent = '') or (CompareText(Parent, Current) = 0) then Break;
    Current := Parent;
  end;
  Result := True;
end;

function IsRealDirectory(const Path: String): Boolean;
var
  Attr: Integer;
begin
  Attr := WinFileAttributes(Path);
  Result := (Attr <> -1) and ((Attr and AttributeDirectory) <> 0) and
    ((Attr and AttributeReparsePoint) = 0) and DirectoryChainIsSafe(Path);
end;

function IsRealFile(const Path: String): Boolean;
var
  Attr: Integer;
begin
  Attr := WinFileAttributes(Path);
  Result := (Attr <> -1) and ((Attr and AttributeDirectory) = 0) and
    ((Attr and AttributeReparsePoint) = 0) and DirectoryChainIsSafe(ExtractFileDir(Path));
end;

function ExistingDirectoryIsSafe(const Path: String): Boolean;
var
  Attr, ErrorCode: Integer;
begin
  Attr := WinFileAttributes(Path);
  if Attr = -1 then
  begin
    ErrorCode := DLLGetLastError;
    Result := (ErrorCode = 2) or (ErrorCode = 3);  { genuinely absent }
  end
  else
    Result := ((Attr and AttributeDirectory) <> 0) and
      ((Attr and AttributeReparsePoint) = 0);
  Result := Result and DirectoryChainIsSafe(Path);
end;

function ExistingFileIsSafe(const Path: String): Boolean;
var
  Attr, ErrorCode: Integer;
begin
  Attr := WinFileAttributes(Path);
  if Attr = -1 then
  begin
    ErrorCode := DLLGetLastError;
    Result := (ErrorCode = 2) or (ErrorCode = 3);
  end
  else
    Result := ((Attr and AttributeDirectory) = 0) and
      ((Attr and AttributeReparsePoint) = 0);
  Result := Result and DirectoryChainIsSafe(ExtractFileDir(Path));
end;

function IsAbsolutePath(const Path: String): Boolean;
begin
  Result := ((Length(Path) >= 3) and
    (Pos(Uppercase(Copy(Path, 1, 1)), 'ABCDEFGHIJKLMNOPQRSTUVWXYZ') > 0) and
    (Copy(Path, 2, 2) = ':\')) or
    ((Length(Path) >= 3) and (Copy(Path, 1, 2) = '\\'));
end;

function NormalizeExtraRoot(const Input: String; var Normalized, Problem: String): Boolean;
var
  S: String;
begin
  Normalized := '';
  Problem := '';
  S := Trim(Input);
  if S = '' then
  begin
    Result := True;
    Exit;
  end;
  if not IsAbsolutePath(S) then
    Problem := 'Choose an absolute folder path.'
  else
  begin
    S := RemoveBackslashUnlessRoot(ExpandFileName(S));
    if not IsRealDirectory(S) then
      Problem := 'The extra folder does not exist or is a link/junction. It was not searched.'
    else
      Normalized := S;
  end;
  Result := Problem = '';
end;

function StandardRoot(): String;
begin
  Result := ExpandConstant('{commoncf64}\VST3');
end;

function BundleName(const Beta: Integer): String;
begin
  Result := 'Vanta Beta 0.' + IntToStr(Beta) + '.vst3';
end;

procedure AddCandidate(const Root, Path: String; const Beta: Integer);
var
  I: Integer;
begin
  for I := 0 to OldCount - 1 do
    if CompareText(OldPaths[I], Path) = 0 then Exit;
  if OldCount < MaxOldCopies then
  begin
    OldRoots[OldCount] := Root;
    OldPaths[OldCount] := Path;
    OldVersions[OldCount] := Beta;
    OldCount := OldCount + 1;
    Log('VANTA older plugin detected: ' + Path);
  end
  else
    AppendLine(ScanIssues, 'Too many old plugin copies to review safely.');
end;

function MayContainPlugin(const Path: String; const Beta: Integer): Boolean;
var
  Attr: Integer;
  ContentsDir, ArchDir, ResourcesDir: String;
begin
  Attr := WinFileAttributes(Path);
  Result := Attr <> -1;
  if not Result or ((Attr and AttributeDirectory) = 0) or
    ((Attr and AttributeReparsePoint) <> 0) then Exit;
  ContentsDir := AddBackslash(Path) + 'Contents';
  ArchDir := AddBackslash(ContentsDir) + 'x86_64-win';
  ResourcesDir := AddBackslash(ContentsDir) + 'Resources';
  { Links or unusual layouts must be reviewed rather than traversed. }
  if PathExists(ContentsDir) and not IsRealDirectory(ContentsDir) then Exit;
  if PathExists(ArchDir) and not IsRealDirectory(ArchDir) then Exit;
  if PathExists(ResourcesDir) and not IsRealDirectory(ResourcesDir) then Exit;
  if PathExists(AddBackslash(ArchDir) + BundleName(Beta)) or
    PathExists(AddBackslash(ContentsDir) + 'moduleinfo.json') or
    PathExists(AddBackslash(ResourcesDir) + 'moduleinfo.json') then Exit;
  { A previously cleaned bundle can remain as a container for user data. If
    neither its known module nor generated metadata remains, leave it alone. }
  Log('VANTA cleanup left a data-only or unrecognised folder: ' + Path);
  Result := False;
end;

procedure ScanRoot(const Root: String; const IsStandard: Boolean);
var
  Beta, Attr, ErrorCode: Integer;
  Candidate: String;
begin
  Attr := WinFileAttributes(Root);
  if Attr = -1 then
  begin
    ErrorCode := DLLGetLastError;
    if (not IsStandard) or ((ErrorCode <> 2) and (ErrorCode <> 3)) then
      AppendLine(ScanIssues, 'Folder could not be searched: ' + Root + ' (' + SysErrorMessage(ErrorCode) + ')');
    Exit;
  end;
  if not IsRealDirectory(Root) then
  begin
    AppendLine(ScanIssues, 'Folder is not a regular directory (possibly a link/junction): ' + Root);
    Exit;
  end;
  for Beta := 1 to CurrentBetaNumber - 1 do
  begin
    Candidate := AddBackslash(Root) + BundleName(Beta);
    Attr := WinFileAttributes(Candidate);
    if Attr = -1 then
    begin
      ErrorCode := DLLGetLastError;
      if (ErrorCode <> 2) and (ErrorCode <> 3) then
        AppendLine(ScanIssues, 'Could not inspect: ' + Candidate + ' (' + SysErrorMessage(ErrorCode) + ')');
    end
    else if MayContainPlugin(Candidate, Beta) then
      AddCandidate(Root, Candidate, Beta);
  end;
end;

procedure ScanOldCopies(const ExtraRoot: String);
begin
  OldCount := 0;
  ScanIssues := '';
  ScanRoot(StandardRoot(), True);
  if ExtraRoot <> '' then ScanRoot(ExtraRoot, False);
end;

function CandidateProblem(const Path: String; const Beta: Integer): String;
var
  Attr: Integer;
  ContentsDir, ArchDir, Binary: String;
begin
  Result := '';
  Attr := WinFileAttributes(Path);
  if Attr = -1 then Exit;  { It was already removed. }
  if (Attr and AttributeReparsePoint) <> 0 then
  begin
    Result := 'linked plugin is not followed';
    Exit;
  end;
  if (Attr and AttributeDirectory) = 0 then Exit;  { Flat VST3 file. }

  ContentsDir := AddBackslash(Path) + 'Contents';
  ArchDir := AddBackslash(ContentsDir) + 'x86_64-win';
  Binary := AddBackslash(ArchDir) + BundleName(Beta);
  if not IsRealDirectory(ContentsDir) or not IsRealDirectory(ArchDir) or
    not IsRealFile(Binary) then
    Result := 'unrecognised bundle, missing binary, or linked path';
end;

function ReviewText(): String;
var
  I: Integer;
  Problem: String;
begin
  Result := '';
  for I := 0 to OldCount - 1 do
  begin
    Problem := CandidateProblem(OldPaths[I], OldVersions[I]);
    if Problem = '' then
      AppendLine(Result, OldPaths[I])
    else
      AppendLine(Result, '[SKIPPED: ' + Problem + '] ' + OldPaths[I]);
  end;
  if ScanIssues <> '' then
  begin
    if Result <> '' then Result := Result + #13#10#13#10;
    Result := Result + 'Could not search:' + #13#10 + ScanIssues;
  end;
end;

procedure DeleteKnownMetadata(const Path: String);
begin
  { Never follow or remove a link, and leave anything unknown alone. }
  if IsRealFile(Path) then
    if not DeleteFile(Path) then
      Log('VANTA cleanup kept metadata it could not remove: ' + Path);
end;

procedure RemoveEmptyRealDirectory(const Path: String);
begin
  if IsRealDirectory(Path) then RemoveDir(Path);
end;

function RemoveCandidate(const Root, Path: String; const Beta: Integer): Boolean;
var
  Attr, ErrorCode: Integer;
  Problem, ContentsDir, ArchDir, ResourcesDir, Binary: String;
begin
  Result := False;
  if not IsRealDirectory(Root) then
  begin
    AppendLine(CleanupFailures, Root + ' (folder changed or is a link/junction)');
    Exit;
  end;
  Attr := WinFileAttributes(Path);
  if Attr = -1 then
  begin
    ErrorCode := DLLGetLastError;
    if (ErrorCode = 2) or (ErrorCode = 3) then
    begin
      Log('VANTA cleanup: already absent: ' + Path);
      Result := True;
    end
    else
      AppendLine(CleanupFailures, Path + ' (could not inspect: ' + SysErrorMessage(ErrorCode) + ')');
    Exit;
  end;
  Problem := CandidateProblem(Path, Beta);
  if Problem <> '' then
  begin
    AppendLine(CleanupFailures, Path + ' (' + Problem + ')');
    Log('VANTA cleanup skipped: ' + Path + ' (' + Problem + ')');
    Exit;
  end;
  if (Attr and AttributeDirectory) = 0 then
  begin
    if DeleteFile(Path) then
    begin
      Log('VANTA cleanup removed flat VST3: ' + Path);
      Result := True;
    end
    else
      AppendLine(CleanupFailures, Path + ' (in use or access denied)');
    Exit;
  end;

  ContentsDir := AddBackslash(Path) + 'Contents';
  ArchDir := AddBackslash(ContentsDir) + 'x86_64-win';
  ResourcesDir := AddBackslash(ContentsDir) + 'Resources';
  Binary := AddBackslash(ArchDir) + BundleName(Beta);
  { Recheck all path components immediately before deleting the one known
    binary. Never recursively delete unknown data from a VST3 bundle. }
  if not IsRealDirectory(Root) or not IsRealDirectory(Path) or
    not IsRealDirectory(ContentsDir) or not IsRealDirectory(ArchDir) or
    not IsRealFile(Binary) then
  begin
    AppendLine(CleanupFailures, Path + ' (changed or linked during cleanup)');
    Exit;
  end;
  if not DeleteFile(Binary) then
  begin
    AppendLine(CleanupFailures, Path + ' (plugin is in use or access denied)');
    Exit;
  end;
  Log('VANTA cleanup removed plugin binary: ' + Binary);
  if IsRealDirectory(Root) and IsRealDirectory(Path) and IsRealDirectory(ContentsDir) then
  begin
    DeleteKnownMetadata(AddBackslash(ContentsDir) + 'moduleinfo.json');
    if IsRealDirectory(ResourcesDir) then
      DeleteKnownMetadata(AddBackslash(ResourcesDir) + 'moduleinfo.json');
  end;
  if IsRealDirectory(Root) and IsRealDirectory(Path) and IsRealDirectory(ContentsDir) then
  begin
    RemoveEmptyRealDirectory(ResourcesDir);
    RemoveEmptyRealDirectory(ArchDir);
    RemoveEmptyRealDirectory(ContentsDir);
  end;
  if IsRealDirectory(Root) then RemoveEmptyRealDirectory(Path);
  if PathExists(Path) then
  begin
    AppendLine(PreservedFolders, Path);
    Log('VANTA cleanup preserved extra files in: ' + Path);
  end;
  Result := True;
end;

function CurrentBundlePathsAreSafe(): Boolean;
var
  Root, Bundle, ContentsDir, ArchDir, ResourcesDir: String;
begin
  Root := StandardRoot();
  Bundle := AddBackslash(Root) + BundleName(CurrentBetaNumber);
  ContentsDir := AddBackslash(Bundle) + 'Contents';
  ArchDir := AddBackslash(ContentsDir) + 'x86_64-win';
  ResourcesDir := AddBackslash(ContentsDir) + 'Resources';
  Result := ExistingDirectoryIsSafe(Root) and ExistingDirectoryIsSafe(Bundle) and
    ExistingDirectoryIsSafe(ContentsDir) and ExistingDirectoryIsSafe(ArchDir) and
    ExistingDirectoryIsSafe(ResourcesDir) and
    ExistingFileIsSafe(AddBackslash(ArchDir) + BundleName(CurrentBetaNumber)) and
    ExistingFileIsSafe(AddBackslash(ResourcesDir) + 'moduleinfo.json');
end;

function CurrentAppPathsAreSafe(): Boolean;
begin
  Result := ExistingDirectoryIsSafe(ExpandConstant('{app}')) and
    ExistingFileIsSafe(ExpandConstant('{app}\Vanta Beta 0.13.exe')) and
    ExistingFileIsSafe(ExpandConstant('{app}\THIRD_PARTY_NOTICES.md'));
#ifdef SuiteBuild
  Result := Result and ExistingFileIsSafe(ExpandConstant('{app}\Vanta-suite-installed.txt')) and
    ExistingFileIsSafe(ExpandConstant('{app}\Laggify-third-party-notices.md')) and
    ExistingFileIsSafe(ExpandConstant('{app}\Sidechain-README.md')) and
    ExistingDirectoryIsSafe(ExpandConstant('{app}\third-party')) and
    ExistingFileIsSafe(ExpandConstant('{app}\third-party\JUCE-LICENSE.md')) and
    ExistingFileIsSafe(ExpandConstant('{app}\third-party\VST3-SDK-LICENSE.txt'));
#endif
end;

#ifdef SuiteBuild
function SuiteBundlePathsAreSafe(const Name: String): Boolean;
var
  Root, Bundle, ContentsDir, ArchDir, ResourcesDir: String;
begin
  Root := StandardRoot();
  Bundle := AddBackslash(Root) + Name;
  ContentsDir := AddBackslash(Bundle) + 'Contents';
  ArchDir := AddBackslash(ContentsDir) + 'x86_64-win';
  ResourcesDir := AddBackslash(ContentsDir) + 'Resources';
  Result := ExistingDirectoryIsSafe(Root) and ExistingDirectoryIsSafe(Bundle) and
    ExistingDirectoryIsSafe(ContentsDir) and ExistingDirectoryIsSafe(ArchDir) and
    ExistingDirectoryIsSafe(ResourcesDir) and
    ExistingFileIsSafe(AddBackslash(ArchDir) + Name) and
    ExistingFileIsSafe(AddBackslash(ResourcesDir) + 'moduleinfo.json');
end;

function SuitePathsAreSafe(): Boolean;
begin
  Result := SuiteBundlePathsAreSafe('Laggify.vst3') and
    SuiteBundlePathsAreSafe('VANTA Sidechain.vst3') and SuiteBundlePathsAreSafe('VANTA FX.vst3');
end;

function SuiteBinariesExist(): Boolean;
begin
  Result := SuitePathsAreSafe() and
    IsRealFile(AddBackslash(StandardRoot()) + 'Laggify.vst3\Contents\x86_64-win\Laggify.vst3') and
    IsRealFile(AddBackslash(StandardRoot()) + 'VANTA Sidechain.vst3\Contents\x86_64-win\VANTA Sidechain.vst3');
end;

function InstalledFileMatches(const Path, Expected: String): Boolean;
begin
  { Missing files are safe to repair or uninstall. Replacements are not ours. }
  Result := False;
  if not PathExists(Path) then
  begin
    Result := True;
    Exit;
  end;
  if not IsRealFile(Path) then Exit;
  try
    Result := CompareText(GetSHA256OfFile(Path), Expected) = 0;
  except
    Log('Could not verify the installed suite file: ' + Path);
  end;
end;

function InstalledFileMatchesCurrentOrLegacy(const Path, CurrentHash, LegacyHash: String): Boolean;
begin
  { Both hashes are known VANTA suite builds, not an arbitrary replacement. }
  Result := InstalledFileMatches(Path, CurrentHash);
  if (not Result) and (LegacyHash <> '') then
    Result := InstalledFileMatches(Path, LegacyHash);
end;

function InstalledFileMatchesAnySuiteBuild(const Path, CurrentHash, LegacyHash, Legacy132Hash: String): Boolean;
begin
  Result := InstalledFileMatchesCurrentOrLegacy(Path, CurrentHash, LegacyHash);
  if (not Result) and (Legacy132Hash <> '') then
    Result := InstalledFileMatches(Path, Legacy132Hash);
end;

function SuiteInstalledFilesMatch(): Boolean;
var
  Synth, Laggify, Sidechain: String;
begin
  Synth := AddBackslash(StandardRoot()) + 'Vanta Beta 0.13.vst3\Contents';
  Laggify := AddBackslash(StandardRoot()) + 'Laggify.vst3\Contents';
  Sidechain := AddBackslash(StandardRoot()) + 'VANTA Sidechain.vst3\Contents';
  Result := InstalledFileMatchesAnySuiteBuild(AddBackslash(Synth) + 'x86_64-win\Vanta Beta 0.13.vst3', '{#SynthBinaryHash}', '{#LegacySynthBinaryHash}', '{#Legacy132SynthBinaryHash}') and
    InstalledFileMatchesAnySuiteBuild(AddBackslash(Synth) + 'Resources\moduleinfo.json', '{#SynthInfoHash}', '{#LegacySynthInfoHash}', '{#Legacy132SynthInfoHash}') and
    InstalledFileMatchesAnySuiteBuild(ExpandConstant('{app}\Vanta Beta 0.13.exe'), '{#StandaloneHash}', '{#LegacyStandaloneHash}', '{#Legacy132StandaloneHash}') and
    InstalledFileMatchesAnySuiteBuild(AddBackslash(Laggify) + 'x86_64-win\Laggify.vst3', '{#LaggifyBinaryHash}', '{#LegacyLaggifyBinaryHash}', '{#Legacy132LaggifyBinaryHash}') and
    InstalledFileMatchesAnySuiteBuild(AddBackslash(Laggify) + 'Resources\moduleinfo.json', '{#LaggifyInfoHash}', '{#LegacyLaggifyInfoHash}', '{#Legacy132LaggifyInfoHash}') and
    InstalledFileMatchesAnySuiteBuild(AddBackslash(Sidechain) + 'x86_64-win\VANTA Sidechain.vst3', '{#SidechainBinaryHash}', '{#LegacySidechainBinaryHash}', '{#Legacy132SidechainBinaryHash}') and
    InstalledFileMatchesAnySuiteBuild(AddBackslash(Sidechain) + 'Resources\moduleinfo.json', '{#SidechainInfoHash}', '{#LegacySidechainInfoHash}', '{#Legacy132SidechainInfoHash}') and
    InstalledFileMatches(AddBackslash(StandardRoot()) + 'VANTA FX.vst3\Contents\x86_64-win\VANTA FX.vst3', '{#FxBinaryHash}') and
    InstalledFileMatches(AddBackslash(StandardRoot()) + 'VANTA FX.vst3\Contents\Resources\moduleinfo.json', '{#FxInfoHash}') and
    InstalledFileMatches(ExpandConstant('{app}\Vanta-suite-installed.txt'), '{#MarkerHash}') and
    InstalledFileMatches(ExpandConstant('{app}\THIRD_PARTY_NOTICES.md'), '{#NoticesHash}') and
    InstalledFileMatches(ExpandConstant('{app}\Laggify-third-party-notices.md'), '{#LaggifyNoticesHash}') and
    InstalledFileMatches(ExpandConstant('{app}\Sidechain-README.md'), '{#SidechainReadmeHash}') and
    InstalledFileMatches(ExpandConstant('{app}\third-party\JUCE-LICENSE.md'), '{#JuceLicenseHash}') and
    InstalledFileMatches(ExpandConstant('{app}\third-party\VST3-SDK-LICENSE.txt'), '{#VstLicenseHash}');
end;

function LegacyLaggifyRegistered(): Boolean;
begin
  Result := RegKeyExists(HKLM64, 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{096E7B0E-1C14-4696-B9A7-6402B2571F91}_is1');
end;

function Vanta13Registered(): Boolean;
begin
  Result := RegKeyExists(HKLM64, 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{DD6ACECB-F464-4CE3-82E6-4040DBCBD598}_is1');
end;

function ExistingVantaInstallMatches(): Boolean;
var
  Key, PreviousPath, Version, Target: String;
begin
  Result := True;
  if not Vanta13Registered() then Exit;
  Key := 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{DD6ACECB-F464-4CE3-82E6-4040DBCBD598}_is1';
  Result := False;
  if not RegQueryStringValue(HKLM64, Key, 'DisplayVersion', Version) then Exit;
  if (Version <> '0.13.0') and (Version <> '0.13.1') and (Version <> '0.13.2') and (Version <> '0.14.0') and (Version <> '0.15.0') then Exit;
  if not RegQueryStringValue(HKLM64, Key, 'InstallLocation', PreviousPath) or (Trim(PreviousPath) = '') then
    if not RegQueryStringValue(HKLM64, Key, 'Inno Setup: App Path', PreviousPath) then Exit;
  if Trim(PreviousPath) = '' then Exit;
  PreviousPath := RemoveBackslashUnlessRoot(ExpandFileName(PreviousPath));
  Target := RemoveBackslashUnlessRoot(ExpandFileName(ExpandConstant('{app}')));
  { The 0.13 uninstaller must be updated in place. Moving the app folder
    would leave a second old uninstaller able to delete suite-owned files. }
  Result := CompareText(PreviousPath, Target) = 0;
end;

function UnownedBundleHasPlugin(const Name: String): Boolean;
var
  Bundle: String;
begin
  Bundle := AddBackslash(StandardRoot()) + Name + '\Contents';
  Result := PathExists(AddBackslash(Bundle) + 'x86_64-win\' + Name) or
    PathExists(AddBackslash(Bundle) + 'Resources\moduleinfo.json') or
    PathExists(AddBackslash(Bundle) + 'moduleinfo.json');
end;

function UnownedEffectExists(): Boolean;
begin
  { A previous suite installation carries a marker in the same app folder.
    Protect an unowned binary/manifest, but allow data-only directories left
    by a prior uninstall. Extra files are never recursively removed. }
  Result := (not IsRealFile(ExpandConstant('{app}\Vanta-suite-installed.txt'))) and
    (((not Vanta13Registered()) and
      (UnownedBundleHasPlugin('Vanta Beta 0.13.vst3') or PathExists(ExpandConstant('{app}\Vanta Beta 0.13.exe')) or
       PathExists(ExpandConstant('{app}\THIRD_PARTY_NOTICES.md')))) or
     UnownedBundleHasPlugin('VANTA FX.vst3') or UnownedBundleHasPlugin('Laggify.vst3') or UnownedBundleHasPlugin('VANTA Sidechain.vst3') or
     PathExists(ExpandConstant('{app}\Laggify-third-party-notices.md')) or
     PathExists(ExpandConstant('{app}\Sidechain-README.md')) or
     PathExists(ExpandConstant('{app}\third-party\JUCE-LICENSE.md')) or
     PathExists(ExpandConstant('{app}\third-party\VST3-SDK-LICENSE.txt')));
end;
#endif

#ifndef SuiteBuild
function SuiteInstallRegisteredOrMarked(): Boolean;
var
  Key, DisplayName, PreviousPath, Marker: String;
begin
  { The VANTA-only setup shares an AppId and synth paths with the suite. It
    must never replace that suite's uninstaller or downgrade its synth. Check
    the registered install as well as the requested directory, and treat even
    an unexpected marker path as a conflict rather than following links. }
  Marker := ExpandConstant('{app}\Vanta-suite-installed.txt');
  Result := (not ExistingFileIsSafe(Marker)) or PathExists(Marker);
  if Result then Exit;
  Key := 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{DD6ACECB-F464-4CE3-82E6-4040DBCBD598}_is1';
  if not RegKeyExists(HKLM64, Key) then Exit;
  if RegQueryStringValue(HKLM64, Key, 'DisplayName', DisplayName) and
     (Pos('VANTA SUITE', Uppercase(DisplayName)) > 0) then
  begin
    Result := True;
    Exit;
  end;
  PreviousPath := '';
  if not RegQueryStringValue(HKLM64, Key, 'InstallLocation', PreviousPath) or (Trim(PreviousPath) = '') then
    if not RegQueryStringValue(HKLM64, Key, 'Inno Setup: App Path', PreviousPath) then PreviousPath := '';
  if Trim(PreviousPath) = '' then
    Result := True  { Unknown ownership: stop rather than replace an uninstaller. }
  else
  begin
    Marker := AddBackslash(PreviousPath) + 'Vanta-suite-installed.txt';
    Result := (not ExistingFileIsSafe(Marker)) or PathExists(Marker);
  end;
end;
#endif

function InstalledCurrentBinaryExists(): Boolean;
var
  Binary: String;
begin
  Binary := AddBackslash(StandardRoot()) + BundleName(CurrentBetaNumber) +
    '\Contents\x86_64-win\' + BundleName(CurrentBetaNumber);
  Result := CurrentBundlePathsAreSafe() and CurrentAppPathsAreSafe() and
    IsRealFile(Binary) and IsRealFile(ExpandConstant('{app}\Vanta Beta 0.13.exe'));
#ifdef SuiteBuild
  Result := Result and SuiteBinariesExist() and SuiteInstalledFilesMatch();
#endif
end;

procedure RunOldCleanup();
var
  I: Integer;
  Success: Boolean;
  MessageText: String;
begin
  CleanupFailures := ScanIssues;
  PreservedFolders := '';
  for I := 0 to OldCount - 1 do
  begin
    Success := RemoveCandidate(OldRoots[I], OldPaths[I], OldVersions[I]);
    if not Success then Log('VANTA cleanup could not remove: ' + OldPaths[I]);
  end;
  CleanupFailed := CleanupFailures <> '';
  if CleanupFailed then
  begin
    Log('VANTA cleanup incomplete. Close DAWs and retry. Details:' + #13#10 + CleanupFailures);
    if not CleanupIsSilent then
    begin
      MessageText := 'Some older VANTA plugin copies could not be removed. Close FL Studio and other audio programs, then retry. Linked or unrecognised folders are never deleted automatically.' + #13#10#13#10 +
        CleanupFailures;
      MsgBox(MessageText, mbError, MB_OK);
    end;
  end;
  if PreservedFolders <> '' then
  begin
    Log('VANTA cleanup kept extra files in:' + #13#10 + PreservedFolders);
    if not CleanupIsSilent then
      MsgBox('Older VANTA plugin binaries were removed. Extra files, including any projects or presets inside their folders, were kept in:' + #13#10#13#10 +
        PreservedFolders, mbInformation, MB_OK);
  end;
  if (not CleanupFailed) and (PreservedFolders = '') then
    Log('VANTA cleanup completed successfully.');
end;

{ A yes answer only starts a search. Removal is a separate, explicit choice
  after the full list has been reviewed. Setup/uninstall cancellation still
  happens before any older files are changed. }
function AskToFindOldVsts(const DuringUninstall: Boolean): Boolean;
var
  ContinueText: String;
begin
  if DuringUninstall then
    ContinueText := 'No keeps old versions and continues to the uninstall confirmation.'
  else
    ContinueText := 'No keeps old versions and continues setup.';
  Result := MsgBox('Uninstall any old VANTA VSTs?' + #13#10#13#10 +
    'Yes searches the standard VST3 folder and lets you review the results. You can also search one additional folder. Nothing is removed yet.' + #13#10#13#10 +
    ContinueText, mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES;
end;

procedure ScanRequestedFolders(const RequestedExtra: String);
var
  Problem: String;
begin
  if NormalizeExtraRoot(RequestedExtra, SelectedExtraRoot, Problem) then
    ScanOldCopies(SelectedExtraRoot)
  else
  begin
    SelectedExtraRoot := '';
    ScanOldCopies('');
    AppendLine(ScanIssues, Problem + ' ' + RequestedExtra);
  end;
end;

procedure RefreshReview();
var
  Text: String;
begin
  Text := ReviewText();
  if OldCount = 0 then
  begin
    if Text <> '' then Text := #13#10#13#10 + Text;
    Text := 'No older VANTA VSTs found in the searched folder(s).' + Text;
  end;
  ReviewMemo.Text := Text;
  ReviewSummary.Caption := 'Found ' + IntToStr(OldCount) +
    ' older VANTA VST candidate(s). Review the paths before choosing what to do.';
  ReviewRemoveButton.Enabled := OldCount > 0;
end;

procedure ReviewFolderChanged(Sender: TObject);
begin
  { Do not let an unsearched pasted path look as if it was included. }
  if CompareText(Trim(ReviewFolderEdit.Text), SelectedExtraRoot) <> 0 then
  begin
    ReviewSummary.Caption := 'Click Search to include the folder you entered before removing anything.';
    ReviewRemoveButton.Enabled := False;
  end
  else
    RefreshReview();
end;

procedure SearchReviewFolder(Sender: TObject);
var
  Folder, Problem: String;
begin
  if not NormalizeExtraRoot(ReviewFolderEdit.Text, Folder, Problem) then
  begin
    MsgBox(Problem, mbError, MB_OK);
    Exit;
  end;
  SelectedExtraRoot := Folder;
  ScanOldCopies(SelectedExtraRoot);
  ReviewFolderEdit.Text := SelectedExtraRoot;
  RefreshReview();
end;

procedure BrowseReviewFolder(Sender: TObject);
var
  Folder: String;
begin
  Folder := Trim(ReviewFolderEdit.Text);
  if BrowseForFolder('Choose an additional VST3 folder to search for old VANTA plugins', Folder, False) then
  begin
    ReviewFolderEdit.Text := Folder;
    SearchReviewFolder(Sender);
  end;
end;

function ReviewOldVsts(const RequestedExtra: String): Boolean;
var
  Form: TSetupForm;
  LabelText: TNewStaticText;
  SearchButton, BrowseButton, KeepButton: TNewButton;
begin
  Result := False;
#if Ver >= EncodeVer(6, 6, 0)
  Form := CreateCustomForm(580, 440, False, False);
#else
  Form := CreateCustomForm;
  Form.ClientWidth := ScaleX(580);
  Form.ClientHeight := ScaleY(440);
#endif
  try
    Form.Caption := 'Older VANTA VSTs found';
    ReviewSummary := TNewStaticText.Create(Form);
    ReviewSummary.Parent := Form;
    ReviewSummary.Left := ScaleX(12);
    ReviewSummary.Top := ScaleY(12);
    ReviewSummary.Width := Form.ClientWidth - ScaleX(24);
    ReviewSummary.Height := ScaleY(36);
    ReviewSummary.AutoSize := False;
    ReviewSummary.WordWrap := True;

    ReviewMemo := TNewMemo.Create(Form);
    ReviewMemo.Parent := Form;
    ReviewMemo.Left := ScaleX(12);
    ReviewMemo.Top := ScaleY(50);
    ReviewMemo.Width := Form.ClientWidth - ScaleX(24);
    ReviewMemo.Height := ScaleY(205);
    ReviewMemo.ReadOnly := True;
    ReviewMemo.WordWrap := False;
    ReviewMemo.ScrollBars := ssBoth;

    LabelText := TNewStaticText.Create(Form);
    LabelText.Parent := Form;
    LabelText.Left := ScaleX(12);
    LabelText.Top := ScaleY(264);
    LabelText.Width := Form.ClientWidth - ScaleX(24);
    LabelText.Height := ScaleY(20);
    LabelText.AutoSize := False;
    LabelText.Caption := 'Additional VST3 folder (optional):';

    ReviewFolderEdit := TNewEdit.Create(Form);
    ReviewFolderEdit.Parent := Form;
    ReviewFolderEdit.Left := ScaleX(12);
    ReviewFolderEdit.Top := ScaleY(286);
    ReviewFolderEdit.Width := Form.ClientWidth - ScaleX(201);
    ReviewFolderEdit.Height := ScaleY(23);
    ReviewFolderEdit.Text := RequestedExtra;
    ReviewFolderEdit.OnChange := @ReviewFolderChanged;

    SearchButton := TNewButton.Create(Form);
    SearchButton.Parent := Form;
    SearchButton.Caption := 'Search';
    SearchButton.Left := Form.ClientWidth - ScaleX(180);
    SearchButton.Top := ScaleY(285);
    SearchButton.Width := ScaleX(78);
    SearchButton.Height := ScaleY(25);
    SearchButton.OnClick := @SearchReviewFolder;

    BrowseButton := TNewButton.Create(Form);
    BrowseButton.Parent := Form;
    BrowseButton.Caption := 'Browse...';
    BrowseButton.Left := Form.ClientWidth - ScaleX(96);
    BrowseButton.Top := ScaleY(285);
    BrowseButton.Width := ScaleX(84);
    BrowseButton.Height := ScaleY(25);
    BrowseButton.OnClick := @BrowseReviewFolder;

    LabelText := TNewStaticText.Create(Form);
    LabelText.Parent := Form;
    LabelText.Left := ScaleX(12);
    LabelText.Top := ScaleY(319);
    LabelText.Width := Form.ClientWidth - ScaleX(24);
    LabelText.Height := ScaleY(58);
    LabelText.AutoSize := False;
    LabelText.WordWrap := True;
    LabelText.Caption := 'The standard VST3 folder is always checked. Only recognised VANTA Beta 0.1-0.12 plugin binaries and their generated metadata may be removed. Projects, presets, other plugins and standalone apps are kept.';

    ReviewRemoveButton := TNewButton.Create(Form);
    ReviewRemoveButton.Parent := Form;
    ReviewRemoveButton.Caption := 'Remove listed VSTs';
    ReviewRemoveButton.Width := ScaleX(166);
    ReviewRemoveButton.Height := ScaleY(26);
    ReviewRemoveButton.Left := Form.ClientWidth - ScaleX(333);
    ReviewRemoveButton.Top := Form.ClientHeight - ScaleY(38);
    ReviewRemoveButton.ModalResult := mrOk;

    KeepButton := TNewButton.Create(Form);
    KeepButton.Parent := Form;
    KeepButton.Caption := 'Keep / Continue';
    KeepButton.Width := ScaleX(149);
    KeepButton.Height := ScaleY(26);
    KeepButton.Left := Form.ClientWidth - ScaleX(161);
    KeepButton.Top := Form.ClientHeight - ScaleY(38);
    KeepButton.ModalResult := mrCancel;
    KeepButton.Default := True;
    KeepButton.Cancel := True;
    Form.ActiveControl := KeepButton;

    RefreshReview();
    ReviewFolderChanged(nil);
    Result := Form.ShowModal() = mrOk;
  finally
    if ReviewFolderEdit <> nil then ReviewFolderEdit.OnChange := nil;
    Form.Free();
    ReviewFolderEdit := nil;
    ReviewMemo := nil;
    ReviewSummary := nil;
    ReviewRemoveButton := nil;
  end;
end;

procedure InitializeWizard();
var
  Extra: String;
begin
  CleanupIsSilent := WizardSilent;
  CleanupRequested := False;
  CleanupFailed := False;
  Extra := SwitchValue('/VANTASCANDIR=');
  if WizardSilent then
    { Keep the existing unattended scan and explicit /REMOVEOLDVANTA policy. }
    ScanRequestedFolders(Extra)
  else if AskToFindOldVsts(False) then
  begin
    ScanRequestedFolders(Extra);
    CleanupRequested := ReviewOldVsts(Extra);
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  if not CurrentBundlePathsAreSafe() or not CurrentAppPathsAreSafe() then
  begin
    Result := 'The Beta 0.13 app or VST3 location contains a link/junction, an unexpected item, or cannot be inspected. Setup will not overwrite it. Resolve the location and retry.';
    Exit;
  end;
#ifdef SuiteBuild
  if not SuitePathsAreSafe() then
    Result := 'A Laggify or Sidechain VST3 path is linked, unexpected, or inaccessible. Setup stopped to protect your files.'
  else if not ExistingVantaInstallMatches() then
    Result := 'VANTA 0.13 or a suite is already registered in another folder or is an unrecognised version. Choose its existing app folder to upgrade it in place, or uninstall it safely first.'
  else if LegacyLaggifyRegistered() then
    Result := 'The separately installed Laggify has its own uninstaller. Close your DAW, uninstall the old Laggify from Windows Settings, then run Vanta Suite Setup again. Your projects and presets are not removed.'
  else if IsRealFile(ExpandConstant('{app}\Vanta-suite-installed.txt')) and not SuiteInstalledFilesMatch() then
    Result := 'An installed suite file has been replaced or cannot be verified. Back up and move that changed file aside before repairing; setup will not overwrite an unowned replacement.'
  else if UnownedEffectExists() then
    Result := 'An existing plugin or suite-named app file is not owned by this setup. Back it up and move it aside before continuing; setup will not overwrite unowned files.';
#else
  if SuiteInstallRegisteredOrMarked() then
    Result := 'VANTA Suite is installed. Use VANTA Suite Setup to upgrade or repair it; this VANTA-only installer will not replace its shared uninstaller or plugins.';
#endif
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep <> ssPostInstall then Exit;
  if WizardSilent then
    CleanupRequested := HasSwitch('/REMOVEOLDVANTA');
  if not CleanupRequested then
  begin
    Log('VANTA cleanup was not selected; older plugins were kept.');
    Exit;
  end;
  { Do not remove a working older plugin if the new app/plugin did not land. }
  if not InstalledCurrentBinaryExists() then
  begin
    CleanupFailed := True;
    Log('VANTA cleanup skipped because the new app or VST3 is missing or unsafe.');
    if not WizardSilent then
      MsgBox('Older VANTA plugins were kept because the new app or plugin could not be verified.', mbError, MB_OK);
    Exit;
  end;
  RunOldCleanup();
end;

function GetCustomSetupExitCode(): Integer;
begin
  Result := 0;
  if CleanupRequested and CleanupFailed then Result := 20;
end;

function InitializeUninstall(): Boolean;
var
  Extra: String;
begin
  Result := False;
  CleanupIsSilent := UninstallSilent;
  CleanupRequested := False;
  CleanupFailed := False;
#ifdef SuiteBuild
  if LegacyLaggifyRegistered() then
  begin
    Log('Suite uninstall stopped: a separate Laggify installer is registered for the same plugin path.');
    if not UninstallSilent then
      MsgBox('A separate Laggify installation was detected. Close your DAW, uninstall the separate Laggify from Windows Settings, then run Vanta Suite Setup to repair the suite before uninstalling it.', mbError, MB_OK);
    Result := False;
    Exit;
  end;
#endif
  if not CurrentBundlePathsAreSafe() or not CurrentAppPathsAreSafe()
#ifdef SuiteBuild
    or not SuitePathsAreSafe() or not SuiteInstalledFilesMatch()
#endif
    then
  begin
    Log('VANTA uninstall stopped: the current app or VST3 location is linked, unexpected, or inaccessible.');
    if not UninstallSilent then
      MsgBox('A current VANTA app or VST3 location is linked, unexpected, inaccessible, or contains a changed file. The uninstaller stopped to protect it. Back up and move replacements aside, resolve unsafe paths, then retry.', mbError, MB_OK);
    Exit;
  end;

  Extra := SwitchValue('/VANTASCANDIR=');
  if UninstallSilent then
  begin
    ScanRequestedFolders(Extra);
    CleanupRequested := HasSwitch('/REMOVEOLDVANTA');
  end
  else if AskToFindOldVsts(True) then
  begin
    ScanRequestedFolders(Extra);
    CleanupRequested := ReviewOldVsts(Extra);
  end;
  Result := True;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  { This is after the user has confirmed the normal uninstall; cancelling it
    must never delete an older copy. Silent cleanup requires an explicit flag. }
  if (CurUninstallStep = usPostUninstall) and CleanupRequested then
    RunOldCleanup();
end;
