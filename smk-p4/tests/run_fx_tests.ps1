param([string]$TestName = "all")
$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $repo
try {
    New-Item -ItemType Directory -Force build/fx012 | Out-Null
    $objects = Get-ChildItem build/harmony09/*.c.o | ForEach-Object FullName
    if (!$objects) { throw 'Run run_harmony.ps1 first to build harmony09 objects' }
    $flags = @(
        '-std=c++20', '-O2', '-Wall', '-Wextra',
        '-DSMKP4_SOUND_EDITOR=1', '-DSMKP4_FULL_FACTORY_LIBRARY=1',
        '-DSMKP4_HARMONY=1', '-DSMKP4_UNIFIED_ENGINE=1', '-DSMKP4_SOUND_LIBRARY=1',
        '-DSMKP4_VOICES=8', '-DAMY_NO_MINIAUDIO', '-DAMY_SAMPLE_RATE=48000',
        '-DAMY_BLOCK_SIZE=256', '-DBLOCK_SIZE_BITS=8',
        '-DAMY_PATCH_CHECKED_RESOURCE_ADMISSION=1', '-DAMY_EXCLUSIVE_FM_PHASE_RESTART=1',
        '-DLV_CONF_INCLUDE_SIMPLE=1',
        '-Ismk-p4/managed_components/lvgl__lvgl', '-Ismk-p4/ui-preview',
        '-Isrc', '-Ismk-p4/main', '-Ismk-p4/components/amy_runtime', '-Ismk-p4/tests',
        '-Ishared/smk_protocol', '-Ismk-p4/tests/nvs_mocks', '-Ismk-p4/components/ui_blueprint/include'
    )
    $wrap = @('-Wl,--wrap=malloc', '-Wl,--wrap=calloc', '-Wl,--wrap=realloc', '-Wl,--wrap=free')

    $testsToRun = @()
    if ($TestName -eq "all" -or $TestName -eq "gate_a") { $testsToRun += "gate_a_fx_init_test" }
    if ($TestName -eq "all" -or $TestName -eq "gate_b_tx") { $testsToRun += "gate_b_transaction_test" }
    if ($TestName -eq "all" -or $TestName -eq "gate_c_save") { $testsToRun += "gate_c_save_semantics_test" }
    if ($TestName -eq "all" -or $TestName -eq "gate_d_unsaved") { $testsToRun += "gate_d_unsaved_fx_test" }
    if ($TestName -eq "all" -or $TestName -eq "gate_f_stale") { $testsToRun += "gate_f_stale_fx_test" }
    if ($TestName -eq "all" -or $TestName -eq "gate_g_ui") { $testsToRun += "gate_g_ui_confirmation_test" }
    if ($TestName -eq "all" -or $TestName -eq "gate_h_codec") { $testsToRun += "gate_h_codec_hardening_test" }
    if ($TestName -eq "all" -or $TestName -eq "gate_b") { $testsToRun += "gate_b_fx_test" }
    if ($TestName -eq "all" -or $TestName -eq "chorus") { $testsToRun += "chorus_test" }
    if ($TestName -eq "all" -or $TestName -eq "reverb") { $testsToRun += "reverb_test" }
    if ($TestName -eq "all" -or $TestName -eq "delay")  { $testsToRun += "delay_test" }
    if ($TestName -eq "all" -or $TestName -eq "eq")     { $testsToRun += "eq_test" }
    if ($TestName -eq "all" -or $TestName -eq "persistence") { $testsToRun += "fx_preset_persistence_test" }

    foreach ($t in $testsToRun) {
        $srcPath = "smk-p4/tests/$t.cpp"
        if (!(Test-Path $srcPath)) { continue }
        Write-Host "Compiling and running $t..."
        $extra = @()
        if ($t -eq "gate_g_ui_confirmation_test") {
            $extra = @(
                'smk-p4/main/ui_blueprint_performance_adapter.cpp',
                'smk-p4/components/ui_blueprint/src/ui_model.cpp',
                'smk-p4/components/ui_blueprint/src/ui_parameters.cpp',
                'smk-p4/components/ui_blueprint/src/ui_theme.cpp',
                'build/ui_blueprint_host/liblvgl_host.a'
            )
        }
        & g++ @flags $srcPath smk-p4/components/amy_runtime/amy_runtime.cpp smk-p4/tests/heap_probe.cpp @objects @extra @wrap -lm -o "build/fx012/$t.exe"
        if ($LASTEXITCODE) { throw "Compilation failed: $t" }
        & "build/fx012/$t.exe"
        if ($LASTEXITCODE) { throw "Test failed: $t" }
        Write-Host "PASS: $t"
    }
} finally {
    Pop-Location
}
