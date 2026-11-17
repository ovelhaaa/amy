param([switch]$ReuseObjects)
$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $repo
try {
    New-Item -ItemType Directory -Force build | Out-Null
    $binary = 'build/performance_scenes_017b_test.exe'
    & g++ -std=c++17 -O2 -Wall -Wextra -Werror -DAMY_NO_MINIAUDIO -Isrc -Ismk-p4/components/amy_runtime -Ismk-p4/main -Ismk-p4/tests/nvs_mocks -Ishared/smk_protocol smk-p4/tests/performance_scenes_017b_test.cpp -o $binary
    if ($LASTEXITCODE) { throw 'Scene selection compile failed' }
    & $binary
    if ($LASTEXITCODE) { throw 'Scene selection test failed' }
    & "$PSScriptRoot/run_harmony.ps1" -ReuseObjects:$ReuseObjects
    $objects = Get-ChildItem build/harmony09/*.c.o | ForEach-Object FullName
    $flags = @(
        '-std=c++17', '-pthread', '-O2', '-Wall', '-Wextra', '-Werror',
        '-DSMKP4_SOUND_EDITOR=1', '-DSMKP4_FULL_FACTORY_LIBRARY=1',
        '-DSMKP4_HARMONY=1', '-DSMKP4_UNIFIED_ENGINE=1',
        '-DSMKP4_SOUND_LIBRARY=1', '-DSMKP4_VOICES=8',
        '-DAMY_NO_MINIAUDIO', '-DAMY_SAMPLE_RATE=48000',
        '-DAMY_BLOCK_SIZE=256', '-DBLOCK_SIZE_BITS=8',
        '-DAMY_PATCH_CHECKED_RESOURCE_ADMISSION=1',
        '-DAMY_EXCLUSIVE_FM_PHASE_RESTART=1',
        '-Isrc', '-Ismk-p4/main', '-Ismk-p4/components/amy_runtime', '-Ishared/smk_protocol',
        '-Ismk-p4/tests/nvs_mocks'
    )
    $runtime = 'build/scene_recall_runtime_test.exe'
    & g++ @flags smk-p4/tests/scene_recall_runtime_test.cpp smk-p4/components/amy_runtime/amy_runtime.cpp smk-p4/tests/heap_probe.cpp @objects '-Wl,--wrap=malloc' '-Wl,--wrap=calloc' '-Wl,--wrap=realloc' '-Wl,--wrap=free' -lm -o $runtime
    if ($LASTEXITCODE) { throw 'Scene runtime compile failed' }
    & $runtime
    if ($LASTEXITCODE) { throw 'Scene runtime test failed' }
} finally {
    Pop-Location
}
