$ErrorActionPreference = 'Continue'
function Checked([string]$label, [string]$exe, [string[]]$arguments) {
    Write-Host "`n=== $label ==="
    $output = (& $exe @arguments 2>&1 | Out-String -Width 4096)
    $code = $LASTEXITCODE
    Write-Output $output
    if ($code -ne 0) {
        if ($output.Length -gt 40000) { $output = $output.Substring($output.Length - 40000) }
        $escaped = $output.Replace('%', '%25').Replace("`r", '%0D').Replace("`n", '%0A')
        Write-Output "::error title=$label failed::$escaped"
        exit $code
    }
}
Checked 'Configure VANTA' 'cmake' @('-S', '.', '-B', 'build', '-A', 'x64')
Checked 'Build VANTA and tests' 'cmake' @('--build','build','--config','Release','--target','VantaTests','VantaEffectsTests','VantaImporterTests','VantaEditorTests','VantaVST3Tests','VantaBeta_Standalone','--parallel','2')
Checked 'Test VANTA' 'ctest' @('--test-dir','build','-C','Release','--output-on-failure','--no-tests=error')
$juce = (Resolve-Path 'build\_deps\juce-src').Path
Checked 'Configure Laggify' 'cmake' @('-S','Laggify VST Effect','-B','build/laggify','-A','x64',"-DFETCHCONTENT_SOURCE_DIR_JUCE=$juce")
Checked 'Build Laggify and tests' 'cmake' @('--build','build/laggify','--config','Release','--target','LaggifyEngineTests','LaggifyProcessorTests','LaggifyVST3Tests','--parallel','2')
Checked 'Test Laggify' 'ctest' @('--test-dir','build/laggify','-C','Release','--output-on-failure','--no-tests=error')
Checked 'Configure Sidechain' 'cmake' @('-S','Sidechain VST Effect','-B','build/sidechain','-A','x64',"-DFETCHCONTENT_SOURCE_DIR_JUCE=$juce")
Checked 'Build Sidechain and tests' 'cmake' @('--build','build/sidechain','--config','Release','--target','SidechainEngineTests','SidechainProcessorTests','SidechainVST3Tests','--parallel','2')
Checked 'Test Sidechain' 'ctest' @('--test-dir','build/sidechain','-C','Release','--output-on-failure','--no-tests=error')
$iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
Checked 'Compile legacy VANTA fixture' $iscc @('installer.iss')
Checked 'Compile legacy Laggify fixture' $iscc @('Laggify VST Effect/installer/Laggify.iss')
Checked 'Compile VANTA Suite setup' $iscc @('suite-installer.iss')
