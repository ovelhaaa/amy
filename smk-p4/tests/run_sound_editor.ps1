$ErrorActionPreference='Stop'
$repo=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $repo
try {
    New-Item -ItemType Directory -Force build/editor011 | Out-Null
    $objects=Get-ChildItem build/harmony09/*.c.o | ForEach-Object FullName
    if(!$objects){throw 'Run run_harmony.ps1 first'}
    $flags=@('-std=c++17','-O2','-Wall','-Wextra','-Werror','-DSMKP4_SOUND_EDITOR=1','-DSMKP4_FULL_FACTORY_LIBRARY=1','-DSMKP4_HARMONY=1','-DSMKP4_UNIFIED_ENGINE=1','-DSMKP4_SOUND_LIBRARY=1','-DSMKP4_VOICES=8','-DAMY_NO_MINIAUDIO','-DAMY_SAMPLE_RATE=48000','-DAMY_BLOCK_SIZE=256','-DBLOCK_SIZE_BITS=8','-DAMY_PATCH_CHECKED_RESOURCE_ADMISSION=1','-DAMY_EXCLUSIVE_FM_PHASE_RESTART=1','-Isrc','-Ismk-p4/main','-Ismk-p4/components/amy_runtime','-Ishared/smk_protocol')
    & g++ @flags smk-p4/tests/sound_editor_test.cpp smk-p4/components/amy_runtime/amy_runtime.cpp smk-p4/tests/heap_probe.cpp @objects '-Wl,--wrap=malloc' '-Wl,--wrap=calloc' '-Wl,--wrap=realloc' '-Wl,--wrap=free' -lm -o build/editor011/test.exe
    if($LASTEXITCODE){throw 'Editor compile failed'}
    & build/editor011/test.exe | Tee-Object build/editor011/test.log
    if($LASTEXITCODE){throw 'Editor test failed'}
    foreach($test in @('sound_editor_pcm_test','harmony_runtime_test','usb_musical_test','factory_runtime_test')){
        & g++ @flags -Wno-error=misleading-indentation "smk-p4/tests/$test.cpp" smk-p4/components/amy_runtime/amy_runtime.cpp smk-p4/tests/heap_probe.cpp @objects '-Wl,--wrap=malloc' '-Wl,--wrap=calloc' '-Wl,--wrap=realloc' '-Wl,--wrap=free' -lm -o "build/editor011/$test.exe"
        if($LASTEXITCODE){throw "Regression compile failed: $test"}
        & "build/editor011/$test.exe" | Tee-Object "build/editor011/$test.log"
        if($LASTEXITCODE){throw "Regression failed: $test"}
    }
} finally {Pop-Location}
