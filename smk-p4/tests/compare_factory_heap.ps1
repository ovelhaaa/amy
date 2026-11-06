$ErrorActionPreference='Stop'
$repo=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $repo
try{
    $objects=Get-ChildItem build/harmony09/*.c.o | ForEach-Object FullName
    $flags=@('-std=c++17','-O2','-DAMY_NO_MINIAUDIO','-DAMY_SAMPLE_RATE=48000','-DAMY_BLOCK_SIZE=256','-DBLOCK_SIZE_BITS=8','-DAMY_PATCH_CHECKED_RESOURCE_ADMISSION=1','-DAMY_EXCLUSIVE_FM_PHASE_RESTART=1','-DSMKP4_HARMONY=1','-DSMKP4_UNIFIED_ENGINE=1','-DSMKP4_SOUND_LIBRARY=1','-DSMKP4_VOICES=8')
    foreach($which in @('baseline','candidate')){
        $source=if($which -eq 'baseline'){(Resolve-Path ../smk-p4-harmony-assist-09).Path}else{$repo}
        $extra=@();if($which -eq 'candidate'){$extra+='-DSMKP4_FULL_FACTORY_LIBRARY=1'}
        & g++ @flags @extra "-I$source/src" "-I$source/smk-p4/components/amy_runtime" smk-p4/tests/factory_heap_comparison.cpp "$source/smk-p4/components/amy_runtime/amy_runtime.cpp" smk-p4/tests/heap_probe.cpp @objects '-Wl,--wrap=malloc' '-Wl,--wrap=calloc' '-Wl,--wrap=realloc' '-Wl,--wrap=free' -lm -o "build/factory010/heap_$which.exe"
        if($LASTEXITCODE){throw 'Heap compile failed'}
        & "build/factory010/heap_$which.exe" | Tee-Object "build/factory010/heap_$which.log"
        if($LASTEXITCODE){throw 'Heap comparison failed'}
    }
}finally{Pop-Location}
