$ErrorActionPreference='Stop'
$repo=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $repo
try {
    $build='build/smk_p4_amy'
    New-Item -ItemType Directory -Force $build | Out-Null
    $cmake=Get-Content smk-p4/components/amy_runtime/CMakeLists.txt -Raw
    $sources=[regex]::Matches($cmake,'\$\{AMY_SRC_DIR\}/([^"\r\n]+\.c)') | ForEach-Object { $_.Groups[1].Value }
    $flags=@('-O2','-DAMY_NO_MINIAUDIO','-DAMY_SAMPLE_RATE=48000','-DAMY_BLOCK_SIZE=256','-DBLOCK_SIZE_BITS=8','-DAMY_PATCH_CHECKED_RESOURCE_ADMISSION=1','-Isrc','-Ismk-p4/components/amy_runtime')
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
    $includes=@('-Ismk-p4/main','-Ishared/smk_protocol')
    foreach($test in @('local_backend_test','performance_ui_test')) {
        $extra=@()
        if($test -eq 'performance_ui_test') { $extra=@('-Ismk-p4/tests/mocks','smk-p4/main/performance_ui.cpp') }
        & g++ -std=c++17 -pthread @flags @includes @extra "smk-p4/tests/$test.cpp" smk-p4/components/amy_runtime/amy_runtime.cpp @objects -lm -o "$build/$test.exe"
        if($LASTEXITCODE -ne 0) { throw "Link failed: $test" }
        & "$build/$test.exe"
        if($LASTEXITCODE -ne 0) { throw "Test failed: $test" }
    }
} finally { Pop-Location }
