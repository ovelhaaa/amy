param([Parameter(Mandatory=$true)][string]$BaselineRoot)
$ErrorActionPreference='Stop'
$repo=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$baselineRepo=(Resolve-Path -LiteralPath $BaselineRoot).Path
Push-Location $repo
try {
    $objects=@(Get-ChildItem build/smk_p4_amy/*.c.o | ForEach-Object FullName)
    if(-not $objects.Count) { throw 'Run smk-p4/tests/run_amy.ps1 first to build the pinned AMY objects' }
    $output='build/p404/retrigger_probe'
    New-Item -ItemType Directory -Force $output | Out-Null
    $flags=@('-std=c++17','-O2','-Wall','-Wextra','-Werror','-DAMY_NO_MINIAUDIO','-DAMY_SAMPLE_RATE=48000','-DAMY_BLOCK_SIZE=256','-DBLOCK_SIZE_BITS=8','-DAMY_PATCH_CHECKED_RESOURCE_ADMISSION=1','-Isrc')
    foreach($variant in @('integration','baseline')) {
        $runtimeRoot=if($variant -eq 'baseline') { Join-Path $baselineRepo 'smk-p4/components/amy_runtime' } else { 'smk-p4/components/amy_runtime' }
        & g++ @flags "-I$runtimeRoot" smk-p4/tests/render_retrigger_probe.cpp (Join-Path $runtimeRoot 'amy_runtime.cpp') @objects -lm -o "$output/$variant.exe"
        if($LASTEXITCODE -ne 0) { throw "Probe compile failed: $variant" }
        & "./$output/$variant.exe" $output $variant | Tee-Object "$output/${variant}_metrics.log"
        if($LASTEXITCODE -ne 0) { throw "Probe failed: $variant" }
    }
    foreach($patch in @(128,135)) {
        $a=(Get-FileHash "$output/baseline_patch_$patch.wav" -Algorithm SHA256).Hash
        $b=(Get-FileHash "$output/integration_patch_$patch.wav" -Algorithm SHA256).Hash
        if($a -ne $b) { throw "Offline PCM differs for patch $patch; investigate before claiming equivalence" }
        Write-Host "PASS: patch $patch offline PCM byte-identical; this is not physical/analog approval"
    }
} finally { Pop-Location }
