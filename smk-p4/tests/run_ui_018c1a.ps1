$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $repo
try {
    $lvgl = Join-Path $repo 'smk-p4/managed_components/lvgl__lvgl'
    $shotsDir = Join-Path $repo 'smk-p4/docs/ui_018c1a_shots'
    New-Item -ItemType Directory -Force -Path $shotsDir | Out-Null
    New-Item -ItemType Directory -Force -Path build/ui_018c1a | Out-Null

    $uiObjects = Get-ChildItem build/ui_blueprint_host/ui_obj/*.o | Where-Object {
        $_.Name -notin @('amy_runtime.o', 'ui_blueprint_performance_adapter.o', 'ui_blueprint.o')
    } | ForEach-Object FullName
    $amyObjects = Get-ChildItem build/harmony09/*.c.o | ForEach-Object FullName

    & g++ -DSMKP4_VOICES=8 -DSMKP4_SOUND_LIBRARY=1 -DSMKP4_UNIFIED_ENGINE=1 -DSMKP4_HARMONY=1 `
        -DSMKP4_FULL_FACTORY_LIBRARY=1 -DSMKP4_SOUND_EDITOR=1 -DSMKP4_LIVE_PERFORMANCE=1 `
        -DSMKP4_MACRO_ROUTING=1 -DSMKP4_SMK25_0142=1 -DSMKP4_ARP_015=1 -DSMKP4_DELAY_SYNC_0151=1 `
        -DSMKP4_PERF_LOCK_0152=1 -DSMKP4_STEP_SEQUENCER=1 -DSMKP4_PATTERN_LIBRARY=1 -DSMKP4_PERFORMANCE_SCENES=1 `
        -std=c++20 -O2 -Wall -Wextra -Werror -DLV_CONF_INCLUDE_SIMPLE=1 -DAMY_NO_MINIAUDIO `
        -DAMY_SAMPLE_RATE=48000 -DAMY_BLOCK_SIZE=256 -DBLOCK_SIZE_BITS=8 -DAMY_PATCH_CHECKED_RESOURCE_ADMISSION=1 `
        "-I$lvgl" -Ismk-p4/tests/nvs_mocks -Ismk-p4/ui-preview -Ismk-p4/components/ui_blueprint/include -Ismk-p4/main `
        -Ishared/smk_protocol -Ismk-p4/components/amy_runtime -Isrc `
        smk-p4/tests/ui_018c1a_hardening_test.cpp smk-p4/components/ui_blueprint/src/ui_blueprint.cpp `
        smk-p4/main/ui_blueprint_performance_adapter.cpp `
        smk-p4/components/amy_runtime/amy_runtime.cpp @uiObjects @amyObjects `
        build/ui_blueprint_host/liblvgl_host.a -lm -o build/ui_018c1a/test.exe
    if ($LASTEXITCODE) { throw '0.18-C.1a hardening test compilation failed' }

    & ./build/ui_018c1a/test.exe $shotsDir
    if ($LASTEXITCODE) { throw '0.18-C.1a hardening test failed' }

    python -c "
from PIL import Image
import glob, os
for ppm in sorted(glob.glob(r'$shotsDir/*.ppm')):
    png = ppm[:-4] + '.png'
    im = Image.open(ppm)
    assert im.size == (800, 480), f'Unexpected size {im.size} for {ppm}'
    im.save(png)
    os.remove(ppm)
print(r'Converted and verified 800x480 PNGs in $shotsDir')
"
    if ($LASTEXITCODE) { throw 'PNG conversion failed' }
} finally {
    Pop-Location
}
