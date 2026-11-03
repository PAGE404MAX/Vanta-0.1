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
  ExtraFolderPage, OldCopiesPage: TWizardPage;
  ExtraFolderEdit: TNewEdit;
  OldCopiesMemo: TNewMemo;
  RemoveOldCheck: TNewCheckBox;
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
    SuiteBundlePathsAreSafe('VANTA Sidechain.vst3');
end;

function SuiteBinariesExist(): Boolean;
begin
  Result := SuitePathsAreSafe() and
    IsRealFile(AddBackslash(StandardRoot()) + 'Laggify.vst3\Contents\x86_64-win\Laggify.vst3') and
    IsRealFile(AddBackslash(StandardRoot()) + 'VANTA Sidechain.vst3\Contents\x86_64-win\VANTA Sidechain.vst3');
end;

function LegacyLaggifyRegistered(): Boolean;
begin
  Result := RegKeyExists(HKLM64, 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{096E7B0E-1C14-4696-B9A7-6402B2571F91}_is1');
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
    (UnownedBundleHasPlugin('Laggify.vst3') or UnownedBundleHasPlugin('VANTA Sidechain.vst3') or
     PathExists(ExpandConstant('{app}\Laggify-third-party-notices.md')) or
     PathExists(ExpandConstant('{app}\Sidechain-README.md')) or
     PathExists(ExpandConstant('{app}\third-party\JUCE-LICENSE.md')) or
     PathExists(ExpandConstant('{app}\third-party\VST3-SDK-LICENSE.txt')));
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
  Result := Result and SuiteBinariesExist();
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

procedure BrowseExtraFolder(Sender: TObject);
var
  Folder: String;
begin
  Folder := Trim(ExtraFolderEdit.Text);
  if BrowseForFolder('Choose an additional VST3 folder to check for old VANTA plugins', Folder, False) then
    ExtraFolderEdit.Text := Folder;
end;

procedure InitializeWizard();
var
  LabelText: TNewStaticText;
  BrowseButton: TNewButton;
  Problem: String;
begin
  CleanupIsSilent := WizardSilent;
  CleanupRequested := False;
  CleanupFailed := False;
  ExtraFolderPage := CreateCustomPage(wpSelectDir, 'Check another plugin folder (optional)',
    'VANTA automatically checks the standard Windows VST3 folder.');
  LabelText := TNewStaticText.Create(ExtraFolderPage);
  LabelText.Parent := ExtraFolderPage.Surface;
  LabelText.AutoSize := False;
  LabelText.WordWrap := True;
  LabelText.Left := 0;
  LabelText.Top := 0;
  LabelText.Width := ExtraFolderPage.SurfaceWidth;
  LabelText.Height := ScaleY(70);
  LabelText.Caption := 'If you manually put older VANTA plugins in another VST3 folder, choose that folder too. Leave this blank otherwise. Only exact VANTA beta names are checked; your presets and projects are kept.';
  ExtraFolderEdit := TNewEdit.Create(ExtraFolderPage);
  ExtraFolderEdit.Parent := ExtraFolderPage.Surface;
  ExtraFolderEdit.Left := 0;
  ExtraFolderEdit.Top := ScaleY(80);
  ExtraFolderEdit.Width := ExtraFolderPage.SurfaceWidth - ScaleX(90);
  ExtraFolderEdit.Text := SwitchValue('/VANTASCANDIR=');
  BrowseButton := TNewButton.Create(ExtraFolderPage);
  BrowseButton.Parent := ExtraFolderPage.Surface;
  BrowseButton.Left := ExtraFolderEdit.Width + ScaleX(8);
  BrowseButton.Top := ExtraFolderEdit.Top - ScaleY(1);
  BrowseButton.Width := ScaleX(82);
  BrowseButton.Height := ScaleY(23);
  BrowseButton.Caption := 'Browse...';
  BrowseButton.OnClick := @BrowseExtraFolder;

  OldCopiesPage := CreateCustomPage(ExtraFolderPage.ID, 'Remove older VANTA plugins',
    'Old copies share a plugin ID and can make a DAW load the wrong beta.');
  LabelText := TNewStaticText.Create(OldCopiesPage);
  LabelText.Parent := OldCopiesPage.Surface;
  LabelText.AutoSize := False;
  LabelText.WordWrap := True;
  LabelText.Left := 0;
  LabelText.Top := 0;
  LabelText.Width := OldCopiesPage.SurfaceWidth;
  LabelText.Height := ScaleY(52);
  LabelText.Caption := 'After Beta 0.13 installs, VANTA can remove the listed old plugin binaries. Other files, presets, projects, and older standalone apps are kept.';
  OldCopiesMemo := TNewMemo.Create(OldCopiesPage);
  OldCopiesMemo.Parent := OldCopiesPage.Surface;
  OldCopiesMemo.Left := 0;
  OldCopiesMemo.Top := ScaleY(55);
  OldCopiesMemo.Width := OldCopiesPage.SurfaceWidth;
  OldCopiesMemo.Height := OldCopiesPage.SurfaceHeight - ScaleY(97);
  OldCopiesMemo.ReadOnly := True;
  OldCopiesMemo.WordWrap := False;
  OldCopiesMemo.ScrollBars := ssBoth;
  RemoveOldCheck := TNewCheckBox.Create(OldCopiesPage);
  RemoveOldCheck.Parent := OldCopiesPage.Surface;
  RemoveOldCheck.Left := 0;
  RemoveOldCheck.Top := OldCopiesPage.SurfaceHeight - ScaleY(32);
  RemoveOldCheck.Width := OldCopiesPage.SurfaceWidth;
  RemoveOldCheck.Height := ScaleY(22);
  RemoveOldCheck.Caption := 'Remove the older VANTA plugin copies listed above';
  RemoveOldCheck.Checked := True;

  { Custom pages are skipped in silent setup; scan explicitly for automation. }
  if WizardSilent then
  begin
    if NormalizeExtraRoot(ExtraFolderEdit.Text, SelectedExtraRoot, Problem) then
      ScanOldCopies(SelectedExtraRoot)
    else
    begin
      ScanOldCopies('');
      AppendLine(ScanIssues, Problem + ' ' + ExtraFolderEdit.Text);
    end;
  end;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  Problem: String;
begin
  Result := True;
  if (not WizardSilent) and (CurPageID = ExtraFolderPage.ID) then
  begin
    if not NormalizeExtraRoot(ExtraFolderEdit.Text, SelectedExtraRoot, Problem) then
    begin
      MsgBox(Problem, mbError, MB_OK);
      Result := False;
      Exit;
    end;
    ScanOldCopies(SelectedExtraRoot);
    OldCopiesMemo.Text := ReviewText();
  end;
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := (PageID = OldCopiesPage.ID) and (OldCount = 0) and (ScanIssues = '');
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
  else if LegacyLaggifyRegistered() then
    Result := 'The separately installed Laggify has its own uninstaller. Close your DAW, uninstall the old Laggify from Windows Settings, then run Vanta Suite Setup again. Your projects and presets are not removed.'
  else if UnownedEffectExists() then
    Result := 'An existing effect or suite-named app file is not owned by this suite. Back it up and move it aside before continuing; setup will not overwrite unowned files.';
#endif
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep <> ssPostInstall then Exit;
  if WizardSilent then
    CleanupRequested := HasSwitch('/REMOVEOLDVANTA')
  else
    CleanupRequested := RemoveOldCheck.Checked;
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
  Extra, Problem, Question: String;
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
    or not SuitePathsAreSafe()
#endif
    then
  begin
    Log('VANTA uninstall stopped: the current app or VST3 location is linked, unexpected, or inaccessible.');
    if not UninstallSilent then
      MsgBox('The current VANTA app or VST3 location is linked, unexpected, or inaccessible. The uninstaller stopped to protect its target. Resolve the location and retry.', mbError, MB_OK);
    Exit;
  end;

  Extra := SwitchValue('/VANTASCANDIR=');
  if (not UninstallSilent) and (Extra = '') then
    if MsgBox('VANTA also checks the standard VST3 folder for older plugin copies. Do you want to check one additional VST3 folder?', mbConfirmation, MB_YESNO) = IDYES then
      if not BrowseForFolder('Choose the additional VST3 folder', Extra, False) then Extra := '';
  if NormalizeExtraRoot(Extra, SelectedExtraRoot, Problem) then
    ScanOldCopies(SelectedExtraRoot)
  else
  begin
    ScanOldCopies('');
    AppendLine(ScanIssues, Problem + ' ' + Extra);
  end;

  if UninstallSilent then
    CleanupRequested := HasSwitch('/REMOVEOLDVANTA')
  else if (OldCount > 0) or (ScanIssues <> '') then
  begin
    Question := 'Remove older VANTA Beta 0.1 through 0.12 plugin copies too?' + #13#10#13#10 +
      'Found ' + IntToStr(OldCount) + ' candidate(s). Standard folder:' + #13#10 + StandardRoot();
    if SelectedExtraRoot <> '' then Question := Question + #13#10#13#10 + 'Additional folder:' + #13#10 + SelectedExtraRoot;
    Question := Question + #13#10#13#10 +
      'Only recognised VANTA plugin binaries and generated metadata are removed. Extra files, presets, projects and older standalone apps stay. Linked or unrecognised copies are skipped.';
    if ScanIssues <> '' then Question := Question + #13#10#13#10 + 'Some folders could not be searched; the uninstaller will report them.';
    CleanupRequested := MsgBox(Question, mbConfirmation, MB_YESNO) = IDYES;
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
