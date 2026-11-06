param([switch]$SkipLaunch, [switch]$TestLegacyFixture, [switch]$TestPublished132)
$ErrorActionPreference = 'Stop'
# This test writes to the system VST3 and Start Menu folders. Never run it on
# a user's computer: it is strictly for disposable GitHub-hosted Windows VMs.
if ($env:GITHUB_ACTIONS -ne 'true' -or !$env:RUNNER_TEMP -or $env:RUNNER_OS -ne 'Windows' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') {
    throw 'Suite installer test requires an isolated GitHub-hosted Windows runner.'
}
$setup = (Resolve-Path 'installer-output\Vanta-Suite-Setup.exe').Path
$vantaSetup = (Resolve-Path 'installer-output\Vanta-Beta-0.14-Setup.exe').Path
$laggifySetup = (Resolve-Path 'installer-output\Laggify-0.2-Setup.exe').Path
$root = Join-Path ([Environment]::GetFolderPath('CommonProgramFiles')) 'VST3'
$app = Join-Path $env:RUNNER_TEMP 'vanta-suite-installer-test'
$legacyApp = Join-Path $env:RUNNER_TEMP 'vanta-suite-old-laggify-test'
$outside = Join-Path $env:RUNNER_TEMP 'vanta-suite-preservation-test'
$parentLink = Join-Path $env:RUNNER_TEMP 'vanta-suite-parent-junction-test'
$synth = Join-Path $root 'Vanta Beta 0.13.vst3'
$laggify = Join-Path $root 'Laggify.vst3'
$sidechain = Join-Path $root 'VANTA Sidechain.vst3'
$fx = Join-Path $root 'VANTA FX.vst3'
$old = Join-Path $root 'Vanta Beta 0.8.vst3'
$other = Join-Path $root 'Unrelated instrument - suite test.vst3'
$synthReg = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{DD6ACECB-F464-4CE3-82E6-4040DBCBD598}_is1'
$laggifyReg = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{096E7B0E-1C14-4696-B9A7-6402B2571F91}_is1'
$previousReg = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{86F8C91C-4DD4-467B-919A-8CD576ED4AA9}_is1'
$programs = Join-Path ([Environment]::GetFolderPath('CommonPrograms')) 'Vanta'
$shortcuts = @((Join-Path $programs 'Vanta Beta 0.13.lnk'), (Join-Path $programs 'Uninstall Vanta Beta 0.13.lnk'), (Join-Path $programs 'Uninstall Vanta Suite.lnk'))
$paths = @($app, $legacyApp, $outside, $parentLink, $synth, $laggify, $sidechain, $fx, $old, $other, $synthReg, $laggifyReg, $previousReg) + $shortcuts
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
    $executable = Uninstaller $dir
    Run $executable (@('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART') + $extra) $ok
    if ($ok) {
        # Inno's temporary child removes the original uninstaller after the
        # parent exits. Don't race that cleanup with an immediate reinstall.
        $deadline = [DateTime]::UtcNow.AddSeconds(20)
        while ((Test-Path -LiteralPath $executable) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 200 }
        if (Test-Path -LiteralPath $executable) { throw "Uninstaller did not finish removing itself: $executable" }
    }
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

    # Even an ancestor of a new app folder may not be a junction.
    $linkTarget = Join-Path $outside 'link target'; New-Item -ItemType Directory -Path $linkTarget -Force | Out-Null
    $protected = Join-Path $linkTarget 'DO NOT TOUCH.flp'; [IO.File]::WriteAllText($protected, 'protected')
    New-Item -ItemType Junction -Path $parentLink -Target $linkTarget | Out-Null
    Setup $setup (Join-Path $parentLink 'nested-app') $false
    Present $protected; Absent (Join-Path $linkTarget 'nested-app\Vanta Beta 0.13.exe'); Remove-Junction $parentLink

    # A fresh install folder may already contain unrelated, suite-named files.
    New-Item -ItemType Directory -Path $app -Force | Out-Null
    $unownedNotice = Join-Path $app 'THIRD_PARTY_NOTICES.md'
    [IO.File]::WriteAllText($unownedNotice, 'unrelated notice - preserve')
    Setup $setup $app $false
    if ([IO.File]::ReadAllText($unownedNotice) -ne 'unrelated notice - preserve') { throw 'An unowned notice was overwritten.' }
    Remove-Item -LiteralPath $app -Recurse -Force

    # An unmanaged current-version synth is also not suite-owned.
    $manualSynth = Binary $synth; New-Item -ItemType Directory -Path (Split-Path -Parent $manualSynth) -Force | Out-Null
    [IO.File]::WriteAllText($manualSynth, 'manual VANTA - preserve')
    Setup $setup $app $false
    if ([IO.File]::ReadAllText($manualSynth) -ne 'manual VANTA - preserve') { throw 'An unmanaged VANTA was overwritten.' }
    Remove-Item -LiteralPath $synth -Recurse -Force

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

    # The old 0.13 uninstaller must be replaced in its existing app folder.
    # A different /DIR must not create an orphan uninstaller with shared files.
    $differentApp = Join-Path $outside 'different app location'
    Setup $setup $differentApp $false; Absent (Join-Path $differentApp 'Vanta Beta 0.13.exe')
    Present (Uninstaller $app); Present (Binary $synth); Present $synthReg

    # A manually-installed module with the same name is not ours to overwrite.
    $manual = Binary $sidechain; New-Item -ItemType Directory -Path (Split-Path -Parent $manual) -Force | Out-Null
    [IO.File]::WriteAllText($manual, 'unmanaged effect - do not overwrite')
    Setup $setup $app $false
    if ([IO.File]::ReadAllText($manual) -ne 'unmanaged effect - do not overwrite') { throw 'An unmanaged effect was overwritten.' }
    Remove-Item -LiteralPath $sidechain -Recurse -Force

    # A final path or any ancestor that is a junction must stop setup.
    New-Item -ItemType Junction -Path $sidechain -Target $linkTarget | Out-Null
    Setup $setup $app $false; Present $protected; Present (Binary $synth); Remove-Junction $sidechain

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
    Present (Binary $fx)
    Compare-Bundle 'build\fx\VantaFX_artefacts\Release\VST3\VANTA FX.vst3' $fx
    Compare-Bundle 'build\sidechain\VantaSidechainFx_artefacts\Release\VST3\VANTA Sidechain.vst3' $sidechain
    if ((Get-FileHash -LiteralPath 'build\VantaBeta_artefacts\Release\Standalone\Vanta Beta 0.13.exe').Hash -ne (Get-FileHash -LiteralPath (Join-Path $app 'Vanta Beta 0.13.exe')).Hash) { throw 'Standalone differs from build.' }
    $userFiles = @((Join-Path $synth 'User synth project.flp'), (Join-Path $laggify 'User laggify project.flp'), (Join-Path $sidechain 'User sidechain preset.vanta'))
    foreach ($f in $userFiles) { [IO.File]::WriteAllText($f, 'preserve me') }
    Setup $setup $app
    foreach ($path in $userFiles) { Present $path }

    # The newer VANTA-only installer shares the AppId. It must not take over
    # the suite's uninstaller or replace a synth owned by the three-plugin suite,
    # even if someone selects a different target directory.
    $suiteHashes = @((Binary $synth), (Binary $laggify), (Binary $sidechain), $suiteUninstall) | ForEach-Object { (Get-FileHash -LiteralPath $_).Hash }
    Setup $vantaSetup $app $false
    Setup $vantaSetup (Join-Path $outside 'different vanta-only app') $false
    $afterHashes = @((Binary $synth), (Binary $laggify), (Binary $sidechain), $suiteUninstall) | ForEach-Object { (Get-FileHash -LiteralPath $_).Hash }
    for ($n = 0; $n -lt $suiteHashes.Count; ++$n) {
        if ($suiteHashes[$n] -ne $afterHashes[$n]) { throw 'VANTA-only setup modified a suite-owned file.' }
    }
    if ((Get-ItemProperty -LiteralPath $synthReg).DisplayName -notmatch 'VANTA Suite') { throw 'VANTA-only setup replaced the suite registration.' }

    # If an old standalone installer was run after the suite, neither suite
    # setup nor suite uninstall may silently take over/delete that plugin.
    New-Item -Path $laggifyReg -Force | Out-Null
    Setup $setup $app $false; Uninstall $app $false
    foreach ($path in @((Binary $synth), (Binary $laggify), (Binary $sidechain))) { Present $path }
    Remove-Item -LiteralPath $laggifyReg -Recurse -Force

    # A different plugin at an owned path is not ours to overwrite/delete.
    $changedBinary = Binary $sidechain; $heldBinary = Join-Path $outside 'held-real-sidechain.vst3'
    Move-Item -LiteralPath $changedBinary -Destination $heldBinary
    [IO.File]::WriteAllText($changedBinary, 'foreign replacement - preserve')
    try {
        Setup $setup $app $false; Uninstall $app $false
        if ([IO.File]::ReadAllText($changedBinary) -ne 'foreign replacement - preserve') { throw 'A replacement module was touched.' }
        foreach ($path in @((Binary $synth), (Binary $laggify), $project, $preset)) { Present $path }
    } finally {
        if (Test-Path -LiteralPath $changedBinary) { Move-Item -LiteralPath $changedBinary -Destination (Join-Path $outside 'preserved-replacement.vst3') }
        if (Test-Path -LiteralPath $heldBinary) { Move-Item -LiteralPath $heldBinary -Destination $changedBinary }
    }
    Present (Join-Path $outside 'preserved-replacement.vst3')

    # Markers and installed documentation are files too: a user's replacement
    # at one of these paths must not be overwritten or removed silently.
    foreach ($name in @('Vanta-suite-installed.txt', 'Sidechain-README.md')) {
        $file = Join-Path $app $name
        $originalBytes = [IO.File]::ReadAllBytes($file)
        [IO.File]::WriteAllText($file, 'user replacement - preserve')
        try {
            Setup $setup $app $false; Uninstall $app $false
            if ([IO.File]::ReadAllText($file) -ne 'user replacement - preserve') { throw "A replacement $name was touched." }
        } finally { [IO.File]::WriteAllBytes($file, $originalBytes) }
    }

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
    foreach ($path in @((Binary $synth), (Binary $laggify), (Binary $sidechain), (Binary $fx), $synthReg, $shortcuts[0], $shortcuts[2])) { Absent $path }
    foreach ($path in ($userFiles + @($oldBinary, $oldPreset, $other, $project, $preset, $previousReg))) { Present $path }

    # Reinstall over data-only folders left by uninstall, then explicitly opt
    # into recognised old VANTA plugin cleanup during suite uninstall.
    Setup $setup $app
    foreach ($path in $userFiles) { Present $path }
    Uninstall $app $true @('/REMOVEOLDVANTA')
    foreach ($path in @((Binary $synth), (Binary $laggify), (Binary $sidechain), $oldBinary, $synthReg)) { Absent $path }
    foreach ($path in ($userFiles + @($oldPreset, $other, $project, $preset, $previousReg))) { Present $path }
    if ($TestLegacyFixture) {
        # The old suite fixture has genuinely different binaries and manifests.
        # An upgrade must accept that known build, but not a foreign replacement.
        $legacySuite = (Resolve-Path 'installer-output\Legacy-Suite-0.13.1-Fixture.exe').Path
        Setup $legacySuite $app
        if ((Get-ItemProperty -LiteralPath $synthReg).DisplayVersion -ne '0.13.1') { throw 'Legacy suite fixture has the wrong registered version.' }
        $oldSideHash = (Get-FileHash -LiteralPath (Binary $sidechain)).Hash
        $newSideHash = (Get-FileHash -LiteralPath 'build\sidechain\VantaSidechainFx_artefacts\Release\VST3\VANTA Sidechain.vst3\Contents\x86_64-win\VANTA Sidechain.vst3').Hash
        if ($oldSideHash -eq $newSideHash) { throw 'Legacy and current fixture must have different bytes.' }
        $oldModule = Binary $sidechain; $heldModule = Join-Path $outside 'held-legacy-sidechain.vst3'
        Move-Item -LiteralPath $oldModule -Destination $heldModule
        [IO.File]::WriteAllText($oldModule, 'foreign file, not a legacy release')
        try {
            Setup $setup $app $false
            if ([IO.File]::ReadAllText($oldModule) -ne 'foreign file, not a legacy release') { throw 'Legacy upgrade overwrote an unrelated file.' }
        } finally {
            Remove-Item -LiteralPath $oldModule -Force -ErrorAction SilentlyContinue
            Move-Item -LiteralPath $heldModule -Destination $oldModule
        }
        Setup $setup $app
        if ((Get-ItemProperty -LiteralPath $synthReg).DisplayVersion -ne '0.14.0') { throw 'The previous suite was not upgraded in place.' }
        if ((Get-FileHash -LiteralPath (Binary $sidechain)).Hash -ne $newSideHash) { throw 'The upgraded Sidechain module did not match the current build.' }
        Present (Uninstaller $app)
        foreach ($path in ($userFiles + @($oldPreset, $other, $project, $preset, $previousReg))) { Present $path }
        Uninstall $app
        foreach ($path in @((Binary $synth), (Binary $laggify), (Binary $sidechain), $synthReg)) { Absent $path }
        foreach ($path in ($userFiles + @($oldPreset, $other, $project, $preset, $previousReg))) { Present $path }
    }
    if ($TestPublished132) {
        # The real published Suite 0.13.2 (Actions run 41580072836) must
        # upgrade in place, but only while its exact verified bytes remain.
        $publishedItems = @(Get-ChildItem -LiteralPath 'old-suite' -Filter '*.exe' -File -ErrorAction SilentlyContinue)
        if ($publishedItems.Count -ne 1) { throw "Expected exactly one published 0.13.2 installer in old-suite; found $($publishedItems.Count)" }
        $publishedSuite = $publishedItems[0].FullName
        Setup $publishedSuite $app
        if ((Get-ItemProperty -LiteralPath $synthReg).DisplayVersion -ne '0.13.2') { throw 'Published suite has the wrong registered version.' }
        foreach ($path in @((Binary $synth), (Binary $laggify), (Binary $sidechain))) { Present $path }
        $publishedSideHash = (Get-FileHash -LiteralPath (Binary $sidechain)).Hash
        $newSideHash = (Get-FileHash -LiteralPath 'build\sidechain\VantaSidechainFx_artefacts\Release\VST3\VANTA Sidechain.vst3\Contents\x86_64-win\VANTA Sidechain.vst3').Hash
        if ($publishedSideHash -eq $newSideHash) { throw 'Published and current Sidechain must have different bytes.' }
        $oldModule = Binary $sidechain; $heldModule = Join-Path $outside 'held-published-sidechain.vst3'
        Move-Item -LiteralPath $oldModule -Destination $heldModule
        [IO.File]::WriteAllText($oldModule, 'foreign file, not the published release')
        try {
            Setup $setup $app $false
            if ([IO.File]::ReadAllText($oldModule) -ne 'foreign file, not the published release') { throw 'Published upgrade overwrote an unrelated file.' }
        } finally {
            Remove-Item -LiteralPath $oldModule -Force -ErrorAction SilentlyContinue
            Move-Item -LiteralPath $heldModule -Destination $oldModule
        }
        Setup $setup $app
        if ((Get-ItemProperty -LiteralPath $synthReg).DisplayVersion -ne '0.14.0') { throw 'The published 0.13.2 suite was not upgraded in place.' }
        if ((Get-FileHash -LiteralPath (Binary $sidechain)).Hash -ne $newSideHash) { throw 'The upgraded Sidechain module did not match the current build.' }
        Present (Uninstaller $app)
        foreach ($path in ($userFiles + @($oldPreset, $other, $project, $preset, $previousReg))) { Present $path }
        Uninstall $app
        foreach ($path in @((Binary $synth), (Binary $laggify), (Binary $sidechain), $synthReg)) { Absent $path }
        foreach ($path in ($userFiles + @($oldPreset, $other, $project, $preset, $previousReg))) { Present $path }
    }
    Write-Host 'Suite tests passed: previous-release migration, unowned replacement guard, junctions, repairs, cleanup consent and preserved user data.'
} finally {
    Remove-Junction (Join-Path $sidechain 'Contents'); Remove-Junction $sidechain; Remove-Junction $parentLink
    foreach ($path in $paths) { if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Recurse -Force } }
}
