param([ValidateSet(4,6,8)][int]$Voices=4,[switch]$SoundLibrary)
$ErrorActionPreference='Stop'
$repo=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $repo
try {
    $build=if($Voices -eq 4) { 'build/smk_p4_amy' } else { "build/smk_p4_amy_$Voices" }
    if($SoundLibrary) { $build+="_library" }
    New-Item -ItemType Directory -Force $build | Out-Null
    $cmake=Get-Content smk-p4/components/amy_runtime/CMakeLists.txt -Raw
    $sources=[regex]::Matches($cmake,'\$\{AMY_SRC_DIR\}/([^"\r\n]+\.c)') | ForEach-Object { $_.Groups[1].Value }
    $flags=@("-DSMKP4_VOICES=$Voices",'-O2','-DAMY_NO_MINIAUDIO','-DAMY_SAMPLE_RATE=48000','-DAMY_BLOCK_SIZE=256','-DBLOCK_SIZE_BITS=8','-DAMY_PATCH_CHECKED_RESOURCE_ADMISSION=1','-DAMY_EXCLUSIVE_FM_PHASE_RESTART=1','-Isrc','-Ismk-p4/components/amy_runtime')
    if($SoundLibrary) { $flags+='-DSMKP4_SOUND_LIBRARY=1' }
    $objects=@()
    foreach($source in $sources) {
        $obj="$build/$source.o"
        & gcc -std=gnu11 @flags -c "src/$source" -o $obj
        if($LASTEXITCODE -ne 0) { throw "Compile failed: $source" }
        $objects+=$obj
    }
    $wraps=@('-Wl,--wrap=malloc','-Wl,--wrap=calloc','-Wl,--wrap=realloc','-Wl,--wrap=free')
    & g++ -std=c++17 @flags smk-p4/tests/amy_runtime_test.cpp smk-p4/tests/heap_probe.cpp smk-p4/components/amy_runtime/amy_runtime.cpp smk-p4/components/amy_runtime/self_test.cpp @objects @wraps -lm -o "$build/runtime.exe"
    if($LASTEXITCODE -ne 0) { throw 'Runtime link failed' }
    & "$build/runtime.exe"
    if($LASTEXITCODE -ne 0) { throw 'Runtime test failed' }
    & g++ -std=c++17 -pthread @flags smk-p4/tests/usb_musical_test.cpp smk-p4/tests/heap_probe.cpp smk-p4/components/amy_runtime/amy_runtime.cpp @objects @wraps -lm -o "$build/usb_musical.exe"
    if($LASTEXITCODE -ne 0) { throw 'USB musical link failed' }
    & "$build/usb_musical.exe"
    if($LASTEXITCODE -ne 0) { throw 'USB musical test failed' }
    & g++ -std=c++17 @flags smk-p4/tests/polyphony_test.cpp smk-p4/tests/heap_probe.cpp smk-p4/components/amy_runtime/amy_runtime.cpp @objects @wraps -lm -o "$build/polyphony.exe"
    if($LASTEXITCODE -ne 0) { throw 'Polyphony link failed' }
    & "$build/polyphony.exe"
    if($LASTEXITCODE -ne 0) { throw 'Polyphony test failed' }
    foreach($test in @('test_retrigger_scenarios','render_retrigger_probe')) {
        & g++ -std=c++17 @flags "smk-p4/tests/$test.cpp" smk-p4/components/amy_runtime/amy_runtime.cpp @objects -lm -o "$build/$test.exe"
        if($LASTEXITCODE -ne 0) { throw "Link failed: $test" }
        & "$build/$test.exe" $build "profile$Voices"
        if($LASTEXITCODE -ne 0) { throw "Test failed: $test" }
    }
    & "$build/render_retrigger_probe.exe" $build "full$Voices" capacity
    if($LASTEXITCODE -ne 0) { throw 'Full-capacity WAV probe failed' }
    if($SoundLibrary -and $Voices -eq 8) {
        foreach($test in @('patch_bank_probe','sound_library_test')) {
            & g++ -std=c++17 @flags -Ismk-p4/main -Ishared/smk_protocol "smk-p4/tests/$test.cpp" smk-p4/tests/heap_probe.cpp smk-p4/components/amy_runtime/amy_runtime.cpp @objects @wraps -lm -o "$build/$test.exe"
            if($LASTEXITCODE) { throw "Link failed: $test" }
            & "$build/$test.exe"
            if($LASTEXITCODE) { throw "Test failed: $test" }
        }
    }
    $includes=@('-Ismk-p4/main','-Ishared/smk_protocol')
    foreach($test in @('local_backend_test','performance_backend_contract_test','performance_ui_test')) {
        $extra=@()
        if($test -eq 'performance_ui_test') { $extra=@('-Ismk-p4/tests/mocks','smk-p4/main/performance_ui.cpp') }
        & g++ -std=c++17 -pthread @flags @includes @extra "smk-p4/tests/$test.cpp" smk-p4/components/amy_runtime/amy_runtime.cpp @objects -lm -o "$build/$test.exe"
        if($LASTEXITCODE -ne 0) { throw "Link failed: $test" }
        & "$build/$test.exe"
        if($LASTEXITCODE -ne 0) { throw "Test failed: $test" }
    }
} finally { Pop-Location }
