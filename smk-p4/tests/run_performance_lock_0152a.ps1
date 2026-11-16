$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $repo
try {
    $objects = Get-ChildItem build/harmony09/*.c.o | ForEach-Object FullName
    $flags = @(
        '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
        '-DSMKP4_SOUND_EDITOR=1', '-DSMKP4_FULL_FACTORY_LIBRARY=1',
        '-DSMKP4_HARMONY=1', '-DSMKP4_UNIFIED_ENGINE=1',
        '-DSMKP4_SOUND_LIBRARY=1', '-DSMKP4_VOICES=8',
        '-DAMY_NO_MINIAUDIO', '-DAMY_SAMPLE_RATE=48000',
        '-DAMY_BLOCK_SIZE=256', '-DBLOCK_SIZE_BITS=8',
        '-DAMY_PATCH_CHECKED_RESOURCE_ADMISSION=1',
        '-DAMY_EXCLUSIVE_FM_PHASE_RESTART=1',
        '-Isrc', '-Ismk-p4/main', '-Ismk-p4/components/amy_runtime',
        '-Ishared/smk_protocol', '-Ismk-p4/tests/nvs_mocks'
    )
    $binary = 'build/performance_lock_0152a_test.exe'
    & g++ @flags smk-p4/tests/performance_lock_0152a_test.cpp smk-p4/components/amy_runtime/amy_runtime.cpp smk-p4/tests/heap_probe.cpp @objects '-Wl,--wrap=malloc' '-Wl,--wrap=calloc' '-Wl,--wrap=realloc' '-Wl,--wrap=free' -lm -o $binary
    if ($LASTEXITCODE) { throw 'Compile failed' }
    & $binary
    if ($LASTEXITCODE) { throw 'Test failed' }
} finally {
    Pop-Location
}
