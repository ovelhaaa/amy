param([switch]$Legacy)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$build = Join-Path $repo 'build/smk_amy_boot'
New-Item -ItemType Directory -Force -Path $build | Out-Null
Push-Location $repo
try {
    # Use the firmware's AMY source list, excluding standalone CLI programs.
    $cmake = Get-Content 'smk-s3/components/amy_engine/CMakeLists.txt' -Raw
    $sources = [regex]::Matches($cmake, '\$\{AMY_SRC_DIR\}/([^"\r\n]+\.c)') |
        ForEach-Object { $_.Groups[1].Value } |
        Where-Object { $_ -notin @('amy-message.c', 'amy-piano.c') }
    $flags = @('-O2', '-DAMY_NO_MINIAUDIO', '-DAMY_SAMPLE_RATE=48000', '-DAMY_BLOCK_SIZE=256',
               '-DBLOCK_SIZE_BITS=8', '-Isrc', '-Itests/mock', '-Ismk-s3/components/audio/include',
               '-Ismk-s3/components/system/include')
    $objects = @()
    foreach ($source in $sources) {
        $obj = Join-Path $build ($source + '.o')
        & gcc -std=gnu11 @flags -c "src/$source" -o $obj
        if ($LASTEXITCODE -ne 0) { throw "Compile failed: $source" }
        $objects += $obj
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
        $ownerExe = Join-Path $build 'test_smk_amy_owner.exe'
        $wraps = @('-Wl,--wrap=amy_add_event', '-Wl,--wrap=amy_render', '-Wl,--wrap=amy_play_message',
                   '-Wl,--wrap=amy_execute_deltas', '-Wl,--wrap=amy_deltas_reset', '-Wl,--wrap=amy_stop')
        & g++ -std=c++17 -pthread @flags tests/test_smk_amy_owner.cpp smk-s3/components/audio/amy_adapter.cpp smk-s3/components/audio/amy_commands.cpp @objects @wraps -lm -o $ownerExe
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
