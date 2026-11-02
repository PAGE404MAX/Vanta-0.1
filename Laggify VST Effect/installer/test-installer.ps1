$ErrorActionPreference = 'Stop'
# The test installs into system VST3 folders: only run on disposable CI VMs.
if ($env:GITHUB_ACTIONS -ne 'true' -or !$env:RUNNER_TEMP -or $env:RUNNER_OS -ne 'Windows' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') {
    throw 'Installer test requires an isolated GitHub-hosted Windows runner.'
}
$source = (Resolve-Path 'build\laggify\LaggifyFx_artefacts\Release\VST3\Laggify.vst3').Path
$setup = (Resolve-Path 'installer-output\Laggify-0.1-Setup.exe').Path
$root = Join-Path ([Environment]::GetFolderPath('CommonProgramFiles')) 'VST3'
$bundle = Join-Path $root 'Laggify.vst3'
$destination = Join-Path $env:RUNNER_TEMP 'Laggify-0.1-installer-test'
$outside = Join-Path $env:RUNNER_TEMP 'Laggify-preservation-test'
$unrelated = Join-Path $root 'Vanta unrelated plugin - Laggify test.vst3'
$registry = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{096E7B0E-1C14-4696-B9A7-6402B2571F91}_is1'
foreach ($path in @($bundle, $destination, $outside, $unrelated, $registry)) {
    if (Test-Path -LiteralPath $path) { throw "Unsafe test: path already exists: $path" }
}
function Assert-Present([string]$path) {
    if (!(Test-Path -LiteralPath $path)) { throw "Missing expected file: $path" }
}
function Run-Checked([string]$exe, [string[]]$arguments) {
    $result = Start-Process -FilePath $exe -ArgumentList $arguments -Wait -PassThru
    if ($result.ExitCode -ne 0) { throw "Exit $($result.ExitCode) running $exe" }
}
try {
    New-Item -ItemType Directory -Path $root, $outside -Force | Out-Null
    [IO.File]::WriteAllText($unrelated, 'unrelated VANTA synth placeholder')
    $project = Join-Path $outside 'My Project.flp'
    [IO.File]::WriteAllText($project, 'user project')
    $installLog = Join-Path $outside 'install.log'
    Run-Checked $setup @('/SP-', '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/DIR=`"$destination`"", "/LOG=`"$installLog`"")
    $files = @(Get-ChildItem -LiteralPath $source -File -Recurse)
    if ($files.Count -eq 0) { throw 'The VST3 bundle is empty.' }
    foreach ($file in $files) {
        $relative = [IO.Path]::GetRelativePath($source, $file.FullName)
        $installed = Join-Path $bundle $relative
        Assert-Present $installed
        if ((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $installed -Algorithm SHA256).Hash) {
            throw "Installed VST3 file differs: $relative"
        }
    }
    Assert-Present (Join-Path $bundle 'Contents\x86_64-win\Laggify.vst3')
    Assert-Present (Join-Path $destination 'THIRD_PARTY_NOTICES.md')
    Assert-Present (Join-Path $destination 'third-party\JUCE-LICENSE.md')
    Assert-Present $registry
    Assert-Present $unrelated
    Assert-Present $project
    # Uninstall only tracks its own files; unrelated material is preserved even
    # if the user placed it within the plugin bundle.
    $extra = Join-Path $bundle 'User data.flp'
    [IO.File]::WriteAllText($extra, 'preserve this user data')
    $uninstaller = Get-ChildItem -LiteralPath $destination -Filter 'unins*.exe' -File | Select-Object -First 1
    if (!$uninstaller) { throw 'Missing uninstaller.' }
    $uninstallLog = Join-Path $outside 'uninstall.log'
    Run-Checked $uninstaller.FullName @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/LOG=`"$uninstallLog`"")
    if (Test-Path -LiteralPath (Join-Path $bundle 'Contents\x86_64-win\Laggify.vst3')) { throw 'Uninstaller left the installed plugin binary.' }
    if (Test-Path -LiteralPath $registry) { throw 'Uninstall registry entry remains.' }
    Assert-Present $extra
    Assert-Present $unrelated
    Assert-Present $project
    Write-Host 'Laggify installer, uninstaller, bundle hashes and preservation checks passed.'
} finally {
    # Every path was guarded before this disposable-runner test began.
    if (Test-Path -LiteralPath $bundle) { Remove-Item -LiteralPath $bundle -Recurse -Force }
    if (Test-Path -LiteralPath $destination) { Remove-Item -LiteralPath $destination -Recurse -Force }
    if (Test-Path -LiteralPath $outside) { Remove-Item -LiteralPath $outside -Recurse -Force }
    if (Test-Path -LiteralPath $unrelated) { Remove-Item -LiteralPath $unrelated -Force }
}
