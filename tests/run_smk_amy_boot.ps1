param([switch]$Legacy, [switch]$ResourcesOnly, [switch]$Mutations, [ValidateSet(128, 256)][int]$BlockSize = 256)
$ErrorActionPreference = 'Stop'
if ($Legacy -and $Mutations) { throw 'Materialization mutations require checked resource tests' }
$repo = Split-Path $PSScriptRoot -Parent
$build = Join-Path $repo 'build/smk_amy_boot'
if ($Legacy) { $build = Join-Path $repo 'build/smk_amy_legacy' }
if ($BlockSize -eq 128) {
    if (!$ResourcesOnly) { throw '128-frame host mode currently supports -ResourcesOnly' }
    $build = Join-Path $repo 'build/smk_amy_resources_128'
}
New-Item -ItemType Directory -Force -Path $build | Out-Null
Push-Location $repo
try {
    # Use the firmware's AMY source list, excluding standalone CLI programs.
    $cmake = Get-Content 'smk-s3/components/amy_engine/CMakeLists.txt' -Raw
    $sources = [regex]::Matches($cmake, '\$\{AMY_SRC_DIR\}/([^"\r\n]+\.c)') |
        ForEach-Object { $_.Groups[1].Value } |
        Where-Object { $_ -notin @('amy-message.c', 'amy-piano.c') }
    $blockBits = if ($BlockSize -eq 128) { 7 } else { 8 }
    $flags = @('-O2', '-DAMY_NO_MINIAUDIO', '-DAMY_SAMPLE_RATE=48000', "-DAMY_BLOCK_SIZE=$BlockSize",
               "-DBLOCK_SIZE_BITS=$blockBits", '-Isrc', '-Itests/mock', '-Ismk-s3/components/audio/include',
               '-Ismk-s3/components/system/include')
    if (!$Legacy) { $flags += '-DAMY_PATCH_CHECKED_RESOURCE_ADMISSION=1' }
    $objects = @()
    foreach ($source in $sources) {
        $obj = Join-Path $build ($source + '.o')
        & gcc -std=gnu11 @flags -c "src/$source" -o $obj
        if ($LASTEXITCODE -ne 0) { throw "Compile failed: $source" }
        $objects += $obj
    }
    if (!$Legacy) {
        $resourcesExe = Join-Path $build 'test_smk_instrument_resources.exe'
        $resourceAmy = Join-Path $build 'amy_resources.o'
        & gcc -std=gnu11 @flags -DMALLOC_CAPS_DEFINED -c src/amy.c -o $resourceAmy
        if ($LASTEXITCODE -ne 0) { throw 'Resource-test AMY compilation failed' }
        $resourceObjects = @($objects | Where-Object { (Split-Path $_ -Leaf) -ne 'amy.c.o' }) + @($resourceAmy)
        & g++ -std=c++17 @flags tests/test_smk_instrument_resources.cpp @resourceObjects '-Wl,--wrap=free' -lm -o $resourcesExe
        if ($LASTEXITCODE -ne 0) { throw 'Instrument resources test link failed' }
        & $resourcesExe
        if ($LASTEXITCODE -ne 0) { throw 'Instrument resources test failed' }
        if ($Mutations) {
            # Mutate ONLY isolated ignored copies. A nonzero exit alone is not
            # enough: require the intended invariant's assertion in stderr.
            $mutationDir = Join-Path $build 'mutations'
            New-Item -ItemType Directory -Force -Path $mutationDir | Out-Null
            $original = Get-Content src/amy.c -Raw
            $cases = @(
                @('false_success', 'if (p == NULL) { ++amy_global.oom_count; return AMY_RESOURCE_NO_MEMORY; }',
                  'if (p == NULL) { ++amy_global.oom_count; return AMY_RESOURCE_OK; }', 'Assertion.*amy_osc_bank_prepare'),
                @('omit_envelope', 'memcpy(replacement->breakpoint_values[set], old->breakpoint_values[set], n * sizeof(float));',
                  '(void)n; /* isolated mutation: omit envelope values */', 'Assertion.*breakpoint_values')
            )
            foreach ($case in $cases) {
                if (!$original.Contains($case[1])) { throw "Mutation anchor absent: $($case[0])" }
                $copy = Join-Path $mutationDir ($case[0] + '.c')
                $obj = Join-Path $mutationDir ($case[0] + '.o')
                $mutant = Join-Path $mutationDir ($case[0] + '.exe')
                $log = Join-Path $mutationDir ($case[0] + '.log')
                [IO.File]::WriteAllText($copy, $original.Replace($case[1], $case[2]))
                & gcc -std=gnu11 @flags -DMALLOC_CAPS_DEFINED -c $copy -o $obj
                if ($LASTEXITCODE -ne 0) { throw 'Mutant compilation failed' }
                $mutantObjects = @($objects | Where-Object { (Split-Path $_ -Leaf) -ne 'amy.c.o' }) + @($obj)
                & g++ -std=c++17 @flags tests/test_smk_instrument_resources.cpp @mutantObjects '-Wl,--wrap=free' -lm -o $mutant
                if ($LASTEXITCODE -ne 0) { throw 'Mutant link failed' }
                & $mutant 2> $log
                $mutantResult = $LASTEXITCODE
                if ($mutantResult -eq 0 -or (Get-Content $log -Raw) -notmatch $case[3]) {
                    throw "Mutation was not detected by intended invariant: $($case[0]) exit=$mutantResult"
                }
                Write-Host "PASS: mutation $($case[0]) rejected by intended assertion (exit=$mutantResult)"
            }
        }
        if ($ResourcesOnly) { return }
    }
    $exe = Join-Path $build 'test_smk_amy_boot.exe'
    & g++ -std=c++17 -pthread @flags tests/test_smk_amy_boot.cpp smk-s3/components/audio/amy_adapter.cpp smk-s3/components/audio/amy_commands.cpp @objects -lm -o $exe
    if ($LASTEXITCODE -ne 0) { throw 'Adapter test link failed' }
    $stderr = Join-Path $build 'stderr.log'
    if ($Legacy) { & $exe --legacy 2> $stderr }
    else { & $exe 2> $stderr }
    $result = $LASTEXITCODE
    $errors = Get-Content $stderr -Raw
    if ($errors) { Write-Host $errors }
    if ($result -ne 0) { throw "Adapter test failed: $result" }
    if (!$Legacy -and $errors) { throw 'Unexpected AMY diagnostics during regression test' }
    if (!$Legacy) {
        # Deliberately exhaust the real oscillator pool in a separate process.
        # Resource admission must reject the entire request before publication.
        $failureLog = Join-Path $build 'dual_allocation_failure.log'
        & $exe --dual-allocation-failure 2> $failureLog
        if ($LASTEXITCODE -ne 0) { throw 'Dual allocation negative probe failed' }
        $expected = @(Get-Content $failureLog)
        if (!$expected.Count) { throw 'Negative probe did not exercise allocation failure' }
        foreach ($line in $expected) {
            if ($line -ne 'patch resource admission failed: 5') {
                throw "Unexpected diagnostic in negative probe: $line"
            }
        }
        $ownerExe = Join-Path $build 'test_smk_amy_owner.exe'
        $wraps = @('-Wl,--wrap=amy_add_event', '-Wl,--wrap=amy_render', '-Wl,--wrap=amy_play_message',
                   '-Wl,--wrap=amy_execute_deltas', '-Wl,--wrap=amy_deltas_reset', '-Wl,--wrap=amy_stop')
        & g++ -std=c++17 -pthread -DSMK_TEST_CLOCK @flags tests/test_smk_amy_owner.cpp smk-s3/components/audio/amy_adapter.cpp smk-s3/components/audio/amy_commands.cpp @objects @wraps -lm -o $ownerExe
        if ($LASTEXITCODE -ne 0) { throw 'Owner test link failed' }
        & $ownerExe 2> $stderr
        $result = $LASTEXITCODE
        $errors = Get-Content $stderr -Raw
        if ($errors) { Write-Host $errors }
        if ($result -ne 0 -or $errors) { throw "Owner test failed: $result" }
    }
} finally {
    Pop-Location
}
