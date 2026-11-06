$ErrorActionPreference='Stop'
$repo=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $repo
try {
    $build='build/unified08'
    New-Item -ItemType Directory -Force $build | Out-Null
    $cmake=Get-Content smk-p4/components/amy_runtime/CMakeLists.txt -Raw
    $sources=[regex]::Matches($cmake,'\$\{AMY_SRC_DIR\}/([^"\r\n]+\.c)') | ForEach-Object { $_.Groups[1].Value }
    $flags=@('-O2','-DAMY_NO_MINIAUDIO','-DAMY_SAMPLE_RATE=48000','-DAMY_BLOCK_SIZE=256','-DBLOCK_SIZE_BITS=8','-DAMY_PATCH_CHECKED_RESOURCE_ADMISSION=1','-DAMY_EXCLUSIVE_FM_PHASE_RESTART=1','-Isrc','-Ismk-p4/components/amy_runtime')
    $objects=@()
    foreach($source in $sources) {
        $obj="$build/$source.o"
        & gcc -std=gnu11 @flags -c "src/$source" -o $obj
        if($LASTEXITCODE) { throw "Compile failed: $source" }
        $objects+=$obj
    }
    & g++ -std=c++17 @flags smk-p4/tests/unified_patch_probe.cpp smk-p4/tests/heap_probe.cpp smk-p4/components/amy_runtime/amy_runtime.cpp @objects '-Wl,--wrap=malloc' '-Wl,--wrap=calloc' '-Wl,--wrap=realloc' '-Wl,--wrap=free' -lm -o "$build/probe.exe"
    if($LASTEXITCODE) { throw 'Unified probe link failed' }
    & "$build/probe.exe" | Tee-Object "$build/probe.log"
    if($LASTEXITCODE) { throw 'Unified probe failed' }
    & g++ -std=c++17 @flags smk-p4/tests/patch_bank_probe.cpp smk-p4/tests/heap_probe.cpp smk-p4/components/amy_runtime/amy_runtime.cpp @objects '-Wl,--wrap=malloc' '-Wl,--wrap=calloc' '-Wl,--wrap=realloc' '-Wl,--wrap=free' -lm -o "$build/candidates.exe"
    if($LASTEXITCODE) { throw 'Candidate probe link failed' }
    & "$build/candidates.exe" | Tee-Object "$build/candidates.log"
    if($LASTEXITCODE) { throw 'Candidate probe failed' }
    & g++ -std=c++17 -pthread @flags -DSMKP4_VOICES=8 -DSMKP4_SOUND_LIBRARY=1 -DSMKP4_UNIFIED_ENGINE=1 -Ismk-p4/main -Ishared/smk_protocol smk-p4/tests/unified_runtime_test.cpp smk-p4/tests/heap_probe.cpp smk-p4/components/amy_runtime/amy_runtime.cpp @objects '-Wl,--wrap=malloc' '-Wl,--wrap=calloc' '-Wl,--wrap=realloc' '-Wl,--wrap=free' -lm -o "$build/runtime.exe"
    if($LASTEXITCODE) { throw 'Unified runtime link failed' }
    & "$build/runtime.exe" | Tee-Object "$build/runtime.log"
    if($LASTEXITCODE) { throw 'Unified runtime failed' }
    foreach($test in @('usb_musical_test','test_retrigger_scenarios')) {
        & g++ -std=c++17 -pthread @flags -DSMKP4_VOICES=8 -DSMKP4_SOUND_LIBRARY=1 -DSMKP4_UNIFIED_ENGINE=1 "smk-p4/tests/$test.cpp" smk-p4/tests/heap_probe.cpp smk-p4/components/amy_runtime/amy_runtime.cpp @objects '-Wl,--wrap=malloc' '-Wl,--wrap=calloc' '-Wl,--wrap=realloc' '-Wl,--wrap=free' -lm -o "$build/$test.exe"
        if($LASTEXITCODE) { throw "Unified link failed: $test" }
        & "$build/$test.exe" | Tee-Object "$build/$test.log"
        if($LASTEXITCODE) { throw "Unified test failed: $test" }
    }
} finally { Pop-Location }
