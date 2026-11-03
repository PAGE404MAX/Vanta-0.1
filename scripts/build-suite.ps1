param([Parameter(Mandatory=$true)][ValidateSet('vanta','laggify','sidechain','package')][string]$Part)
$ErrorActionPreference = 'Continue'
function Checked([string]$label, [string]$exe, [string[]]$arguments) {
    Write-Host "`n=== $label ==="
    $output = (& $exe @arguments 2>&1 | Out-String -Width 4096)
    $code = $LASTEXITCODE
    Write-Output $output
    if ($code -ne 0) {
        # GitHub truncates annotations at about 4096 characters; keep the tail
        # short enough that the actual compiler or test failure is visible.
        if ($output.Length -gt 2400) { $output = $output.Substring($output.Length - 2400) }
        $escaped = $output.Replace('%', '%25').Replace("`r", '%0D').Replace("`n", '%0A')
        Write-Output "::error title=$label failed::$escaped"
        exit $code
    }
}
switch ($Part) {
    'vanta' {
        Checked 'Configure VANTA' 'cmake' @('-S', '.', '-B', 'build', '-A', 'x64')
        Checked 'Build VANTA and tests' 'cmake' @('--build','build','--config','Release','--target','VantaTests','VantaEffectsTests','VantaImporterTests','VantaEditorTests','VantaVST3Tests','VantaBeta_Standalone','--parallel','2')
        Checked 'Test VANTA' 'ctest' @('--test-dir','build','-C','Release','--output-on-failure','--no-tests=error')
    }
    'laggify' {
        Checked 'Configure Laggify' 'cmake' @('-S','Laggify VST Effect','-B','build/laggify','-A','x64')
        Checked 'Build Laggify engine and editor tests' 'cmake' @('--build','build/laggify','--config','Release','--target','LaggifyEngineTests','LaggifyProcessorTests','--parallel','2')
        Checked 'Build Laggify VST3' 'cmake' @('--build','build/laggify','--config','Release','--target','LaggifyFx_VST3','--parallel','2')
        Checked 'Build Laggify VST3 host tests' 'cmake' @('--build','build/laggify','--config','Release','--target','LaggifyVST3Tests','--parallel','2')
        Checked 'Test Laggify' 'ctest' @('--test-dir','build/laggify','-C','Release','--output-on-failure','--no-tests=error')
    }
    'sidechain' {
        Checked 'Configure Sidechain' 'cmake' @('-S','Sidechain VST Effect','-B','build/sidechain','-A','x64')
        Checked 'Build Sidechain VST3' 'cmake' @('--build','build/sidechain','--config','Release','--target','VantaSidechainFx_VST3','--parallel','2')
        Checked 'Build Sidechain engine and editor tests' 'cmake' @('--build','build/sidechain','--config','Release','--target','SidechainEngineTests','SidechainProcessorTests','--parallel','2')
        Checked 'Build Sidechain VST3 host tests' 'cmake' @('--build','build/sidechain','--config','Release','--target','SidechainVST3Tests','--parallel','2')
        Checked 'Test Sidechain' 'ctest' @('--test-dir','build/sidechain','-C','Release','--output-on-failure','--no-tests=error')
    }
    'package' {
        # Do not publish an upgrade installer until every released 0.13.2
        # binary/manifest is verified against its exact published bytes.
        $source = Get-Content -LiteralPath 'suite-installer.iss' -Raw
        if ($source -match '(?m)^#define Legacy132\w+Hash\s+""') {
            Write-Output '::error title=Suite upgrade verification incomplete::Published 0.13.2 hashes must be verified before packaging.'
            exit 1
        }
        $iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
        Checked 'Compile legacy VANTA fixture' $iscc @('installer.iss')
        Checked 'Compile legacy Laggify fixture' $iscc @('Laggify VST Effect/installer/Laggify.iss')
        Checked 'Compile VANTA Suite setup' $iscc @('suite-installer.iss')
    }
}
