$ErrorActionPreference='Stop'
$repo=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $repo
try {
    python smk-p4/tests/generate_factory_library.py --check
    if($LASTEXITCODE){throw 'Stale factory catalog'}
    $objects=Get-ChildItem build/harmony09/*.c.o | ForEach-Object FullName
    $flags=@('-std=c++17','-O2','-DSMKP4_FULL_FACTORY_LIBRARY=1','-DSMKP4_HARMONY=1','-DSMKP4_UNIFIED_ENGINE=1','-DSMKP4_SOUND_LIBRARY=1','-DSMKP4_VOICES=8','-DAMY_NO_MINIAUDIO','-DAMY_SAMPLE_RATE=48000','-DAMY_BLOCK_SIZE=256','-DBLOCK_SIZE_BITS=8','-DAMY_PATCH_CHECKED_RESOURCE_ADMISSION=1','-DAMY_EXCLUSIVE_FM_PHASE_RESTART=1','-Isrc','-Ismk-p4/main','-Ismk-p4/components/amy_runtime','-Ishared/smk_protocol')
    & g++ @flags smk-p4/tests/factory_runtime_test.cpp smk-p4/components/amy_runtime/amy_runtime.cpp smk-p4/tests/heap_probe.cpp @objects '-Wl,--wrap=malloc' '-Wl,--wrap=calloc' '-Wl,--wrap=realloc' '-Wl,--wrap=free' -lm -o build/factory010/runtime.exe
    if($LASTEXITCODE){throw 'Factory runtime compile failed'}
    & build/factory010/runtime.exe | Tee-Object build/factory010/runtime.log
    if($LASTEXITCODE){throw 'Factory runtime test failed'}
} finally {Pop-Location}
