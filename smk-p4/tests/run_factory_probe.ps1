$ErrorActionPreference='Stop'
$repo=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $repo
try {
    New-Item -ItemType Directory -Force build/factory010 | Out-Null
    $objects=Get-ChildItem build/harmony09/*.c.o | ForEach-Object FullName
    if(!$objects){throw 'Run run_harmony.ps1 first (same AMY compile definitions)'}
    $flags=@('-O2','-DAMY_NO_MINIAUDIO','-DAMY_SAMPLE_RATE=48000','-DAMY_BLOCK_SIZE=256','-DBLOCK_SIZE_BITS=8','-DAMY_PATCH_CHECKED_RESOURCE_ADMISSION=1','-DAMY_EXCLUSIVE_FM_PHASE_RESTART=1','-Isrc','-Ismk-p4/components/amy_runtime')
    & g++ -std=c++17 @flags smk-p4/tests/factory_library_probe.cpp smk-p4/tests/heap_probe.cpp smk-p4/components/amy_runtime/amy_runtime.cpp @objects '-Wl,--wrap=malloc' '-Wl,--wrap=calloc' '-Wl,--wrap=realloc' '-Wl,--wrap=free' -lm -o build/factory010/probe.exe
    if($LASTEXITCODE){throw 'Probe compile failed'}
    & build/factory010/probe.exe | Tee-Object build/factory010/probe.log
    if($LASTEXITCODE){throw 'Factory probe failed'}
} finally {Pop-Location}
