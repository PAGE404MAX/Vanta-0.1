$ErrorActionPreference = 'Stop'

# This test installs into system VST3 and Start Menu folders and must only run
# on a disposable GitHub-hosted Windows runner, never a user's machine.
if ($env:GITHUB_ACTIONS -ne 'true' -or !$env:RUNNER_TEMP -or $env:RUNNER_OS -ne 'Windows' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') {
    throw 'Installer smoke test requires an isolated GitHub-hosted Windows runner.'
}
$sourceBundle = (Resolve-Path 'build\VantaBeta_artefacts\Release\VST3\Vanta Beta 0.6.vst3').Path
$sourceApp = (Resolve-Path 'build\VantaBeta_artefacts\Release\Standalone\Vanta Beta 0.6.exe').Path
$setup = (Resolve-Path 'installer-output\Vanta-Beta-0.6-Setup.exe').Path
$destination = Join-Path $env:RUNNER_TEMP 'Vanta-0.6-installer-test'
$installedBundle = Join-Path ([Environment]::GetFolderPath('CommonProgramFiles')) 'VST3\Vanta Beta 0.6.vst3'
$installedApp = Join-Path $destination 'Vanta Beta 0.6.exe'
$shortcut = Join-Path ([Environment]::GetFolderPath('CommonPrograms')) 'Vanta\Vanta Beta 0.6.lnk'
$installLog = Join-Path $env:RUNNER_TEMP 'vanta-install.log'
$uninstallLog = Join-Path $env:RUNNER_TEMP 'vanta-uninstall.log'
foreach ($path in @($destination, $installedBundle, $shortcut)) {
    if (Test-Path -LiteralPath $path) { throw "Installer test path already exists: $path" }
}

$arguments = @('/SP-', '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/DIR=`"$destination`"", "/LOG=`"$installLog`"")
$install = Start-Process -FilePath $setup -ArgumentList $arguments -Wait -PassThru
if ($install.ExitCode -ne 0) {
    if (Test-Path $installLog) { Get-Content $installLog }
    throw "Installer failed with exit code $($install.ExitCode)"
}

$runningApp = $null
try {
    $pluginBinary = Join-Path $installedBundle 'Contents\x86_64-win\Vanta Beta 0.6.vst3'
    if (!(Test-Path -LiteralPath $pluginBinary -PathType Leaf)) { throw "Installed x64 VST3 binary is missing: $pluginBinary" }
    $files = @(Get-ChildItem -LiteralPath $sourceBundle -File -Recurse)
    if ($files.Count -eq 0) { throw 'The build produced an empty VST3 bundle.' }
    foreach ($file in $files) {
        $relative = [IO.Path]::GetRelativePath($sourceBundle, $file.FullName)
        $installedFile = Join-Path $installedBundle $relative
        if (!(Test-Path -LiteralPath $installedFile -PathType Leaf)) { throw "Missing installed file: $relative" }
        if ((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $installedFile -Algorithm SHA256).Hash) {
            throw "Installed VST3 file differs from the build: $relative"
        }
    }
    if (!(Test-Path -LiteralPath $installedApp -PathType Leaf)) { throw 'The standalone application was not installed.' }
    if ((Get-FileHash -LiteralPath $sourceApp -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $installedApp -Algorithm SHA256).Hash) {
        throw 'Installed standalone application differs from the build.'
    }
    if (!(Test-Path -LiteralPath $shortcut -PathType Leaf)) { throw 'The Start Menu shortcut is missing.' }
    $shortcutTarget = (New-Object -ComObject WScript.Shell).CreateShortcut($shortcut).TargetPath
    if ($shortcutTarget -ne $installedApp) { throw "The Start Menu shortcut points to the wrong target: $shortcutTarget" }

    # This is a startup/window smoke test, not an audio-device or FL Studio test.
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
    if (!$runningApp.CloseMainWindow()) { throw 'Could not request that the standalone window close.' }
    if (!$runningApp.WaitForExit(10000)) { throw 'The standalone application did not close in time.' }
    if ($runningApp.ExitCode -ne 0) { throw "Standalone exited with code $($runningApp.ExitCode)." }
    $runningApp = $null

    $uninstaller = Get-ChildItem -LiteralPath $destination -Filter 'unins*.exe' -File | Select-Object -First 1
    if (!$uninstaller) { throw 'The uninstaller was not created.' }
    $uninstallArgs = @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/LOG=`"$uninstallLog`"")
    $uninstall = Start-Process -FilePath $uninstaller.FullName -ArgumentList $uninstallArgs -Wait -PassThru
    if ($uninstall.ExitCode -ne 0) {
        if (Test-Path $uninstallLog) { Get-Content $uninstallLog }
        throw "Uninstaller failed with exit code $($uninstall.ExitCode)"
    }
    foreach ($path in @($installedBundle, $installedApp, $shortcut)) {
        if (Test-Path -LiteralPath $path) { throw "File remains after uninstall: $path" }
    }
    Write-Host 'Installer smoke test passed: VST3 and standalone match, standalone launched, shortcut works, and uninstall removed both.'
}
finally {
    if ($null -ne $runningApp) {
        $runningApp.Refresh()
        if (!$runningApp.HasExited) { Stop-Process -Id $runningApp.Id -Force }
    }
}
