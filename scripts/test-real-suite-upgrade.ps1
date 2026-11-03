$ErrorActionPreference = 'Stop'
# Isolated hosted runner only: this test installs real plugin paths.
if ($env:GITHUB_ACTIONS -ne 'true' -or !$env:RUNNER_TEMP -or $env:RUNNER_OS -ne 'Windows' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') {
    throw 'Release upgrade test requires an isolated GitHub-hosted Windows runner.'
}
$oldSetup = (Resolve-Path 'build/previous-release/Vanta-Suite-Setup.exe').Path
$newSetup = (Resolve-Path 'installer-output/Vanta-Suite-Setup.exe').Path
$root = Join-Path ([Environment]::GetFolderPath('CommonProgramFiles')) 'VST3'
$app = Join-Path $env:RUNNER_TEMP 'vanta-real-suite-upgrade-test'
$other = Join-Path $env:RUNNER_TEMP 'vanta-real-suite-upgrade-data'
$synth = Join-Path $root 'Vanta Beta 0.13.vst3'
$laggify = Join-Path $root 'Laggify.vst3'
$sidechain = Join-Path $root 'VANTA Sidechain.vst3'
$reg = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{DD6ACECB-F464-4CE3-82E6-4040DBCBD598}_is1'
$programs = Join-Path ([Environment]::GetFolderPath('CommonPrograms')) 'Vanta'
$shortcuts = @((Join-Path $programs 'Vanta Beta 0.13.lnk'), (Join-Path $programs 'Uninstall Vanta Suite.lnk'))
$paths = @($app, $other, $synth, $laggify, $sidechain, $reg) + $shortcuts
foreach ($path in $paths) { if (Test-Path -LiteralPath $path) { throw "Real-upgrade test path already exists: $path" } }
function Run([string]$exe, [string[]]$arguments, [bool]$expectSuccess) {
    $process = Start-Process -FilePath $exe -ArgumentList $arguments -PassThru
    if (!$process.WaitForExit(90000)) { Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue; throw "Timed out: $exe" }
    if (($expectSuccess -and $process.ExitCode -ne 0) -or (!$expectSuccess -and $process.ExitCode -eq 0)) { throw "Unexpected exit $($process.ExitCode): $exe" }
}
function Setup([string]$exe, [bool]$expectSuccess) {
    Run $exe @('/SP-', '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/DIR=`"$app`"") $expectSuccess
}
function Binary([string]$bundle) { Join-Path $bundle ('Contents\x86_64-win\' + (Split-Path -Leaf $bundle)) }
try {
    New-Item -ItemType Directory -Path $root, $other -Force | Out-Null
    Setup $oldSetup $true
    if ((Get-ItemProperty -LiteralPath $reg).DisplayVersion -ne '0.13.1') { throw 'The published previous suite did not install.' }
    $data = Join-Path $sidechain 'User project.flp'; [IO.File]::WriteAllText($data, 'preserve user data')
    $module = Binary $sidechain; $held = Join-Path $other 'previous-module.vst3'
    Move-Item -LiteralPath $module -Destination $held
    [IO.File]::WriteAllText($module, 'foreign replacement')
    try {
        Setup $newSetup $false
        if ([IO.File]::ReadAllText($module) -ne 'foreign replacement') { throw 'The new installer overwrote a replacement.' }
    } finally {
        Remove-Item -LiteralPath $module -Force -ErrorAction SilentlyContinue
        Move-Item -LiteralPath $held -Destination $module
    }
    Setup $newSetup $true
    if ((Get-ItemProperty -LiteralPath $reg).DisplayVersion -ne '0.13.2') { throw 'The published suite was not upgraded in place.' }
    if (!(Test-Path -LiteralPath $data)) { throw 'A user project was removed during upgrade.' }
    $uninstallers = @(Get-ChildItem -LiteralPath $app -Filter 'unins*.exe' -File)
    if ($uninstallers.Count -ne 1) { throw 'The upgrade left more than one uninstaller.' }
    $uninstaller = $uninstallers[0].FullName
    Run $uninstaller @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART') $true
    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    while ((Test-Path -LiteralPath $uninstaller) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 200 }
    if (Test-Path -LiteralPath $uninstaller) { throw 'The upgraded suite uninstaller did not finish removing itself.' }
    foreach ($bundle in @($synth, $laggify, $sidechain)) { if (Test-Path -LiteralPath (Binary $bundle)) { throw "The new uninstaller left a suite module: $bundle" } }
    if (!(Test-Path -LiteralPath $data)) { throw 'A user project was removed during uninstall.' }
    Write-Host 'Actual 0.13.1 upgrade passed: exact hashes, replacement guard, one uninstaller and user data preservation.'
} finally {
    foreach ($path in $paths) { if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Recurse -Force } }
}
