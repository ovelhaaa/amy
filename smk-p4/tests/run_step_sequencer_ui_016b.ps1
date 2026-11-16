$ErrorActionPreference='Stop'
$repo=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $repo
try {
    if (!(Test-Path 'build/ui_blueprint_host/liblvgl_host.a')) {
        & ./smk-p4/ui-preview/run_preview.ps1
    }
    if (!(Get-ChildItem 'build/harmony09/*.c.o' -ErrorAction SilentlyContinue)) {
        & ./smk-p4/tests/run_harmony.ps1 -ReuseObjects
    }
    $lvgl=Join-Path $repo 'smk-p4/managed_components/lvgl__lvgl'
    if(!(Test-Path $lvgl)) { $lvgl=Join-Path $repo '../../../smk-p4/managed_components/lvgl__lvgl' }
    $flags=@('-DSMKP4_SOUND_EDITOR=1','-DSMKP4_FULL_FACTORY_LIBRARY=1','-DSMKP4_HARMONY=1','-DSMKP4_UNIFIED_ENGINE=1','-DSMKP4_SOUND_LIBRARY=1','-DSMKP4_VOICES=8','-std=c++20','-O2','-Wall','-Wextra','-Werror','-DLV_CONF_INCLUDE_SIMPLE=1','-DAMY_NO_MINIAUDIO','-DAMY_SAMPLE_RATE=48000','-DAMY_BLOCK_SIZE=256','-DBLOCK_SIZE_BITS=8','-DAMY_PATCH_CHECKED_RESOURCE_ADMISSION=1','-DAMY_EXCLUSIVE_FM_PHASE_RESTART=1',"-I$lvgl",'-Ismk-p4/ui-preview','-Ismk-p4/components/ui_blueprint/include','-Ismk-p4/main','-Ishared/smk_protocol','-Ismk-p4/components/amy_runtime','-Isrc','-Ismk-p4/tests/nvs_mocks')
    New-Item -ItemType Directory -Force build/step016b-ui | Out-Null
    $objects=@()
    foreach($dir in @('src','widgets','screens')) {
        foreach($file in Get-ChildItem "smk-p4/components/ui_blueprint/$dir/*.cpp") {
            $obj="build/step016b-ui/$($file.BaseName).o"
            & g++ @flags -c $file.FullName -o $obj
            if($LASTEXITCODE) { throw "UI compile failed: $($file.Name)" }
            $objects+=$obj
        }
    }
    $amy=Get-ChildItem build/harmony09/*.c.o | ForEach-Object FullName
    & g++ @flags smk-p4/tests/step_sequencer_ui_016b_test.cpp smk-p4/main/ui_blueprint_performance_adapter.cpp smk-p4/components/amy_runtime/amy_runtime.cpp smk-p4/tests/heap_probe.cpp @objects @amy build/ui_blueprint_host/liblvgl_host.a '-Wl,--wrap=malloc' '-Wl,--wrap=calloc' '-Wl,--wrap=realloc' '-Wl,--wrap=free' -lm -o build/step016b-ui/test.exe
    if($LASTEXITCODE) { throw 'Sequencer UI link failed' }
    & build/step016b-ui/test.exe build/step016b-ui | Tee-Object build/step016b-ui/test.log
    if($LASTEXITCODE) { throw 'Sequencer UI test failed' }
} finally { Pop-Location }
