param([switch]$Suite)
$ErrorActionPreference = 'Stop'
# These tests write to system plugin and Start Menu folders. Only run them in
# disposable GitHub-hosted Windows VMs, never on an actual user's computer.
if ($env:GITHUB_ACTIONS -ne 'true' -or !$env:RUNNER_TEMP -or $env:RUNNER_OS -ne 'Windows' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') {
    throw 'Installer UI test requires an isolated GitHub-hosted Windows runner.'
}

Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class VantaUi {
    public class Info { public IntPtr Handle; public string Text; public string ClassName; public bool Enabled; }
    private delegate bool EnumProc(IntPtr hwnd, IntPtr param);
    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumProc callback, IntPtr param);
    [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr hwnd, EnumProc callback, IntPtr param);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool IsWindowEnabled(IntPtr hwnd);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int size);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr hwnd, StringBuilder text, int size);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern IntPtr SendMessage(IntPtr hwnd, int msg, IntPtr wparam, IntPtr lparam);
    [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW")] private static extern IntPtr ReadText(IntPtr hwnd, int msg, IntPtr wparam, StringBuilder text);
    [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW")] private static extern IntPtr WriteText(IntPtr hwnd, int msg, IntPtr wparam, string text);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern bool PostMessage(IntPtr hwnd, int msg, IntPtr wparam, IntPtr lparam);
    private static Info Read(IntPtr hwnd) {
        var text = new StringBuilder(8192); var cls = new StringBuilder(256);
        GetWindowText(hwnd, text, text.Capacity); GetClassName(hwnd, cls, cls.Capacity);
        // WM_GETTEXT is marshalled across processes for standard edit controls.
        if (cls.ToString().IndexOf("Edit", StringComparison.OrdinalIgnoreCase) >= 0 ||
            cls.ToString().IndexOf("Memo", StringComparison.OrdinalIgnoreCase) >= 0)
            ReadText(hwnd, 0x000D, new IntPtr(text.Capacity), text);
        return new Info { Handle = hwnd, Text = text.ToString(), ClassName = cls.ToString(), Enabled = IsWindowEnabled(hwnd) };
    }
    public static Info[] Top() {
        var list = new List<Info>();
        EnumWindows((h, p) => { if (IsWindowVisible(h)) list.Add(Read(h)); return true; }, IntPtr.Zero);
        return list.ToArray();
    }
    public static Info[] Children(IntPtr parent) {
        var list = new List<Info>();
        EnumChildWindows(parent, (h, p) => { if (IsWindowVisible(h)) list.Add(Read(h)); return true; }, IntPtr.Zero);
        return list.ToArray();
    }
    public static void SetText(IntPtr hwnd, string text) {
        if (WriteText(hwnd, 0x000C, IntPtr.Zero, text) == IntPtr.Zero) throw new Exception("Could not set folder text");
    }
    public static void Click(IntPtr hwnd) { if (!PostMessage(hwnd, 0x00F5, IntPtr.Zero, IntPtr.Zero)) throw new Exception("Could not click control"); }
}
'@

$setupFile = if ($Suite) { 'installer-output\Vanta-Suite-Setup.exe' } else { 'installer-output\Vanta-Beta-0.13-Setup.exe' }
$setup = (Resolve-Path $setupFile).Path
$root = Join-Path ([Environment]::GetFolderPath('CommonProgramFiles')) 'VST3'
$kind = if ($Suite) { 'suite' } else { 'vanta' }
$destination = Join-Path $env:RUNNER_TEMP "Vanta-$kind-old-vst-ui-test"
$customRoot = Join-Path $env:RUNNER_TEMP "Vanta-$kind-custom-vst-ui-test"
$bundle9 = Join-Path $root 'Vanta Beta 0.9.vst3'
$bundle8 = Join-Path $root 'Vanta Beta 0.8.vst3'
$custom6 = Join-Path $customRoot 'Vanta Beta 0.6.vst3'
$current = Join-Path $root 'Vanta Beta 0.13.vst3'
$programs = Join-Path ([Environment]::GetFolderPath('CommonPrograms')) 'Vanta'
$shortcut = Join-Path $programs 'Vanta Beta 0.13.lnk'
$uninstallName = if ($Suite) { 'Uninstall Vanta Suite.lnk' } else { 'Uninstall Vanta Beta 0.13.lnk' }
$uninstallShortcut = Join-Path $programs $uninstallName
$reg = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{DD6ACECB-F464-4CE3-82E6-4040DBCBD598}_is1'
$laggify = Join-Path $root 'Laggify.vst3'
$sidechain = Join-Path $root 'VANTA Sidechain.vst3'
$guard = @($destination, $customRoot, $bundle9, $bundle8, $current, $shortcut, $uninstallShortcut, $reg)
if ($Suite) { $guard += @($laggify, $sidechain) }
foreach ($path in $guard) { if (Test-Path -LiteralPath $path) { throw "UI test path already exists: $path" } }

function Test-Binary([string]$Bundle, [int]$Version) { Join-Path $Bundle "Contents\x86_64-win\Vanta Beta 0.$Version.vst3" }
function New-TestBundle([string]$Bundle, [int]$Version) {
    $binary = Test-Binary $Bundle $Version
    New-Item -ItemType Directory -Path (Split-Path -Parent $binary) -Force | Out-Null
    [IO.File]::WriteAllText($binary, 'disposable test module')
}
function Button($Controls, [string]$Name, [bool]$RequireEnabled = $true) {
    $Controls | Where-Object { (!$RequireEnabled -or $_.Enabled) -and $_.Text.Replace('&', '').Trim() -eq $Name } | Select-Object -First 1
}
function Describe-Windows() {
    $lines = @()
    foreach ($window in [VantaUi]::Top()) {
        if ($window.Text -match 'Vanta|Setup|Uninstall') {
            $lines += "[$($window.ClassName)] $($window.Text)"
            $lines += @([VantaUi]::Children($window.Handle) | Where-Object { $_.Text } | ForEach-Object { "  [$($_.ClassName)] $($_.Text)" })
        }
    }
    return ($lines -join "`n")
}
function ChildText($controls) { ($controls | ForEach-Object { $_.Text }) -join ' ' }

function Run-SetupUi([bool]$SearchOld, [bool]$RemoveOld, [string]$Name, [string[]]$ExpectedPaths, [string]$ExtraFolder = '') {
    $log = Join-Path $env:RUNNER_TEMP "vanta-$kind-interactive-$Name.log"
    $arguments = @('/SP-', '/NORESTART', "/DIR=`"$destination`"", "/LOG=`"$log`"")
    $process = Start-Process -FilePath $setup -ArgumentList $arguments -PassThru
    $questionSeen = $false; $reviewSeen = $false; $folderSearched = $false; $finishedClicked = $false; $idle = 0
    $trace = [Collections.Generic.List[string]]::new(); $lastPage = ''; $lastClickedPage = ''
    $deadline = [DateTime]::UtcNow.AddSeconds(110)
    try {
        while ([DateTime]::UtcNow -lt $deadline) {
            $handled = $false
            foreach ($window in [VantaUi]::Top()) {
                if ($window.Text -notmatch 'Vanta|Setup|Uninstall') { continue }
                $controls = @([VantaUi]::Children($window.Handle))
                $text = $window.Text + ' ' + (ChildText $controls)
                if ($text -match 'Uninstall any old VANTA VSTs\?') {
                    $handled = $true
                    if (!$questionSeen) {
                        $button = Button $controls $(if ($SearchOld) { 'Yes' } else { 'No' })
                        if ($button) { [VantaUi]::Click($button.Handle); $questionSeen = $true }
                    }
                    break
                }
                if ($window.Text -eq 'Older VANTA VSTs found') {
                    $handled = $true
                    if (!$questionSeen -or !$SearchOld) { throw 'Old-version review appeared before the yes/no question.' }
                    if ($reviewSeen) { break }
                    $memo = $controls | Where-Object { $_.ClassName -match 'Memo' -or $_.Text -match 'No older VANTA VSTs found|Vanta Beta 0\.[0-9]+' } | Select-Object -First 1
                    $edit = $controls | Where-Object { $_.ClassName -match 'Edit' -and $_.Handle -ne $memo.Handle } | Select-Object -First 1
                    $remove = Button $controls 'Remove listed VSTs' $false
                    $keep = Button $controls 'Keep / Continue'
                    if (!$edit -or !$memo -or !$remove -or !$keep) { break }
                    if ($ExtraFolder -and !$folderSearched) {
                        [VantaUi]::SetText($edit.Handle, $ExtraFolder)
                        $updated = @([VantaUi]::Children($window.Handle))
                        if ((Button $updated 'Remove listed VSTs' $false).Enabled) { throw 'Unsearched folder input did not disable removal.' }
                        $search = Button $updated 'Search'
                        if (!$search) { throw 'Search button missing from review.' }
                        [VantaUi]::Click($search.Handle); $folderSearched = $true
                        break
                    }
                    $allFound = $true
                    foreach ($path in $ExpectedPaths) { if ($memo.Text -notlike "*$path*") { $allFound = $false } }
                    if (!$allFound) { break }
                    if ($ExpectedPaths.Count -eq 0) {
                        if ($memo.Text -notmatch 'No older VANTA VSTs found') { break }
                        if ($remove.Enabled) { throw 'Removal should be disabled when no older versions were found.' }
                    }
                    foreach ($path in $ExpectedPaths) { if (!(Test-Path -LiteralPath $path)) { throw 'A VST was removed before review.' } }
                    if ($RemoveOld -and !$remove.Enabled) { throw 'Removal is unexpectedly disabled after searching.' }
                    [VantaUi]::Click($(if ($RemoveOld) { $remove.Handle } else { $keep.Handle }))
                    $reviewSeen = $true
                    break
                }
            }
            if (!$handled -and $questionSeen -and (!$SearchOld -or $reviewSeen)) {
                $wizard = [VantaUi]::Top() | Where-Object { $_.ClassName -eq 'TWizardForm' -and $_.Text -match 'Vanta' } | Select-Object -First 1
                if ($wizard) {
                    $idle = 0
                    $controls = @([VantaUi]::Children($wizard.Handle))
                    $page = (($controls | Where-Object { $_.Text } | ForEach-Object { "[$($_.ClassName)] $($_.Text)" }) -join ' | ')
                    if ($page -ne $lastPage) { $trace.Add($page); $lastPage = $page }
                    $finish = Button $controls 'Finish'; $install = Button $controls 'Install'; $next = Button $controls 'Next'
                    if (!$next) { $next = Button $controls 'Next >' }
                    $onReadyPage = @($controls | Where-Object { $_.Text -eq 'Ready to Install' }).Count -gt 0
                    $onFinishPage = @($controls | Where-Object { $_.Text -like 'Completing the Vanta*Setup Wizard*' }).Count -gt 0
                    if ($page -ne $lastClickedPage -and !($onReadyPage -and !$install) -and !($onFinishPage -and !$finish)) {
                        if ($finish) { [VantaUi]::Click($finish.Handle); $finishedClicked = $true; $lastClickedPage = $page }
                        elseif ($install) { [VantaUi]::Click($install.Handle); $lastClickedPage = $page }
                        elseif ($next) { [VantaUi]::Click($next.Handle); $lastClickedPage = $page }
                    }
                } else {
                    $idle++
                    if ($finishedClicked -and $idle -ge 3) { break }
                }
            }
            Start-Sleep -Milliseconds 400
        }
        if (!$questionSeen -or ($SearchOld -ne $reviewSeen) -or !$finishedClicked) {
            $logTail = if (Test-Path -LiteralPath $log) { (Get-Content -LiteralPath $log -Tail 80) -join "`n" } else { '(no setup log)' }
            throw "Setup sequence failed (question=$questionSeen, review=$reviewSeen, finished=$finishedClicked). Pages:`n$($trace -join "`n")`nWindows:`n$(Describe-Windows)`nLog:`n$logTail"
        }
        if (!$process.WaitForExit(15000)) { throw 'Setup process did not exit.' }
        if ($process.ExitCode -ne 0) { throw "Interactive setup exit code: $($process.ExitCode)" }
        $detected = [bool](Select-String -LiteralPath $log -SimpleMatch 'VANTA older plugin detected' -Quiet)
        if (!$SearchOld -and $detected) { throw 'Setup scanned despite answering No.' }
        if ($SearchOld -and $ExpectedPaths.Count -gt 0 -and !$detected) { throw 'Setup did not detect the older plugins.' }
    } finally { $process.Refresh(); if (!$process.HasExited) { Stop-Process -Id $process.Id -Force } }
}

function Run-UninstallUi([bool]$SearchOld, [bool]$RemoveOld, [bool]$Confirm, [string]$Name) {
    $uninstaller = Get-ChildItem -LiteralPath $destination -Filter 'unins*.exe' -File | Select-Object -First 1
    if (!$uninstaller) { throw 'Uninstaller missing before interactive test.' }
    $log = Join-Path $env:RUNNER_TEMP "vanta-$kind-interactive-$Name.log"
    $process = Start-Process -FilePath $uninstaller.FullName -ArgumentList @('/NORESTART', "/LOG=`"$log`"") -PassThru
    $questionSeen = $false; $reviewSeen = $false; $confirmationSeen = $false; $successSeen = $false; $idle = 0
    $deadline = [DateTime]::UtcNow.AddSeconds(110)
    try {
        while ([DateTime]::UtcNow -lt $deadline) {
            $handled = $false
            foreach ($window in [VantaUi]::Top()) {
                if ($window.Text -notmatch 'Vanta|Uninstall') { continue }
                $controls = @([VantaUi]::Children($window.Handle)); $text = $window.Text + ' ' + (ChildText $controls)
                $buttonName = ''; $step = ''
                if ($text -match 'Uninstall any old VANTA VSTs\?') {
                    if (!$questionSeen) { $buttonName = if ($SearchOld) { 'Yes' } else { 'No' }; $step = 'question' }
                    else { $handled = $true; break }
                } elseif ($window.Text -eq 'Older VANTA VSTs found') {
                    if (!$questionSeen -or !$SearchOld) { throw 'Uninstall review appeared before the first question.' }
                    if ($reviewSeen) { $handled = $true; break }
                    $memo = $controls | Where-Object { $_.ClassName -match 'Memo' -or $_.Text -like "*$bundle8*" } | Select-Object -First 1
                    if (!$memo -or $memo.Text -notlike "*$bundle8*") { $handled = $true; break }
                    if (!(Test-Path -LiteralPath (Test-Binary $bundle8 8))) { throw 'Old VST was removed before uninstall confirmation.' }
                    $buttonName = if ($RemoveOld) { 'Remove listed VSTs' } else { 'Keep / Continue' }
                    $step = 'review'
                } elseif ($text -match 'Are you sure you want to completely remove') {
                    if (!$questionSeen -or ($SearchOld -and !$reviewSeen)) { throw 'The old-VST question was not the first uninstall prompt.' }
                    $buttonName = if ($Confirm) { 'Yes' } else { 'No' }; $step = 'confirmation'
                } elseif ($text -match 'Older VANTA plugin binaries were removed') { $buttonName = 'OK' }
                elseif ($text -match 'successfully removed') { $buttonName = 'OK'; $step = 'success' }
                if ($buttonName) {
                    $button = Button $controls $buttonName
                    if ($button) {
                        [VantaUi]::Click($button.Handle)
                        if ($step -eq 'question') { $questionSeen = $true }
                        if ($step -eq 'review') { $reviewSeen = $true }
                        if ($step -eq 'confirmation') { $confirmationSeen = $true }
                        if ($step -eq 'success') { $successSeen = $true }
                        $handled = $true; break
                    }
                }
            }
            if ($handled) { $idle = 0 }
            else {
                $process.Refresh()
                if ($process.HasExited -and $confirmationSeen -and (!$Confirm -or $successSeen)) { $idle++ } else { $idle = 0 }
                if ($idle -ge 4) { break }
            }
            Start-Sleep -Milliseconds 400
        }
        if (!$questionSeen -or ($SearchOld -ne $reviewSeen) -or !$confirmationSeen -or ($Confirm -and !$successSeen)) {
            throw "Uninstall sequence failed (question=$questionSeen, review=$reviewSeen, confirmation=$confirmationSeen, success=$successSeen). Windows:`n$(Describe-Windows)"
        }
        if (!$process.WaitForExit(15000)) { throw 'Uninstaller process did not exit.' }
        if ($Confirm -and $process.ExitCode -ne 0) { throw "Interactive uninstaller exit code: $($process.ExitCode)" }
        $detected = [bool](Select-String -LiteralPath $log -SimpleMatch 'VANTA older plugin detected' -Quiet)
        if (!$SearchOld -and $detected) { throw 'Uninstaller scanned despite answering No.' }
    } finally { $process.Refresh(); if (!$process.HasExited) { Stop-Process -Id $process.Id -Force } }
}

try {
    New-Item -ItemType Directory -Path $root, $customRoot -Force | Out-Null
    New-TestBundle $bundle9 9; New-TestBundle $custom6 6
    $customPreset = Join-Path $custom6 'Important custom preset.vanta'
    [IO.File]::WriteAllText($customPreset, 'must be kept')

    Run-SetupUi -SearchOld $false -RemoveOld $false -Name 'no-scan' -ExpectedPaths @()
    if (!(Test-Path -LiteralPath (Test-Binary $bundle9 9))) { throw 'Answering No removed an old plugin.' }
    Run-SetupUi -SearchOld $true -RemoveOld $false -Name 'review-and-keep' -ExpectedPaths @($bundle9, $custom6) -ExtraFolder $customRoot
    foreach ($path in @((Test-Binary $bundle9 9), (Test-Binary $custom6 6))) { if (!(Test-Path -LiteralPath $path)) { throw 'Keeping old VSTs removed a plugin.' } }
    Run-SetupUi -SearchOld $true -RemoveOld $true -Name 'review-and-remove' -ExpectedPaths @($bundle9, $custom6) -ExtraFolder $customRoot
    foreach ($path in @((Test-Binary $bundle9 9), (Test-Binary $custom6 6))) { if (Test-Path -LiteralPath $path) { throw 'Confirmed old-plugin removal did not remove a plugin.' } }
    if (!(Test-Path -LiteralPath $customPreset)) { throw 'Old-version cleanup removed a user preset.' }
    Run-SetupUi -SearchOld $true -RemoveOld $false -Name 'no-old-versions' -ExpectedPaths @()

    New-TestBundle $bundle8 8
    $preset = Join-Path $bundle8 'Important preset.vanta'; [IO.File]::WriteAllText($preset, 'must be kept')
    Run-UninstallUi -SearchOld $false -RemoveOld $false -Confirm $false -Name 'no-scan-cancel-uninstall'
    Run-UninstallUi -SearchOld $true -RemoveOld $true -Confirm $false -Name 'review-cancel-uninstall'
    if (!(Test-Path -LiteralPath (Test-Binary $bundle8 8)) -or !(Test-Path -LiteralPath $current)) { throw 'Cancelling uninstall removed a plugin.' }
    Run-UninstallUi -SearchOld $true -RemoveOld $true -Confirm $true -Name 'confirm-uninstall'
    if (Test-Path -LiteralPath (Test-Binary $bundle8 8)) { throw 'Interactive uninstall did not remove the old plugin.' }
    if (Test-Path -LiteralPath (Test-Binary $current 13)) { throw 'Interactive uninstall did not remove the current plugin.' }
    if ($Suite) {
        foreach ($effect in @($laggify, $sidechain)) {
            $binary = Join-Path $effect ('Contents\x86_64-win\' + (Split-Path -Leaf $effect))
            if (Test-Path -LiteralPath $binary) { throw "Interactive uninstall did not remove $effect." }
        }
    }
    if (!(Test-Path -LiteralPath $preset) -or !(Test-Path -LiteralPath $customPreset)) { throw 'Interactive cleanup removed a user preset.' }
    Write-Host 'Interactive old-VST prompt tests passed: first question, no/yes, results, custom folder, opt-out, no results, cancel, uninstall, and preservation.'
} finally {
    foreach ($path in $guard) { if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Recurse -Force } }
}
