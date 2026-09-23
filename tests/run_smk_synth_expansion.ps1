$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
Push-Location $repo
try {
    New-Item -ItemType Directory -Force -Path 'build/smk_expansion' | Out-Null
    $includes = @(
        '-Itests/mock',
        '-Ismk-s3/components/synth/include',
        '-Ismk-s3/components/midi/include',
        '-Ismk-s3/components/sequencer/include',
        '-Ismk-s3/components/system/include',
        '-Ismk-s3/components/storage/include',
        '-Ismk-s3/components/audio/include',
        '-Ismk-s3/components/ui/include'
    )
    $sources = @(
        'tests/test_smk_synth_expansion.cpp',
        'smk-s3/components/midi/event_bus.cpp',
        'smk-s3/components/midi/controller_profile.cpp',
        'smk-s3/components/sequencer/src/step_sequencer.cpp',
        'smk-s3/components/synth/src/factory_patches.cpp',
        'smk-s3/components/synth/src/patch_manager.cpp',
        'smk-s3/components/synth/src/soft_takeover.cpp',
        'smk-s3/components/storage/src/storage_manager.cpp'
    )
    $exe = 'build/smk_expansion/test_expansion.exe'
    & g++ -std=c++17 -O2 @includes $sources -o $exe
    if ($LASTEXITCODE -ne 0) { throw 'Expansion test compilation failed' }
    & ./$exe
    if ($LASTEXITCODE -ne 0) { throw 'Expansion tests failed' }
} finally {
    Pop-Location
}
