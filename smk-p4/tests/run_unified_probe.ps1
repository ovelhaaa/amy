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
} finally { Pop-Location }
