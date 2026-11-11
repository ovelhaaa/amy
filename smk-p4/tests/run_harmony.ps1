param([switch]$ReuseObjects)
$ErrorActionPreference='Stop'
$repo=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $repo
try {
    $build='build/harmony09'
    New-Item -ItemType Directory -Force $build | Out-Null
    $flags=@('-O2','-Wall','-Wextra','-DAMY_NO_MINIAUDIO','-DAMY_SAMPLE_RATE=48000','-DAMY_BLOCK_SIZE=256','-DBLOCK_SIZE_BITS=8','-DAMY_PATCH_CHECKED_RESOURCE_ADMISSION=1','-DAMY_EXCLUSIVE_FM_PHASE_RESTART=1','-DSMKP4_VOICES=8','-DSMKP4_SOUND_LIBRARY=1','-DSMKP4_UNIFIED_ENGINE=1','-DSMKP4_HARMONY=1','-Isrc','-Ismk-p4/components/amy_runtime','-Ismk-p4/main','-Ishared/smk_protocol','-Ismk-p4/tests/nvs_mocks')
    & g++ -std=c++17 @flags smk-p4/tests/harmony_test.cpp -o "$build/music.exe"
    if($LASTEXITCODE){throw 'Music compile failed'}
    & "$build/music.exe" | Tee-Object "$build/music.log"
    if($LASTEXITCODE){throw 'Music failed'}
    $cmake=Get-Content smk-p4/components/amy_runtime/CMakeLists.txt -Raw
    $sources=[regex]::Matches($cmake,'\$\{AMY_SRC_DIR\}/([^"\r\n]+\.c)') | ForEach-Object { $_.Groups[1].Value }
    $objects=@()
    foreach($source in $sources){
        $obj="$build/$source.o"
        if(!$ReuseObjects -or !(Test-Path $obj)){
            & gcc -std=gnu11 @flags -c "src/$source" -o $obj
            if($LASTEXITCODE){throw "AMY compile failed: $source"}
        }
        $objects+=$obj
    }
    foreach($test in @('harmony_runtime_test','usb_musical_test','test_retrigger_scenarios')){
        & g++ -std=c++17 -pthread @flags "smk-p4/tests/$test.cpp" smk-p4/tests/heap_probe.cpp smk-p4/components/amy_runtime/amy_runtime.cpp @objects '-Wl,--wrap=malloc' '-Wl,--wrap=calloc' '-Wl,--wrap=realloc' '-Wl,--wrap=free' -lm -o "$build/$test.exe"
        if($LASTEXITCODE){throw "Runtime compile failed: $test"}
        & "$build/$test.exe" | Tee-Object "$build/$test.log"
        if($LASTEXITCODE){throw "Runtime failed: $test"}
    }
} finally {Pop-Location}
