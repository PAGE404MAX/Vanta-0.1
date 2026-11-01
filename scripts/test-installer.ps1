$ErrorActionPreference = 'Stop'

$source = (Resolve-Path 'build\VantaBeta_artefacts\Release\VST3\Vanta Beta 0.4.vst3').Path
$setup = (Resolve-Path 'installer-output\Vanta-Beta-0.4-Setup.exe').Path
$destination = Join-Path $env:RUNNER_TEMP 'Vanta-0.4-installer-test'
$installLog = Join-Path $env:RUNNER_TEMP 'vanta-install.log'
$uninstallLog = Join-Path $env:RUNNER_TEMP 'vanta-uninstall.log'
if (Test-Path $destination) { throw "The installer test destination already exists: $destination" }

$arguments = @('/SP-', '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/DIR=`"$destination`"", "/LOG=`"$installLog`"")
$install = Start-Process -FilePath $setup -ArgumentList $arguments -Wait -PassThru
if ($install.ExitCode -ne 0) {
    if (Test-Path $installLog) { Get-Content $installLog }
    throw "Installer failed with exit code $($install.ExitCode)"
}

$installedBundle = Join-Path $destination 'Vanta Beta 0.4.vst3'
$pluginBinary = Join-Path $installedBundle 'Contents\x86_64-win\Vanta Beta 0.4.vst3'
if (!(Test-Path $pluginBinary -PathType Leaf)) { throw "Installed x64 VST3 binary is missing: $pluginBinary" }
$files = @(Get-ChildItem -LiteralPath $source -File -Recurse)
if ($files.Count -eq 0) { throw 'The build produced an empty VST3 bundle.' }
foreach ($file in $files) {
    $relative = [IO.Path]::GetRelativePath($source, $file.FullName)
    $installedFile = Join-Path $installedBundle $relative
    if (!(Test-Path -LiteralPath $installedFile -PathType Leaf)) { throw "Missing installed file: $relative" }
    if ((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $installedFile -Algorithm SHA256).Hash) {
        throw "Installed file differs from the build: $relative"
    }
}

$uninstaller = Get-ChildItem -LiteralPath $destination -Filter 'unins*.exe' -File | Select-Object -First 1
if (!$uninstaller) { throw 'The uninstaller was not created.' }
$uninstallArgs = @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/LOG=`"$uninstallLog`"")
$uninstall = Start-Process -FilePath $uninstaller.FullName -ArgumentList $uninstallArgs -Wait -PassThru
if ($uninstall.ExitCode -ne 0) {
    if (Test-Path $uninstallLog) { Get-Content $uninstallLog }
    throw "Uninstaller failed with exit code $($uninstall.ExitCode)"
}
if (Test-Path -LiteralPath $installedBundle) { throw 'The VST3 bundle remains after uninstall.' }
Write-Host 'Installer smoke test passed: VST3 files match and uninstall removes the bundle.'
