param([switch]$SkipLaunch)
$ErrorActionPreference = 'Stop'
# This test writes to the system VST3 and Start Menu folders. Never run it on
# a user's computer: it is strictly for disposable GitHub-hosted Windows VMs.
if ($env:GITHUB_ACTIONS -ne 'true' -or !$env:RUNNER_TEMP -or $env:RUNNER_OS -ne 'Windows' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') {
    throw 'Suite installer test requires an isolated GitHub-hosted Windows runner.'
}
$setup = (Resolve-Path 'installer-output\Vanta-Suite-Setup.exe').Path
$vantaSetup = (Resolve-Path 'installer-output\Vanta-Beta-0.13-Setup.exe').Path
$laggifySetup = (Resolve-Path 'installer-output\Laggify-0.2-Setup.exe').Path
$root = Join-Path ([Environment]::GetFolderPath('CommonProgramFiles')) 'VST3'
$app = Join-Path $env:RUNNER_TEMP 'vanta-suite-installer-test'
$legacyApp = Join-Path $env:RUNNER_TEMP 'vanta-suite-old-laggify-test'
$outside = Join-Path $env:RUNNER_TEMP 'vanta-suite-preservation-test'
$parentLink = Join-Path $env:RUNNER_TEMP 'vanta-suite-parent-junction-test'
$synth = Join-Path $root 'Vanta Beta 0.13.vst3'
$laggify = Join-Path $root 'Laggify.vst3'
$sidechain = Join-Path $root 'VANTA Sidechain.vst3'
$old = Join-Path $root 'Vanta Beta 0.8.vst3'
$other = Join-Path $root 'Unrelated instrument - suite test.vst3'
$synthReg = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{DD6ACECB-F464-4CE3-82E6-4040DBCBD598}_is1'
$laggifyReg = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{096E7B0E-1C14-4696-B9A7-6402B2571F91}_is1'
$previousReg = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{86F8C91C-4DD4-467B-919A-8CD576ED4AA9}_is1'
$programs = Join-Path ([Environment]::GetFolderPath('CommonPrograms')) 'Vanta'
$shortcuts = @((Join-Path $programs 'Vanta Beta 0.13.lnk'), (Join-Path $programs 'Uninstall Vanta Beta 0.13.lnk'), (Join-Path $programs 'Uninstall Vanta Suite.lnk'))
$paths = @($app, $legacyApp, $outside, $parentLink, $synth, $laggify, $sidechain, $old, $other, $synthReg, $laggifyReg, $previousReg) + $shortcuts
foreach ($path in $paths) { if (Test-Path -LiteralPath $path) { throw "Suite test path already exists: $path" } }
function Binary([string]$Bundle) { Join-Path $Bundle ("Contents\x86_64-win\" + (Split-Path -Leaf $Bundle)) }
function Present([string]$Path) { if (!(Test-Path -LiteralPath $Path)) { throw "Expected path is missing: $Path" } }
function Absent([string]$Path) { if (Test-Path -LiteralPath $Path) { throw "Expected path was not removed: $Path" } }
function Run([string]$exe, [string[]]$arguments, [bool]$expectSuccess = $true) {
    Write-Host "Running $exe $($arguments -join ' ')"
    $result = Start-Process -FilePath $exe -ArgumentList $arguments -PassThru
    if (!$result.WaitForExit(60000)) {
        Stop-Process -Id $result.Id -Force -ErrorAction SilentlyContinue
        throw "Timed out running $exe with arguments $($arguments -join ' ')"
    }
    if (($expectSuccess -and $result.ExitCode -ne 0) -or (!$expectSuccess -and $result.ExitCode -eq 0)) {
        throw "Unexpected exit $($result.ExitCode) from $exe; arguments $($arguments -join ' ')"
    }
}
function Setup([string]$exe, [string]$dir, [bool]$ok = $true, [string[]]$extra = @()) {
    Run $exe (@('/SP-', '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/DIR=`"$dir`"") + $extra) $ok
}
function Uninstaller([string]$dir) {
    $items = @(Get-ChildItem -LiteralPath $dir -Filter 'unins*.exe' -File)
    if ($items.Count -ne 1) { throw "Expected exactly one uninstaller in $dir; found $($items.Count)" }
    return $items[0].FullName
}
function Uninstall([string]$dir, [bool]$ok = $true, [string[]]$extra = @()) {
    Run (Uninstaller $dir) (@('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART') + $extra) $ok
}
function Remove-Junction([string]$path) {
    if (Test-Path -LiteralPath $path) {
        $item = Get-Item -LiteralPath $path -Force
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { [IO.Directory]::Delete($path) }
    }
}
function Compare-Bundle([string]$src, [string]$dest) {
    $items = @(Get-ChildItem -LiteralPath $src -File -Recurse)
    if ($items.Count -eq 0) { throw "Empty source plugin: $src" }
    foreach ($item in $items) {
        $relative = [IO.Path]::GetRelativePath((Resolve-Path $src).Path, $item.FullName)
        $target = Join-Path $dest $relative; Present $target
        if ((Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash) { throw "Wrong installed file: $target" }
    }
}
try {
    New-Item -ItemType Directory -Path $root, $outside -Force | Out-Null
    New-Item -Path $previousReg -Force | Out-Null
    New-ItemProperty -LiteralPath $previousReg -Name DisplayName -Value 'Preserve independent VANTA 0.12 entry' | Out-Null
    $oldBinary = Binary $old; New-Item -ItemType Directory -Path (Split-Path -Parent $oldBinary) -Force | Out-Null
    [IO.File]::WriteAllText($oldBinary, 'older VANTA test plugin')
    $oldPreset = Join-Path $old 'project to preserve.flp'; [IO.File]::WriteAllText($oldPreset, 'user project in old bundle')
    [IO.File]::WriteAllText($other, 'unrelated plugin')
    $project = Join-Path $outside 'My Project.flp'; $preset = Join-Path $outside 'My Sound.vanta'
    [IO.File]::WriteAllText($project, 'user project'); [IO.File]::WriteAllText($preset, 'user preset')

    # Reproduce both independently installed products. The suite must NOT take
    # over Laggify while that product's old uninstaller owns the same VST3.
    Setup $vantaSetup $app; Setup $laggifySetup $legacyApp
    foreach ($path in @((Binary $synth), (Binary $laggify), $synthReg, $laggifyReg, $oldBinary)) { Present $path }
    $oldLaggifyHash = (Get-FileHash -LiteralPath (Binary $laggify)).Hash
    Setup $setup $app $false
    if ((Get-FileHash -LiteralPath (Binary $laggify)).Hash -ne $oldLaggifyHash) { throw 'Suite overwrote Laggify while its old uninstaller still owned it.' }
    foreach ($path in @((Binary $synth), (Binary $laggify), $synthReg, $laggifyReg, $oldBinary)) { Present $path }
    Absent (Binary $sidechain)
    Uninstall $legacyApp; Absent $laggifyReg; Absent (Binary $laggify)

    # A manually-installed module with the same name is not ours to overwrite.
    $manual = Binary $sidechain; New-Item -ItemType Directory -Path (Split-Path -Parent $manual) -Force | Out-Null
    [IO.File]::WriteAllText($manual, 'unmanaged effect - do not overwrite')
    Setup $setup $app $false
    if ([IO.File]::ReadAllText($manual) -ne 'unmanaged effect - do not overwrite') { throw 'An unmanaged effect was overwritten.' }
    Remove-Item -LiteralPath $sidechain -Recurse -Force

    # A final path or any ancestor that is a junction must stop setup.
    $linkTarget = Join-Path $outside 'link target'; New-Item -ItemType Directory -Path $linkTarget -Force | Out-Null
    $protected = Join-Path $linkTarget 'DO NOT TOUCH.flp'; [IO.File]::WriteAllText($protected, 'protected')
    New-Item -ItemType Junction -Path $sidechain -Target $linkTarget | Out-Null
    Setup $setup $app $false; Present $protected; Present (Binary $synth); Remove-Junction $sidechain
    New-Item -ItemType Junction -Path $parentLink -Target $linkTarget | Out-Null
    Setup $setup (Join-Path $parentLink 'nested-app') $false
    Present $protected; Absent (Join-Path $linkTarget 'nested-app\Vanta Beta 0.13.exe'); Remove-Junction $parentLink

    # Explicit in-place upgrade of the exact 0.13 app: one registry entry and
    # one uninstaller own all three. Silent setup keeps older copies by default.
    Setup $setup $app
    foreach ($path in @((Binary $synth), (Binary $laggify), (Binary $sidechain), $synthReg, $oldBinary, $other, $project, $preset, $previousReg)) { Present $path }
    Absent $laggifyReg
    if ((Get-ItemProperty -LiteralPath $synthReg).DisplayName -notmatch 'VANTA Suite') { throw 'The suite did not replace the VANTA 0.13 uninstall entry.' }
    $suiteUninstall = Uninstaller $app
    $shell = New-Object -ComObject WScript.Shell
    if ($shell.CreateShortcut($shortcuts[2]).TargetPath -ne $suiteUninstall) { throw 'Suite uninstaller shortcut points elsewhere.' }
    Compare-Bundle 'build\VantaBeta_artefacts\Release\VST3\Vanta Beta 0.13.vst3' $synth
    Compare-Bundle 'build\laggify\LaggifyFx_artefacts\Release\VST3\Laggify.vst3' $laggify
    Compare-Bundle 'build\sidechain\VantaSidechainFx_artefacts\Release\VST3\VANTA Sidechain.vst3' $sidechain
    if ((Get-FileHash -LiteralPath 'build\VantaBeta_artefacts\Release\Standalone\Vanta Beta 0.13.exe').Hash -ne (Get-FileHash -LiteralPath (Join-Path $app 'Vanta Beta 0.13.exe')).Hash) { throw 'Standalone differs from build.' }
    $userFiles = @((Join-Path $synth 'User synth project.flp'), (Join-Path $laggify 'User laggify project.flp'), (Join-Path $sidechain 'User sidechain preset.vanta'))
    foreach ($f in $userFiles) { [IO.File]::WriteAllText($f, 'preserve me') }
    Setup $setup $app
    foreach ($path in $userFiles) { Present $path }

    # If an old standalone installer was run after the suite, neither suite
    # setup nor suite uninstall may silently take over/delete that plugin.
    New-Item -Path $laggifyReg -Force | Out-Null
    Setup $setup $app $false; Uninstall $app $false
    foreach ($path in @((Binary $synth), (Binary $laggify), (Binary $sidechain))) { Present $path }
    Remove-Item -LiteralPath $laggifyReg -Recurse -Force

    # A changed path after installation must stop both reinstall and uninstall.
    $contents = Join-Path $sidechain 'Contents'; $heldContents = Join-Path $outside 'held-sidechain-contents'
    Move-Item -LiteralPath $contents -Destination $heldContents
    New-Item -ItemType Junction -Path $contents -Target $linkTarget | Out-Null
    try {
        Setup $setup $app $false; Uninstall $app $false
        foreach ($path in @($protected, (Binary $synth), (Binary $laggify), $project, $preset)) { Present $path }
    } finally { Remove-Junction $contents; if (Test-Path -LiteralPath $heldContents) { Move-Item -LiteralPath $heldContents -Destination $contents } }

    if (!$SkipLaunch) {
        $proc = Start-Process -FilePath (Join-Path $app 'Vanta Beta 0.13.exe') -PassThru
        try {
            $opened = $false
            for ($i=0; $i -lt 30; $i++) { Start-Sleep -Seconds 1; $proc.Refresh(); if ($proc.HasExited) { throw 'Standalone exited during startup.' }; if ($proc.MainWindowHandle -ne [IntPtr]::Zero) { $opened=$true; break } }
            if (!$opened) { throw 'Standalone window did not appear.' }
            if (!$proc.CloseMainWindow() -or !$proc.WaitForExit(10000) -or $proc.ExitCode -ne 0) { throw 'Standalone did not close cleanly.' }
        } finally { $proc.Refresh(); if (!$proc.HasExited) { Stop-Process -Id $proc.Id -Force } }
    }
    Uninstall $app
    foreach ($path in @((Binary $synth), (Binary $laggify), (Binary $sidechain), $synthReg, $shortcuts[0], $shortcuts[2])) { Absent $path }
    foreach ($path in ($userFiles + @($oldBinary, $oldPreset, $other, $project, $preset, $previousReg))) { Present $path }

    # Reinstall over data-only folders left by uninstall, then explicitly opt
    # into recognised old VANTA plugin cleanup during suite uninstall.
    Setup $setup $app
    foreach ($path in $userFiles) { Present $path }
    Uninstall $app $true @('/REMOVEOLDVANTA')
    foreach ($path in @((Binary $synth), (Binary $laggify), (Binary $sidechain), $oldBinary, $synthReg)) { Absent $path }
    foreach ($path in ($userFiles + @($oldPreset, $other, $project, $preset, $previousReg))) { Present $path }
    Write-Host 'Suite tests passed: legacy migration guard, exact in-place upgrade, unmanaged collision, junctions/ancestors, three bundle hashes, repair, opt-out/opt-in and preserved user data.'
} finally {
    Remove-Junction (Join-Path $sidechain 'Contents'); Remove-Junction $sidechain; Remove-Junction $parentLink
    foreach ($path in $paths) { if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Recurse -Force } }
}
