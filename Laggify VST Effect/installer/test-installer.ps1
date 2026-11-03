$ErrorActionPreference = 'Stop'
# The test installs into system VST3 folders: only run on disposable CI VMs.
if ($env:GITHUB_ACTIONS -ne 'true' -or !$env:RUNNER_TEMP -or $env:RUNNER_OS -ne 'Windows' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') {
    throw 'Installer test requires an isolated GitHub-hosted Windows runner.'
}
$source = (Resolve-Path 'build\laggify\LaggifyFx_artefacts\Release\VST3\Laggify.vst3').Path
$setup = (Resolve-Path 'installer-output\Laggify-0.2-Setup.exe').Path
$root = Join-Path ([Environment]::GetFolderPath('CommonProgramFiles')) 'VST3'
$bundle = Join-Path $root 'Laggify.vst3'
$destination = Join-Path $env:RUNNER_TEMP 'Laggify-0.2-installer-test'
$fixtureIss = 'Laggify VST Effect\installer\legacy-upgrade-test.iss'
$fixtureExe = 'installer-output\Laggify-0.1-upgrade-test.exe'
$outside = Join-Path $env:RUNNER_TEMP 'Laggify-preservation-test'
$unrelated = Join-Path $root 'Vanta unrelated plugin - Laggify test.vst3'
$registry = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{096E7B0E-1C14-4696-B9A7-6402B2571F91}_is1'
foreach ($path in @($bundle, $destination, $outside, $unrelated, $registry, $fixtureIss, $fixtureExe)) {
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
    # Build a 0.1-style installer fixture with the same AppId and paths. This
    # tests an in-place upgrade without depending on an expiring CI artifact.
    $iss = [IO.File]::ReadAllText((Resolve-Path 'Laggify VST Effect\installer\Laggify.iss').Path)
    if (!$iss.Contains('#define AppVersion "0.2"') -or !$iss.Contains('OutputBaseFilename=Laggify-0.2-Setup')) {
        throw 'Unable to construct the 0.1 upgrade fixture.'
    }
    $iss = $iss.Replace('#define AppVersion "0.2"', '#define AppVersion "0.1"').Replace(
        'OutputBaseFilename=Laggify-0.2-Setup', 'OutputBaseFilename=Laggify-0.1-upgrade-test')
    [IO.File]::WriteAllText((Join-Path (Get-Location) $fixtureIss), $iss, [Text.UTF8Encoding]::new($false))
    $iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
    Run-Checked $iscc @("`"$fixtureIss`"")
    $legacySetup = (Resolve-Path $fixtureExe).Path
    Run-Checked $legacySetup @('/SP-', '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/DIR=`"$destination`"")
    Assert-Present $registry
    if ((Get-ItemProperty -LiteralPath $registry).DisplayVersion -ne '0.1') { throw 'Legacy fixture did not register 0.1.' }
    $binary = Join-Path $bundle 'Contents\x86_64-win\Laggify.vst3'
    Assert-Present $binary
    [IO.File]::WriteAllText($binary, 'older installed binary fixture')
    $extra = Join-Path $bundle 'User data.flp'
    [IO.File]::WriteAllText($extra, 'preserve this user data')
    $installLog = Join-Path $outside 'install.log'
    Run-Checked $setup @('/SP-', '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/DIR=`"$destination`"", "/LOG=`"$installLog`"")
    if ((Get-ItemProperty -LiteralPath $registry).DisplayVersion -ne '0.2') { throw 'Upgrade did not register 0.2.' }
    Assert-Present $extra
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
    # Uninstall only tracks its own files; the extra user file survives both
    # the upgrade and uninstall even though it lives inside the plugin bundle.
    $uninstaller = Get-ChildItem -LiteralPath $destination -Filter 'unins*.exe' -File | Select-Object -First 1
    if (!$uninstaller) { throw 'Missing uninstaller.' }
    $uninstallLog = Join-Path $outside 'uninstall.log'
    Run-Checked $uninstaller.FullName @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/LOG=`"$uninstallLog`"")
    if (Test-Path -LiteralPath (Join-Path $bundle 'Contents\x86_64-win\Laggify.vst3')) { throw 'Uninstaller left the installed plugin binary.' }
    if (Test-Path -LiteralPath $registry) { throw 'Uninstall registry entry remains.' }
    Assert-Present $extra
    Assert-Present $unrelated
    Assert-Present $project
    Write-Host 'Laggify 0.1-style upgrade, installer, uninstaller, bundle hashes and preservation checks passed.'
} finally {
    # Every path was guarded before this disposable-runner test began.
    if (Test-Path -LiteralPath $fixtureIss) { Remove-Item -LiteralPath $fixtureIss -Force }
    if (Test-Path -LiteralPath $fixtureExe) { Remove-Item -LiteralPath $fixtureExe -Force }
    if (Test-Path -LiteralPath $bundle) { Remove-Item -LiteralPath $bundle -Recurse -Force }
    if (Test-Path -LiteralPath $destination) { Remove-Item -LiteralPath $destination -Recurse -Force }
    if (Test-Path -LiteralPath $outside) { Remove-Item -LiteralPath $outside -Recurse -Force }
    if (Test-Path -LiteralPath $unrelated) { Remove-Item -LiteralPath $unrelated -Force }
}
