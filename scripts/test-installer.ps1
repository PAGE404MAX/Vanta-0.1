param([switch]$SkipLaunch)
$ErrorActionPreference = 'Stop'

# This test installs into system VST3 and Start Menu folders. It must ONLY run
# on a disposable GitHub-hosted Windows runner, never on a user's machine.
if ($env:GITHUB_ACTIONS -ne 'true' -or !$env:RUNNER_TEMP -or $env:RUNNER_OS -ne 'Windows' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') {
    throw 'Installer test requires an isolated GitHub-hosted Windows runner.'
}

$sourceBundle = (Resolve-Path 'build\VantaBeta_artefacts\Release\VST3\Vanta Beta 0.13.vst3').Path
$sourceApp = (Resolve-Path 'build\VantaBeta_artefacts\Release\Standalone\Vanta Beta 0.13.exe').Path
$setup = (Resolve-Path 'installer-output\Vanta-Beta-0.14-Setup.exe').Path
$standardRoot = Join-Path ([Environment]::GetFolderPath('CommonProgramFiles')) 'VST3'
$destination = Join-Path $env:RUNNER_TEMP 'Vanta-0.13-installer-test'
$customRoot = Join-Path $env:RUNNER_TEMP 'Vanta-custom-vst3-test'
$unscannedRoot = Join-Path $env:RUNNER_TEMP 'Vanta-unscanned-vst3-test'
$linkTarget = Join-Path $env:RUNNER_TEMP 'Vanta-junction-target-test'
$linkedRoot = Join-Path $env:RUNNER_TEMP 'Vanta-junction-root-test'
$outsideData = Join-Path $env:RUNNER_TEMP 'Vanta-projects-preservation-test'
$installedBundle = Join-Path $standardRoot 'Vanta Beta 0.13.vst3'
$installedApp = Join-Path $destination 'Vanta Beta 0.13.exe'
$shortcut = Join-Path ([Environment]::GetFolderPath('CommonPrograms')) 'Vanta\Vanta Beta 0.13.lnk'
$uninstallShortcut = Join-Path ([Environment]::GetFolderPath('CommonPrograms')) 'Vanta\Uninstall Vanta Beta 0.13.lnk'
$unrelated = Join-Path $standardRoot 'Other Synth - Vanta installer test.vst3'
$similarName = Join-Path $standardRoot 'Vanta Beta 0.6 not-a-vanta-plugin.vst3'
$registryKey = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{DD6ACECB-F464-4CE3-82E6-4040DBCBD598}_is1'
# Beta 0.12 kept its own uninstall entry. Setup and cleanup must preserve it.
$previousRegistryKey = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{86F8C91C-4DD4-467B-919A-8CD576ED4AA9}_is1'
$previousRegistryName = 'Vanta Beta 0.12 (installer-test fixture)'

function Bundle([string]$Root, [int]$Beta) { Join-Path $Root "Vanta Beta 0.$Beta.vst3" }
function Binary([string]$Root, [int]$Beta) {
    Join-Path (Bundle $Root $Beta) "Contents\x86_64-win\Vanta Beta 0.$Beta.vst3"
}
function Assert-Present([string]$Path, [string]$Reason) {
    if (!(Test-Path -LiteralPath $Path)) { throw "$Reason : missing $Path" }
}
function Assert-Absent([string]$Path, [string]$Reason) {
    if (Test-Path -LiteralPath $Path) { throw "$Reason : still exists $Path" }
}
function New-TestBundle([string]$Root, [int]$Beta, [string]$Extra = '') {
    $bundle = Bundle $Root $Beta
    $binary = Binary $Root $Beta
    New-Item -ItemType Directory -Path (Split-Path -Parent $binary) -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $bundle 'Contents\Resources') -Force | Out-Null
    [IO.File]::WriteAllText($binary, "test-only VANTA beta 0.$Beta module")
    [IO.File]::WriteAllText((Join-Path $bundle 'Contents\Resources\moduleinfo.json'), '{"test":true}')
    if ($Extra) {
        $extraPath = Join-Path $bundle $Extra
        New-Item -ItemType Directory -Path (Split-Path -Parent $extraPath) -Force | Out-Null
        [IO.File]::WriteAllText($extraPath, 'user data: never delete')
    }
}
function Start-Checked([string]$Executable, [string[]]$Arguments, [string]$Log, [int]$Expected = 0) {
    $process = Start-Process -FilePath $Executable -ArgumentList $Arguments -Wait -PassThru
    if ($process.ExitCode -ne $Expected) {
        if (Test-Path -LiteralPath $Log) { Get-Content -LiteralPath $Log }
        throw "Unexpected exit code $($process.ExitCode), expected $Expected, from $Executable"
    }
}
function Install-Test([string]$Name, [string[]]$More = @(), [int]$Expected = 0) {
    $log = Join-Path $env:RUNNER_TEMP "vanta-0.13-$Name.log"
    $args = @('/SP-', '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/DIR=`"$destination`"", "/LOG=`"$log`"") + $More
    Start-Checked $setup $args $log $Expected
    return $log
}
function Uninstall-Test([string]$Name, [string[]]$More = @()) {
    $uninstaller = Get-ChildItem -LiteralPath $destination -Filter 'unins*.exe' -File | Select-Object -First 1
    if (!$uninstaller) { throw 'The uninstaller was not created.' }
    $log = Join-Path $env:RUNNER_TEMP "vanta-0.13-$Name.log"
    $args = @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/LOG=`"$log`"") + $More
    Start-Checked $uninstaller.FullName $args $log
    return $log
}
function Remove-TestJunction([string]$Path) {
    if (Test-Path -LiteralPath $Path) {
        $item = Get-Item -LiteralPath $Path -Force
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            # Remove the junction itself; never recursively traverse its target.
            [IO.Directory]::Delete($Path)
        }
    }
}

$betaPaths = @(1..14 | ForEach-Object { Bundle $standardRoot $_ })
$guardPaths = @($destination, $customRoot, $unscannedRoot, $linkTarget, $linkedRoot, $outsideData,
    $shortcut, $uninstallShortcut, $unrelated, $similarName) + $betaPaths
foreach ($path in $guardPaths) {
    if (Test-Path -LiteralPath $path) { throw "Installer test path already exists: $path" }
}
if ($registryKey -eq $previousRegistryKey) { throw 'Each beta must have a separate uninstall AppId.' }
foreach ($key in @($registryKey, $previousRegistryKey)) {
    if (Test-Path -LiteralPath $key) { throw "Installer test registry key already exists: $key" }
}

$runningApp = $null
$lockedStream = $null
try {
    New-Item -Path $previousRegistryKey -Force | Out-Null
    New-ItemProperty -LiteralPath $previousRegistryKey -Name 'DisplayName' -Value $previousRegistryName | Out-Null
    New-Item -ItemType Directory -Path $standardRoot, $customRoot, $unscannedRoot, $outsideData -Force | Out-Null
    [IO.File]::WriteAllText((Bundle $standardRoot 1), 'flat test VST3')
    New-TestBundle $standardRoot 6 'Important Project.flp'
    New-TestBundle $standardRoot 8
    New-TestBundle $standardRoot 10
    New-TestBundle $standardRoot 11
    New-TestBundle $standardRoot 12
    New-TestBundle $customRoot 5 'Contents\User Preset.vanta'
    New-TestBundle $unscannedRoot 8 'Manual copy.txt'
    New-TestBundle $standardRoot 14 'future version.txt'
    [IO.File]::WriteAllText($unrelated, 'other plugin')
    [IO.File]::WriteAllText($similarName, 'not an exact beta name')
    $project = Join-Path $outsideData 'My Project.flp'
    $preset = Join-Path $outsideData 'My Preset.vanta'
    [IO.File]::WriteAllText($project, 'project outside the plugin folder')
    [IO.File]::WriteAllText($preset, 'preset outside the plugin folder')
    $oldStandalone = Join-Path $outsideData 'Vanta Beta 0.8.exe'
    [IO.File]::WriteAllText($oldStandalone, 'older standalone remains')

    # Without explicit permission, silent setup logs old plugins but keeps them.
    $log = Install-Test 'initial'
    if (!(Select-String -LiteralPath $log -SimpleMatch 'VANTA older plugin detected' -Quiet)) { throw 'Old plugin detection was not logged.' }
    foreach ($beta in @(1, 6, 8, 10, 11, 12)) { Assert-Present (Bundle $standardRoot $beta) 'Silent setup removed an old plugin without consent' }
    Assert-Present (Binary $customRoot 5) 'Custom folder was touched without being selected'

    $pluginBinary = Join-Path $installedBundle 'Contents\x86_64-win\Vanta Beta 0.13.vst3'
    Assert-Present $pluginBinary 'Installed x64 VST3 binary is missing'
    $files = @(Get-ChildItem -LiteralPath $sourceBundle -File -Recurse)
    if ($files.Count -eq 0) { throw 'The build produced an empty VST3 bundle.' }
    foreach ($file in $files) {
        $relative = [IO.Path]::GetRelativePath($sourceBundle, $file.FullName)
        $installedFile = Join-Path $installedBundle $relative
        Assert-Present $installedFile "Installed VST3 file $relative"
        if ((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $installedFile -Algorithm SHA256).Hash) {
            throw "Installed VST3 file differs from the build: $relative"
        }
    }
    Assert-Present $installedApp 'Standalone was not installed'
    Assert-Present (Join-Path $destination 'THIRD_PARTY_NOTICES.md') 'Third-party notices were not installed'
    if ((Get-FileHash -LiteralPath $sourceApp -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $installedApp -Algorithm SHA256).Hash) { throw 'Installed standalone differs from the build.' }
    Assert-Present $registryKey 'Windows Settings uninstall entry is missing'
    if ((Get-ItemPropertyValue -LiteralPath $previousRegistryKey -Name 'DisplayName') -ne $previousRegistryName) { throw 'Setup changed the Beta 0.12 uninstall entry.' }
    Assert-Present $shortcut 'Standalone Start Menu shortcut is missing'
    Assert-Present $uninstallShortcut 'Uninstaller Start Menu shortcut is missing'
    $shell = New-Object -ComObject WScript.Shell
    if ($shell.CreateShortcut($shortcut).TargetPath -ne $installedApp) { throw 'Standalone shortcut points to the wrong target.' }
    $uninstallerTarget = $shell.CreateShortcut($uninstallShortcut).TargetPath
    Assert-Present $uninstallerTarget 'Uninstaller shortcut target is missing'
    if ((Split-Path -Parent $uninstallerTarget) -ne $destination -or (Split-Path -Leaf $uninstallerTarget) -notlike 'unins*.exe') { throw "Uninstaller shortcut points to the wrong target: $uninstallerTarget" }

    # A known app file replaced by a directory is an unsafe/unexpected layout.
    # Both setup and uninstall must stop before changing the current or old
    # plugins, rather than overwriting or traversing the replacement.
    $notices = Join-Path $destination 'THIRD_PARTY_NOTICES.md'
    $heldNotices = Join-Path $outsideData 'held-third-party-notices.md'
    Move-Item -LiteralPath $notices -Destination $heldNotices
    New-Item -ItemType Directory -Path $notices | Out-Null
    $unsafeMarker = Join-Path $notices 'DO NOT TOUCH.flp'
    [IO.File]::WriteAllText($unsafeMarker, 'user data')
    try {
        $guardLog = Join-Path $env:RUNNER_TEMP 'vanta-0.13-guard-setup.log'
        $guardArgs = @('/SP-', '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/REMOVEOLDVANTA', "/DIR=`"$destination`"", "/LOG=`"$guardLog`"")
        $guardSetup = Start-Process -FilePath $setup -ArgumentList $guardArgs -Wait -PassThru
        if ($guardSetup.ExitCode -eq 0) { throw 'Setup accepted an unexpected current app file layout.' }
        $uninstallGuardLog = Join-Path $env:RUNNER_TEMP 'vanta-0.13-guard-uninstall.log'
        $guardUninstall = Start-Process -FilePath $uninstallerTarget -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/REMOVEOLDVANTA', "/LOG=`"$uninstallGuardLog`"") -Wait -PassThru
        if ($guardUninstall.ExitCode -eq 0) { throw 'Uninstaller accepted an unexpected current app file layout.' }
        foreach ($path in @($unsafeMarker, $pluginBinary, $installedApp, (Binary $standardRoot 8))) { Assert-Present $path 'Guard did not preserve an existing file' }
    } finally {
        if (Test-Path -LiteralPath $notices) { Remove-Item -LiteralPath $notices -Recurse -Force }
        if (Test-Path -LiteralPath $heldNotices) { Move-Item -LiteralPath $heldNotices -Destination $notices }
    }

    # The real build checks process/window startup; the fast packaging job uses
    # tiny test files instead and exercises the same installer and cleanup code.
    if (!$SkipLaunch) {
        $runningApp = Start-Process -FilePath $installedApp -PassThru
        $hasWindow = $false
        for ($i = 0; $i -lt 30; $i++) {
            Start-Sleep -Seconds 1
            $runningApp.Refresh()
            if ($runningApp.HasExited) { throw "Standalone exited during startup (code $($runningApp.ExitCode))." }
            if ($runningApp.MainWindowHandle -ne [IntPtr]::Zero) { $hasWindow = $true; break }
        }
        if (!$hasWindow) { throw 'Standalone did not create a main window within 30 seconds.' }
        Start-Sleep -Seconds 2
        $runningApp.Refresh()
        if ($runningApp.HasExited) { throw "Standalone exited after creating its window (code $($runningApp.ExitCode))." }
        if (!$runningApp.CloseMainWindow()) { throw 'Could not request the standalone window to close.' }
        if (!$runningApp.WaitForExit(10000)) { throw 'Standalone did not close in time.' }
        if ($runningApp.ExitCode -ne 0) { throw "Standalone exited with code $($runningApp.ExitCode)." }
        $runningApp = $null
    }

    $currentUserFile = Join-Path $installedBundle 'User project in current bundle.flp'
    [IO.File]::WriteAllText($currentUserFile, 'keep me during cleanup and uninstall')
    $customArg = "/VANTASCANDIR=`"$customRoot`""
    $log = Install-Test 'cleanup' @('/REMOVEOLDVANTA', $customArg)
    foreach ($beta in @(1, 8, 10, 11, 12)) { Assert-Absent (Bundle $standardRoot $beta) 'Old plugin copy should be removed' }
    Assert-Absent (Binary $standardRoot 6) 'Old 0.6 module should be removed'
    Assert-Present (Join-Path (Bundle $standardRoot 6) 'Important Project.flp') 'Project inside old bundle must be preserved'
    Assert-Absent (Binary $customRoot 5) 'Selected custom-folder plugin should be removed'
    Assert-Present (Join-Path (Bundle $customRoot 5) 'Contents\User Preset.vanta') 'Preset inside custom bundle must be preserved'
    foreach ($path in @($pluginBinary, $currentUserFile, $installedApp, (Binary $standardRoot 14), (Binary $unscannedRoot 8), $unrelated, $similarName, $project, $preset, $oldStandalone)) {
        Assert-Present $path 'Current, future, unrelated, unselected or user data was touched'
    }
    if (!(Select-String -LiteralPath $log -SimpleMatch 'VANTA cleanup preserved extra files' -Quiet)) { throw 'Preserved user data was not reported.' }

    # Exercise every historical name in both roots. The fixed candidate array
    # must be large enough for all 24 possible recognised copies.
    foreach ($beta in 1..12) {
        New-TestBundle $standardRoot $beta
        New-TestBundle $customRoot $beta
    }
    $null = Install-Test 'all-historical-copies' @('/REMOVEOLDVANTA', $customArg)
    foreach ($beta in 1..12) {
        Assert-Absent (Binary $standardRoot $beta) 'Standard old copy was not cleaned'
        Assert-Absent (Binary $customRoot $beta) 'Custom old copy was not cleaned'
    }
    Assert-Present $pluginBinary 'Current plugin was damaged by full cleanup'
    Assert-Present (Binary $standardRoot 14) 'Future plugin was damaged by full cleanup'

    # An in-use module must be reported as a failure, not silently destroyed.
    New-TestBundle $standardRoot 9
    $lockedPath = Binary $standardRoot 9
    $lockedStream = [IO.File]::Open($lockedPath, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    $log = Install-Test 'locked' @('/REMOVEOLDVANTA') 20
    Assert-Present $lockedPath 'Locked plugin was removed'
    Assert-Present $pluginBinary 'Current plugin was damaged by failed cleanup'
    if (!(Select-String -LiteralPath $log -SimpleMatch 'VANTA cleanup incomplete' -Quiet)) { throw 'Locked plugin failure was not reported.' }
    $lockedStream.Dispose(); $lockedStream = $null
    $null = Install-Test 'retry' @('/REMOVEOLDVANTA')
    Assert-Absent (Bundle $standardRoot 9) 'Unlocked plugin should be removable on retry'

    # Junctions at the bundle, contents and optional-root levels must not be
    # traversed. An unrecognised bundle with metadata must also be left alone.
    $targetArch = Join-Path $linkTarget 'Contents\x86_64-win'
    New-Item -ItemType Directory -Path $targetArch -Force | Out-Null
    foreach ($beta in @(4, 7)) { [IO.File]::WriteAllText((Join-Path $targetArch "Vanta Beta 0.$beta.vst3"), 'protected target') }
    [IO.File]::WriteAllText((Join-Path $linkTarget 'DO NOT DELETE.flp'), 'protected target')
    New-Item -ItemType Junction -Path (Bundle $standardRoot 7) -Target $linkTarget | Out-Null
    New-Item -ItemType Directory -Path (Bundle $standardRoot 4) -Force | Out-Null
    New-Item -ItemType Junction -Path (Join-Path (Bundle $standardRoot 4) 'Contents') -Target (Join-Path $linkTarget 'Contents') | Out-Null
    New-Item -ItemType Junction -Path $linkedRoot -Target $linkTarget | Out-Null
    $unknown = Bundle $standardRoot 2
    New-Item -ItemType Directory -Path (Join-Path $unknown 'Contents\Resources') -Force | Out-Null
    [IO.File]::WriteAllText((Join-Path $unknown 'Contents\Resources\moduleinfo.json'), 'unrecognised plugin metadata')
    [IO.File]::WriteAllText((Join-Path $unknown 'DO NOT DELETE.vanta'), 'unrecognised data')
    $log = Install-Test 'junctions' @('/REMOVEOLDVANTA', "/VANTASCANDIR=`"$linkedRoot`"") 20
    foreach ($path in @((Join-Path $targetArch 'Vanta Beta 0.4.vst3'), (Join-Path $targetArch 'Vanta Beta 0.7.vst3'), (Join-Path $linkTarget 'DO NOT DELETE.flp'), (Join-Path $unknown 'DO NOT DELETE.vanta'))) {
        Assert-Present $path 'A linked or unrecognised target was touched'
    }
    if (!(Select-String -LiteralPath $log -SimpleMatch 'VANTA cleanup incomplete' -Quiet)) { throw 'Unsafe paths were not reported.' }
    Remove-TestJunction (Bundle $standardRoot 7)
    Remove-TestJunction (Join-Path (Bundle $standardRoot 4) 'Contents')
    Remove-TestJunction $linkedRoot
    Remove-Item -LiteralPath (Bundle $standardRoot 4), $unknown -Recurse -Force

    # Uninstall normally: current app/plugin and shortcuts go away, but old
    # copies and user-added files are kept unless cleanup was explicitly opted in.
    New-TestBundle $standardRoot 8 'Saved Preset.vanta'
    $null = Uninstall-Test 'uninstall-keep-old'
    foreach ($path in @($pluginBinary, $installedApp, $shortcut, $uninstallShortcut, $registryKey)) { Assert-Absent $path 'Normal uninstall did not remove an installed item' }
    foreach ($path in @((Binary $standardRoot 8), $currentUserFile, (Binary $standardRoot 14), (Binary $unscannedRoot 8), $project, $preset, $oldStandalone)) { Assert-Present $path 'Normal uninstall touched retained data or an older plugin' }

    # Reinstall and explicitly ask the uninstaller to clean old copies, including
    # a separately selected custom VST3 folder.
    $null = Install-Test 'reinstall'
    New-TestBundle $customRoot 10 'User Project.flp'
    $null = Uninstall-Test 'uninstall-clean-old' @('/REMOVEOLDVANTA', $customArg)
    Assert-Absent $pluginBinary 'Uninstall must remove the current plugin'
    Assert-Absent (Binary $standardRoot 8) 'Explicit uninstall cleanup must remove old plugin'
    Assert-Absent (Binary $customRoot 10) 'Explicit uninstall cleanup must remove custom old plugin'
    foreach ($path in @((Join-Path (Bundle $standardRoot 8) 'Saved Preset.vanta'), (Join-Path (Bundle $customRoot 10) 'User Project.flp'), $currentUserFile, (Binary $standardRoot 14), (Binary $unscannedRoot 8), $project, $preset, $oldStandalone)) {
        Assert-Present $path 'Uninstall cleanup touched retained data'
    }
    foreach ($path in @($installedApp, $shortcut, $uninstallShortcut, $registryKey)) { Assert-Absent $path 'Final uninstall did not remove an installed item' }
    if ((Get-ItemPropertyValue -LiteralPath $previousRegistryKey -Name 'DisplayName') -ne $previousRegistryName) { throw 'Uninstall changed the Beta 0.12 uninstall entry.' }
    Write-Host 'Installer tests passed: consent, old/current/future versions, independent uninstall entries, custom paths, preservation, locked files, junctions, retries, reinstall and uninstall.'
}
finally {
    if ($null -ne $lockedStream) { $lockedStream.Dispose() }
    if ($null -ne $runningApp) {
        $runningApp.Refresh()
        if (!$runningApp.HasExited) { Stop-Process -Id $runningApp.Id -Force }
    }
    # Only paths verified absent at the start are cleaned, on a disposable VM.
    Remove-TestJunction (Bundle $standardRoot 7)
    Remove-TestJunction (Join-Path (Bundle $standardRoot 4) 'Contents')
    Remove-TestJunction $linkedRoot
    foreach ($path in $guardPaths) {
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Recurse -Force }
    }
    foreach ($key in @($registryKey, $previousRegistryKey)) {
        if (Test-Path -LiteralPath $key) { Remove-Item -LiteralPath $key -Recurse -Force }
    }
}
