param([switch]$Observe,[switch]$StressOnly,[switch]$TimingOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $repo
try {
    New-Item -ItemType Directory -Force build/factory010 | Out-Null
    $objects=Get-ChildItem build/harmony09/*.c.o | ForEach-Object FullName
    if(!$objects){throw 'Run run_harmony.ps1 first (same AMY compile definitions)'}
    $flags=@('-O2','-DAMY_NO_MINIAUDIO','-DAMY_SAMPLE_RATE=48000','-DAMY_BLOCK_SIZE=256','-DBLOCK_SIZE_BITS=8','-DAMY_PATCH_CHECKED_RESOURCE_ADMISSION=1','-DAMY_EXCLUSIVE_FM_PHASE_RESTART=1','-Isrc','-Ismk-p4/components/amy_runtime')
    $out=if($TimingOnly){'timing'}elseif($Observe){'observed'}elseif($StressOnly){'stress'}else{'probe'}
    $selected=@()
    if($TimingOnly){
        $flags+='-DFACTORY_TIMING_ONLY=1'
        $metrics=Get-Content smk-p4/docs/factory_library_010_metrics.json -Raw | ConvertFrom-Json
        $selected=@($metrics.patches | Where-Object { $_.reason -eq 'HOST_DEADLINE_REVIEW' } | ForEach-Object { [string]$_.id })
        if(!$selected.Count){throw 'No pending timing IDs'}
    }
    if($StressOnly){$flags+='-DFACTORY_STRESS_ONLY=1'}
    if($Observe){
        python smk-p4/tests/instrument_factory_audio.py
        if($LASTEXITCODE){throw 'Observer generation failed'}
        & gcc -std=gnu11 @flags -c build/factory010/amy_observed.c -o build/factory010/amy_observed.o
        if($LASTEXITCODE){throw 'Observer compile failed'}
        $objects=@($objects | Where-Object { (Split-Path $_ -Leaf) -ne 'amy.c.o' }) + @('build/factory010/amy_observed.o')
        $flags+='-DFACTORY_AUDIO_OBSERVER=1'
    }
    & g++ -std=c++17 @flags smk-p4/tests/factory_library_probe.cpp smk-p4/tests/heap_probe.cpp smk-p4/components/amy_runtime/amy_runtime.cpp @objects '-Wl,--wrap=malloc' '-Wl,--wrap=calloc' '-Wl,--wrap=realloc' '-Wl,--wrap=free' -lm -o "build/factory010/$out.exe"
    if($LASTEXITCODE){throw 'Probe compile failed'}
    & "build/factory010/$out.exe" @selected | Tee-Object "build/factory010/$out.log"
    if($LASTEXITCODE){throw 'Factory probe failed'}
} finally {Pop-Location}
