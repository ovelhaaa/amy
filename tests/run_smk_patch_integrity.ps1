$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
Push-Location $repo
try {
    $build = 'build/smk_patch_integrity'
    New-Item -ItemType Directory -Force -Path $build | Out-Null

    # Reuse the firmware's AMY source list, excluding standalone CLI programs.
    $cmake = Get-Content 'smk-s3/components/amy_engine/CMakeLists.txt' -Raw
    $sources = [regex]::Matches($cmake, '\$\{AMY_SRC_DIR\}/([^"\r\n]+\.c)') |
        ForEach-Object { $_.Groups[1].Value } |
        Where-Object { $_ -notin @('amy-message.c', 'amy-piano.c') }

    $flags = @(
        '-O2', '-DAMY_NO_MINIAUDIO', '-DAMY_SAMPLE_RATE=48000', '-DAMY_BLOCK_SIZE=256',
        '-DBLOCK_SIZE_BITS=8', '-Isrc', '-Itests/mock',
        '-Ismk-s3/components/audio/include', '-Ismk-s3/components/system/include',
        '-Ismk-s3/components/synth/include', '-Ismk-s3/components/sequencer/include',
        '-Ismk-s3/components/midi/include', '-Ismk-s3/components/storage/include',
        '-Ismk-s3/components/ui/include'
    )

    $objects = @()
    foreach ($source in $sources) {
        $obj = Join-Path $build ($source + '.o')
        & gcc -std=gnu11 @flags -c "src/$source" -o $obj
        if ($LASTEXITCODE -ne 0) { throw "Compile failed: $source" }
        $objects += $obj
    }

    $exe = Join-Path $build 'test_smk_patch_integrity.exe'
    & g++ -std=c++17 -pthread @flags tests/test_smk_patch_integrity.cpp `
        smk-s3/components/audio/amy_adapter.cpp `
        smk-s3/components/audio/amy_commands.cpp `
        smk-s3/components/synth/src/patch_manager.cpp `
        smk-s3/components/synth/src/factory_patches.cpp `
        smk-s3/components/synth/src/soft_takeover.cpp `
        smk-s3/components/sequencer/src/step_sequencer.cpp `
        smk-s3/components/midi/event_bus.cpp `
        smk-s3/components/midi/controller_profile.cpp `
        smk-s3/components/storage/src/storage_manager.cpp `
        @objects -lm -o $exe
    if ($LASTEXITCODE -ne 0) { throw 'Patch integrity test link failed' }

    $stderr = Join-Path $build 'stderr.log'
    & $exe 2> $stderr
    $result = $LASTEXITCODE
    $errors = Get-Content $stderr -Raw
    if ($errors) { Write-Host $errors }
    if ($result -ne 0) { throw "Patch integrity test failed: $result" }
    if ($errors) { throw 'Unexpected AMY diagnostics during patch integrity test' }

    # M3.1 real-AMY reverb freeze regression: fresh engine, lazy tank allocation,
    # disable, patch switching, order independence and v6 save/reload. Link a
    # second executable so the AMY engine starts from a clean (tank-absent) state.
    $frozenExe = Join-Path $build 'test_smk_reverb_freeze.exe'
    & g++ -std=c++17 -pthread @flags tests/test_smk_reverb_freeze.cpp `
        smk-s3/components/audio/amy_adapter.cpp `
        smk-s3/components/audio/amy_commands.cpp `
        smk-s3/components/synth/src/patch_manager.cpp `
        smk-s3/components/synth/src/factory_patches.cpp `
        smk-s3/components/synth/src/soft_takeover.cpp `
        smk-s3/components/sequencer/src/step_sequencer.cpp `
        smk-s3/components/midi/event_bus.cpp `
        smk-s3/components/midi/controller_profile.cpp `
        smk-s3/components/storage/src/storage_manager.cpp `
        @objects -lm -o $frozenExe
    if ($LASTEXITCODE -ne 0) { throw 'Reverb freeze test link failed' }

    $frozenStderr = Join-Path $build 'stderr_freeze.log'
    & $frozenExe 2> $frozenStderr
    $frozenResult = $LASTEXITCODE
    $frozenErrors = Get-Content $frozenStderr -Raw
    if ($frozenErrors) { Write-Host $frozenErrors }
    if ($frozenResult -ne 0) { throw "Reverb freeze test failed: $frozenResult" }
    if ($frozenErrors) { throw 'Unexpected AMY diagnostics during reverb freeze test' }
} finally {
    Pop-Location
}
