$ErrorActionPreference = 'Stop'
# This uses the actual visible setup/uninstaller and writes into system VST3.
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
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern bool PostMessage(IntPtr hwnd, int msg, IntPtr wparam, IntPtr lparam);
    private static Info Read(IntPtr hwnd) {
        var text = new StringBuilder(8192); var cls = new StringBuilder(256);
        GetWindowText(hwnd, text, text.Capacity); GetClassName(hwnd, cls, cls.Capacity);
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
    public static int Checked(IntPtr hwnd) { return SendMessage(hwnd, 0x00F0, IntPtr.Zero, IntPtr.Zero).ToInt32(); }
    public static void Click(IntPtr hwnd) { if (!PostMessage(hwnd, 0x00F5, IntPtr.Zero, IntPtr.Zero)) throw new Exception("Could not click control"); }
}
'@

$setup = (Resolve-Path 'installer-output\Vanta-Beta-0.12-Setup.exe').Path
$root = Join-Path ([Environment]::GetFolderPath('CommonProgramFiles')) 'VST3'
$destination = Join-Path $env:RUNNER_TEMP 'Vanta-0.12-interactive-installer-test'
$bundle9 = Join-Path $root 'Vanta Beta 0.9.vst3'
$bundle8 = Join-Path $root 'Vanta Beta 0.8.vst3'
$current = Join-Path $root 'Vanta Beta 0.12.vst3'
$programs = Join-Path ([Environment]::GetFolderPath('CommonPrograms')) 'Vanta'
$shortcut = Join-Path $programs 'Vanta Beta 0.12.lnk'
$uninstallShortcut = Join-Path $programs 'Uninstall Vanta Beta 0.12.lnk'
$reg = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{86F8C91C-4DD4-467B-919A-8CD576ED4AA9}_is1'
$guard = @($destination, $bundle9, $bundle8, $current, $shortcut, $uninstallShortcut, $reg)
foreach ($path in $guard) {
    if (Test-Path -LiteralPath $path) { throw "UI test path already exists: $path" }
}

function Test-Binary([string]$Bundle, [int]$Version) { Join-Path $Bundle "Contents\x86_64-win\Vanta Beta 0.$Version.vst3" }
function New-TestBundle([string]$Bundle, [int]$Version) {
    $binary = Test-Binary $Bundle $Version
    New-Item -ItemType Directory -Path (Split-Path -Parent $binary) -Force | Out-Null
    [IO.File]::WriteAllText($binary, 'disposable test module')
}
function Button($Controls, [string]$Name) {
    $Controls | Where-Object { $_.Enabled -and $_.Text.Replace('&', '').Trim() -eq $Name } | Select-Object -First 1
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
function Run-SetupUi([bool]$KeepOld, [string]$Name) {
    $log = Join-Path $env:RUNNER_TEMP "vanta-interactive-$Name.log"
    $arguments = @('/SP-', '/NORESTART', "/DIR=`"$destination`"", "/LOG=`"$log`"")
    $process = Start-Process -FilePath $setup -ArgumentList $arguments -PassThru
    $reviewSeen = $false; $finishedClicked = $false; $idle = 0
    $trace = [Collections.Generic.List[string]]::new()
    $lastPage = ''; $lastClickedPage = ''
    $deadline = [DateTime]::UtcNow.AddSeconds(90)
    try {
        while ([DateTime]::UtcNow -lt $deadline) {
            $wizard = [VantaUi]::Top() | Where-Object { $_.ClassName -eq 'TWizardForm' -and $_.Text -match 'Vanta Beta 0\.12' } | Select-Object -First 1
            if ($wizard) {
                $idle = 0
                $controls = @([VantaUi]::Children($wizard.Handle))
                $page = (($controls | Where-Object { $_.Text } | ForEach-Object { "[$($_.ClassName)] $($_.Text)" }) -join ' | ')
                if ($page -ne $lastPage) { $trace.Add($page); $lastPage = $page }
                $choice = $controls | Where-Object { $_.Text -like 'Remove the older VANTA plugin copies listed above*' } | Select-Object -First 1
                if ($choice -and !$reviewSeen) {
                    if ([VantaUi]::Checked($choice.Handle) -ne 1) { throw 'The old-plugin cleanup checkbox should be checked by default.' }
                    if ($KeepOld) {
                        [VantaUi]::Click($choice.Handle)
                        Start-Sleep -Milliseconds 300
                        if ([VantaUi]::Checked($choice.Handle) -ne 0) { throw 'Could not opt out of old-plugin cleanup.' }
                    }
                    $reviewSeen = $true
                }
                $finish = Button $controls 'Finish'
                $install = Button $controls 'Install'
                $next = Button $controls 'Next'
                if (!$next) { $next = Button $controls 'Next >' }
                $onReviewPage = @($controls | Where-Object { $_.Text -eq 'Remove older VANTA plugins' }).Count -gt 0
                # Do not race through a page while its controls are still being
                # created, and never click a stale Next button twice.
                if ($page -ne $lastClickedPage -and !($onReviewPage -and !$choice)) {
                    if ($finish) { [VantaUi]::Click($finish.Handle); $finishedClicked = $true; $lastClickedPage = $page }
                    elseif ($install -and $reviewSeen) { [VantaUi]::Click($install.Handle); $lastClickedPage = $page }
                    elseif ($next) { [VantaUi]::Click($next.Handle); $lastClickedPage = $page }
                }
            } else {
                $idle++
                if ($finishedClicked -and $idle -ge 3) { break }
            }
            Start-Sleep -Milliseconds 400
        }
        if (!$reviewSeen -or !$finishedClicked) {
            $logTail = if (Test-Path -LiteralPath $log) { (Get-Content -LiteralPath $log -Tail 80) -join "`n" } else { '(no setup log)' }
            throw "Setup did not display the review page and finish (review=$reviewSeen, finish=$finishedClicked). Pages:`n$($trace -join "`n")`nWindows:`n$(Describe-Windows)`nLog:`n$logTail"
        }
        if (!$process.WaitForExit(15000)) { throw 'Setup process did not exit.' }
        if ($process.ExitCode -ne 0) { throw "Interactive setup exit code: $($process.ExitCode)" }
        if (!(Select-String -LiteralPath $log -SimpleMatch 'VANTA older plugin detected' -Quiet)) { throw 'Interactive setup did not detect the older plugin.' }
    } finally {
        $process.Refresh()
        if (!$process.HasExited) { Stop-Process -Id $process.Id -Force }
    }
}
function Run-UninstallUi([bool]$Confirm, [string]$Name) {
    $uninstaller = Get-ChildItem -LiteralPath $destination -Filter 'unins*.exe' -File | Select-Object -First 1
    if (!$uninstaller) { throw 'Uninstaller missing before interactive test.' }
    $log = Join-Path $env:RUNNER_TEMP "vanta-interactive-$Name.log"
    $process = Start-Process -FilePath $uninstaller.FullName -ArgumentList @('/NORESTART', "/LOG=`"$log`"") -PassThru
    $extraSeen = $false; $oldSeen = $false; $confirmationSeen = $false; $successSeen = $false; $idle = 0
    $deadline = [DateTime]::UtcNow.AddSeconds(90)
    try {
        while ([DateTime]::UtcNow -lt $deadline) {
            $handled = $false
            foreach ($window in [VantaUi]::Top()) {
                if ($window.Text -notmatch 'Vanta|Uninstall') { continue }
                $controls = @([VantaUi]::Children($window.Handle))
                $text = ($window.Text + ' ' + (($controls | ForEach-Object { $_.Text }) -join ' '))
                $buttonName = ''
                if ($text -match 'check one additional VST3 folder') { $buttonName = 'No'; $extraSeen = $true }
                elseif ($text -match 'Remove older VANTA Beta 0\.1 through 0\.11') { $buttonName = 'Yes'; $oldSeen = $true }
                elseif ($text -match 'Are you sure you want to completely remove') {
                    $buttonName = if ($Confirm) { 'Yes' } else { 'No' }
                    $confirmationSeen = $true
                }
                elseif ($text -match 'Older VANTA plugin binaries were removed') { $buttonName = 'OK' }
                elseif ($text -match 'successfully removed') { $buttonName = 'OK'; $successSeen = $true }
                if ($buttonName) {
                    $button = Button $controls $buttonName
                    if ($button) { [VantaUi]::Click($button.Handle); $handled = $true; break }
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
        if (!$extraSeen -or !$oldSeen -or !$confirmationSeen -or ($Confirm -and !$successSeen)) { throw "Uninstall prompts were not observed. Windows:`n$(Describe-Windows)" }
        if (!$process.WaitForExit(15000)) { throw 'Uninstaller process did not exit.' }
        if ($Confirm -and $process.ExitCode -ne 0) { throw "Interactive uninstaller exit code: $($process.ExitCode)" }
    } finally {
        $process.Refresh()
        if (!$process.HasExited) { Stop-Process -Id $process.Id -Force }
    }
}

try {
    New-Item -ItemType Directory -Path $root -Force | Out-Null
    New-TestBundle $bundle9 9
    Run-SetupUi $true 'keep'
    if (!(Test-Path -LiteralPath (Test-Binary $bundle9 9))) { throw 'Opting out of cleanup did not keep the old plugin.' }
    Run-SetupUi $false 'remove'
    if (Test-Path -LiteralPath (Test-Binary $bundle9 9)) { throw 'Default interactive cleanup did not remove the old plugin.' }

    New-TestBundle $bundle8 8
    $preset = Join-Path $bundle8 'Important preset.vanta'
    [IO.File]::WriteAllText($preset, 'must be kept')
    Run-UninstallUi $false 'cancel-uninstall'
    if (!(Test-Path -LiteralPath (Test-Binary $bundle8 8)) -or !(Test-Path -LiteralPath $current)) { throw 'Cancelling uninstall removed a plugin.' }
    Run-UninstallUi $true 'confirm-uninstall'
    if (Test-Path -LiteralPath (Test-Binary $bundle8 8)) { throw 'Interactive uninstall did not remove the old plugin.' }
    if (Test-Path -LiteralPath (Test-Binary $current 12)) { throw 'Interactive uninstall did not remove the current plugin.' }
    if (!(Test-Path -LiteralPath $preset)) { throw 'Interactive cleanup removed a user preset.' }
    Write-Host 'Interactive installer/uninstaller test passed: checkbox opt-out/default, cancellation, confirmation and preservation.'
} finally {
    foreach ($path in $guard) {
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Recurse -Force }
    }
}
