param([ValidateSet(4,6,8)][int]$Voices=4,[switch]$SoundLibrary)
$ErrorActionPreference='Stop'
$repo=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $repo
try {
    # run_preview.ps1 and run_amy.ps1 produce the libraries/objects used here.
    $lvgl=Join-Path $repo '../../../smk-p4/managed_components/lvgl__lvgl'
    if(Test-Path smk-p4/managed_components/lvgl__lvgl) { $lvgl=Join-Path $repo 'smk-p4/managed_components/lvgl__lvgl' }
    $uiObjects=Get-ChildItem build/ui_blueprint_host/ui_obj/*.o | ForEach-Object FullName
    $amyBuild=if($Voices -eq 4) { 'build/smk_p4_amy' } else { "build/smk_p4_amy_$Voices" }
    $extraFlags=@()
    $suffix="$Voices"
    if($SoundLibrary) { $amyBuild+='_library'; $extraFlags+='-DSMKP4_SOUND_LIBRARY=1'; $suffix+='_library' }
    $amyObjects=Get-ChildItem "$amyBuild/*.c.o" | ForEach-Object FullName
    New-Item -ItemType Directory -Force build/p404 | Out-Null
    & g++ @extraFlags "-DSMKP4_VOICES=$Voices" -std=c++20 -O2 -Wall -Wextra -Werror -DLV_CONF_INCLUDE_SIMPLE=1 -DAMY_NO_MINIAUDIO -DAMY_SAMPLE_RATE=48000 -DAMY_BLOCK_SIZE=256 -DBLOCK_SIZE_BITS=8 -DAMY_PATCH_CHECKED_RESOURCE_ADMISSION=1 "-I$lvgl" -Ismk-p4/ui-preview -Ismk-p4/components/ui_blueprint/include -Ismk-p4/main -Ishared/smk_protocol -Ismk-p4/components/amy_runtime -Isrc smk-p4/tests/ui_blueprint_adapter_test.cpp smk-p4/main/ui_blueprint_performance_adapter.cpp smk-p4/components/amy_runtime/amy_runtime.cpp @uiObjects @amyObjects build/ui_blueprint_host/liblvgl_host.a -lm -o build/p404/adapter$suffix.exe
    if($LASTEXITCODE) { throw 'Adapter compilation failed' }
    & ./build/p404/adapter$suffix.exe
    if($LASTEXITCODE) { throw 'Adapter tests failed' }
} finally { Pop-Location }
