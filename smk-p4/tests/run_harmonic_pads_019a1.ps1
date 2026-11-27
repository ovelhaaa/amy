param([switch]$Clean, [switch]$Regressions)
$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $repo
try {
    $smkP4 = Join-Path $repo 'smk-p4'
    $lvgl = Join-Path $smkP4 'managed_components/lvgl__lvgl'
    if (-not (Test-Path $lvgl)) {
        throw "Missing LVGL dependency at $lvgl"
    }
    $previewDir = Join-Path $smkP4 'ui-preview'
    $uiCompDir = Join-Path $smkP4 'components/ui_blueprint'
    $uiIncDir = Join-Path $uiCompDir 'include'
    $amyRuntimeDir = Join-Path $smkP4 'components/amy_runtime'
    $amySrcDir = Join-Path $repo 'src'
    $shotsDir = Join-Path $smkP4 'docs/harmonic_pads_019a1_shots'

    $buildDir = Join-Path $repo 'build/harmonic_pads_019a1'
    if ($Clean -and (Test-Path $buildDir)) {
        $resolvedBuild = (Resolve-Path -LiteralPath $buildDir).Path
        $expectedBuild = [IO.Path]::GetFullPath((Join-Path $repo 'build/harmonic_pads_019a1'))
        if ($resolvedBuild -ne $expectedBuild) { throw "Unsafe clean target: $resolvedBuild" }
        Remove-Item -LiteralPath $resolvedBuild -Recurse -Force
    }

    $lvglObjDir = Join-Path $buildDir 'lvgl_obj'
    $amyObjDir = Join-Path $buildDir 'amy_obj'
    $uiObjDir = Join-Path $buildDir 'ui_obj'
    New-Item -ItemType Directory -Force -Path $shotsDir, $buildDir, $lvglObjDir, $amyObjDir, $uiObjDir | Out-Null

    $gitSha = (& git rev-parse HEAD 2>$null)
    if (-not $gitSha) { $gitSha = 'unknown' }
    $gitBranch = (& git rev-parse --abbrev-ref HEAD 2>$null)
    if (-not $gitBranch) { $gitBranch = 'unknown' }
    $gitDirtyAtStart = @(& git status --porcelain)

    $commonFlags = @(
        '-DSMKP4_VOICES=8',
        '-DSMKP4_SOUND_LIBRARY=1',
        '-DSMKP4_UNIFIED_ENGINE=1',
        '-DSMKP4_HARMONY=1',
        '-DSMKP4_FULL_FACTORY_LIBRARY=1',
        '-DSMKP4_SOUND_EDITOR=1',
        '-DSMKP4_LIVE_PERFORMANCE=1',
        '-DSMKP4_MACRO_ROUTING=1',
        '-DSMKP4_SMK25_0142=1',
        '-DSMKP4_ARP_015=1',
        '-DSMKP4_DELAY_SYNC_0151=1',
        '-DSMKP4_PERF_LOCK_0152=1',
        '-DSMKP4_STEP_SEQUENCER=1',
        '-DSMKP4_PATTERN_LIBRARY=1',
        '-DSMKP4_PERFORMANCE_SCENES=1',
        '-DLV_CONF_INCLUDE_SIMPLE=1',
        '-DAMY_NO_MINIAUDIO',
        '-DAMY_SAMPLE_RATE=48000',
        '-DAMY_BLOCK_SIZE=256',
        '-DBLOCK_SIZE_BITS=8',
        '-DAMY_PATCH_CHECKED_RESOURCE_ADMISSION=1',
        '-DAMY_EXCLUSIVE_FM_PHASE_RESTART=1',
        '-fno-strict-aliasing'
    )
    foreach ($tool in @('gcc', 'g++', 'ar', 'python', 'git')) {
        if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) { throw "Missing host dependency: $tool" }
    }
    & python -c 'from PIL import Image'
    if ($LASTEXITCODE -ne 0) { throw 'Missing Python dependency: Pillow' }

    $inputDirs = @($uiCompDir, $amyRuntimeDir, $amySrcDir, $previewDir,
                  (Join-Path $smkP4 'main'), (Join-Path $repo 'shared/smk_protocol'),
                  (Join-Path $smkP4 'tests'), (Join-Path $lvgl 'src'))
    $inputs = @($inputDirs | ForEach-Object {
        Get-ChildItem -LiteralPath $_ -Recurse -File | Where-Object { $_.Extension -in @('.h', '.hpp', '.c', '.cpp') }
    }) + @(Get-ChildItem -LiteralPath $lvgl -File -Filter '*.h')
    $inputs = @($inputs | Sort-Object FullName -Unique)
    $inputHashes = @($inputs | ForEach-Object {
        $_.FullName.Substring($repo.Length) + ':' + (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    })
    $compiler = @(& gcc --version; & g++ --version; & ar --version) -join "`n"
    $configStamp = ($commonFlags -join ' ') + " | pool=128 | O2 | C99/gnu11/C++20 | Wall/Wextra/Werror`n" +
                   $compiler + "`n" + ($inputHashes -join "`n") + "`n" +
                   (Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash
    $stampFile = Join-Path $buildDir '.build_config'
    $forceRebuild = $Clean -or (-not (Test-Path $stampFile)) -or ((Get-Content $stampFile -Raw).Trim() -ne $configStamp)
    if (Test-Path $stampFile) { Remove-Item -LiteralPath $stampFile }

    Write-Host "=== SMK-P4 0.19-A.1 Host Test Suite ==="
    Write-Host "Branch : $gitBranch"
    Write-Host "Commit : $gitSha"
    Write-Host "Build  : $buildDir (Clean=$Clean, ForceRebuild=$forceRebuild)"

    & g++ -std=c++20 -O2 -Wall -Wextra -Werror "-I$amyRuntimeDir" (Join-Path $smkP4 'tests/harmonic_pads_music_test.cpp') -o (Join-Path $buildDir 'music.exe')
    if ($LASTEXITCODE) { throw 'Pad music compilation failed' }
    & (Join-Path $buildDir 'music.exe')
    if ($LASTEXITCODE) { throw 'Pad music tests failed' }

    # 1. Build LVGL 9.3.0
    $lvglSrcDir = Join-Path $lvgl 'src'
    $lvglCFiles = @(Get-ChildItem -Path $lvglSrcDir -Recurse -Filter '*.c' | Where-Object {
        $_.FullName -notmatch '\\drivers\\' -and
        $_.FullName -notmatch '\\osal\\' -and
        $_.FullName -notmatch '\\libs\\' -and
        $_.FullName -notmatch '\\draw\\nxp\\' -and
        $_.FullName -notmatch '\\draw\\renesas\\' -and
        $_.FullName -notmatch '\\draw\\sdl\\' -and
        $_.FullName -notmatch '\\draw\\vg_lite\\' -and
        $_.FullName -notmatch '\\draw\\dma2d\\' -and
        $_.FullName -notmatch '\\draw\\espressif\\' -and
        $_.FullName -notmatch '\\draw\\nema_gfx\\' -and
        $_.FullName -notmatch '\\draw\\opengles\\' -and
        $_.FullName -notmatch '\\draw\\sw\\arm2d\\' -and
        $_.FullName -notmatch '\\draw\\sw\\blend\\neon\\' -and
        $_.FullName -notmatch '\\draw\\sw\\blend\\helium\\' -and
        $_.FullName -notmatch '\\draw\\sw\\blend\\riscv_v\\'
    })
    $lvglCFiles += Get-Item (Join-Path $lvglSrcDir 'osal\lv_os.c')
    $lvglCFiles += Get-Item (Join-Path $lvglSrcDir 'osal\lv_os_none.c')
    $lvglCFiles += Get-Item (Join-Path $lvglSrcDir 'libs\bin_decoder\lv_bin_decoder.c')

    $lvConfTime = (Get-Item (Join-Path $previewDir 'lv_conf.h')).LastWriteTime
    $lvglObjs = @()
    $lvglCompiled = 0
    foreach ($file in $lvglCFiles) {
        $relPath = $file.FullName.Substring($lvglSrcDir.Length + 1).Replace('\', '_').Replace('/', '_')
        $obj = Join-Path $lvglObjDir ($relPath + '.o')
        $lvglObjs += $obj
        if ($forceRebuild -or (-not (Test-Path $obj)) -or ($file.LastWriteTime -gt (Get-Item $obj).LastWriteTime) -or ($lvConfTime -gt (Get-Item $obj).LastWriteTime)) {
            & gcc -O2 -std=c99 -DLV_CONF_INCLUDE_SIMPLE=1 "-I$previewDir" "-I$lvgl" "-I$lvglSrcDir" -c $file.FullName -o $obj
            if ($LASTEXITCODE -ne 0) { throw "Failed compiling LVGL file: $($file.FullName)" }
            $lvglCompiled++
        }
    }
    $lvglArchive = Join-Path $buildDir 'liblvgl_host.a'
    if ($lvglCompiled -gt 0 -or -not (Test-Path $lvglArchive)) {
        if (Test-Path $lvglArchive) { Remove-Item $lvglArchive }
        $batchSize = 60
        for ($i = 0; $i -lt $lvglObjs.Count; $i += $batchSize) {
            $batch = $lvglObjs[$i..([Math]::Min($i + $batchSize - 1, $lvglObjs.Count - 1))]
            & ar rcs $lvglArchive $batch
            if ($LASTEXITCODE -ne 0) { throw 'Failed archiving liblvgl_host.a' }
        }
    }
    Write-Host "[1/4] LVGL ready ($($lvglObjs.Count) objects, $lvglCompiled compiled)"

    # 2. Build AMY C objects from CMakeLists.txt
    $cmake = Get-Content (Join-Path $amyRuntimeDir 'CMakeLists.txt') -Raw
    $amySources = [regex]::Matches($cmake, '\$\{AMY_SRC_DIR\}/([^"\r\n]+\.c)') | ForEach-Object { $_.Groups[1].Value }
    if ($amySources.Count -lt 10) { throw 'Failed to parse AMY C sources from CMakeLists.txt' }
    $amyHeaderTime = (Get-ChildItem -Path $amySrcDir -Filter '*.h' | Sort-Object LastWriteTime -Descending | Select-Object -First 1).LastWriteTime
    $amyObjects = @()
    $amyCompiled = 0
    foreach ($src in $amySources) {
        $srcPath = Join-Path $amySrcDir $src
        if (-not (Test-Path $srcPath)) { throw "Missing AMY C source: $srcPath" }
        $obj = Join-Path $amyObjDir (($src -replace '[\\/]', '_') + '.o')
        $amyObjects += $obj
        if ($forceRebuild -or (-not (Test-Path $obj)) -or ((Get-Item $srcPath).LastWriteTime -gt (Get-Item $obj).LastWriteTime) -or ($amyHeaderTime -gt (Get-Item $obj).LastWriteTime)) {
            & gcc -std=gnu11 -O2 -Wall -Wextra @commonFlags "-I$amySrcDir" "-I$amyRuntimeDir" -c $srcPath -o $obj
            if ($LASTEXITCODE -ne 0) { throw "AMY compile failed: $src" }
            $amyCompiled++
        }
    }
    Write-Host "[2/4] AMY ready ($($amyObjects.Count) objects, $amyCompiled compiled)"

    # 3. Build ui_blueprint C++ objects
    $uiCppFiles = @()
    $uiCppFiles += Get-ChildItem -Path (Join-Path $uiCompDir 'src') -Filter '*.cpp'
    $uiCppFiles += Get-ChildItem -Path (Join-Path $uiCompDir 'widgets') -Filter '*.cpp'
    $uiCppFiles += Get-ChildItem -Path (Join-Path $uiCompDir 'screens') -Filter '*.cpp'

    $headerDirs = @($uiIncDir, $amyRuntimeDir, $previewDir, (Join-Path $smkP4 'main'), (Join-Path $repo 'shared/smk_protocol'))
    $latestHeaderTime = ($headerDirs | ForEach-Object { Get-ChildItem -Path $_ -Filter '*.h' -ErrorAction SilentlyContinue } | Sort-Object LastWriteTime -Descending | Select-Object -First 1).LastWriteTime

    $includeArgs = @(
        "-I$lvgl",
        "-I$lvglSrcDir",
        "-I$smkP4/tests/nvs_mocks",
        "-I$previewDir",
        "-I$uiIncDir",
        "-I$smkP4/main",
        "-I$repo/shared/smk_protocol",
        "-I$amyRuntimeDir",
        "-I$amySrcDir"
    )

    $uiObjects = @()
    $uiCompiled = 0
    foreach ($file in $uiCppFiles) {
        $obj = Join-Path $uiObjDir ($file.BaseName + '.o')
        $uiObjects += $obj
        if ($forceRebuild -or (-not (Test-Path $obj)) -or ($file.LastWriteTime -gt (Get-Item $obj).LastWriteTime) -or ($latestHeaderTime -gt (Get-Item $obj).LastWriteTime)) {
            & g++ -std=c++20 -O2 -Wall -Wextra -Werror @commonFlags @includeArgs -c $file.FullName -o $obj
            if ($LASTEXITCODE -ne 0) { throw "UI Blueprint compile failed: $($file.FullName)" }
            $uiCompiled++
        }
    }
    Write-Host "[3/4] UI Blueprint ready ($($uiObjects.Count) objects, $uiCompiled compiled)"

    # 4. Link and run harmonic_pads_019a1_test.cpp
    & g++ -std=c++20 -O2 -Wall -Wextra -Werror @commonFlags @includeArgs `
        (Join-Path $smkP4 'tests/harmonic_pads_019a1_test.cpp') `
        (Join-Path $smkP4 'tests/harmonic_pads_owner_test.cpp') `
        (Join-Path $smkP4 'tests/heap_probe.cpp') `
        '-Wl,--wrap=malloc' '-Wl,--wrap=calloc' '-Wl,--wrap=realloc' '-Wl,--wrap=free' `
        (Join-Path $smkP4 'main/ui_blueprint_performance_adapter.cpp') `
        (Join-Path $amyRuntimeDir 'amy_runtime.cpp') `
        @uiObjects @amyObjects $lvglArchive -lm -o (Join-Path $buildDir 'test.exe')
    if ($LASTEXITCODE -ne 0) { throw '0.19-A.1 test compilation failed' }

    Write-Host "[4/4] Running test.exe ..."
    & (Join-Path $buildDir 'test.exe') $shotsDir
    if ($LASTEXITCODE -ne 0) { throw '0.19-A.1 test failed' }

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
    if ($LASTEXITCODE -ne 0) { throw 'PNG conversion failed' }
    if ($Regressions) {
        foreach ($test in @('ui_018c3a_test', 'ui_018c3_test', 'ui_018c2_test', 'ui_018c1_integration_test', 'ui_018c1a_hardening_test', 'sound_editor_ui_test')) {
            $testSource = Join-Path $smkP4 "tests/$test.cpp"
            $extra = @()
            if ($test -eq 'sound_editor_ui_test') {
                $extra = @((Join-Path $smkP4 'tests/smk25_0142_ui_test.cpp'),
                           (Join-Path $smkP4 'tests/heap_probe.cpp'),
                           '-Wl,--wrap=malloc', '-Wl,--wrap=calloc', '-Wl,--wrap=realloc', '-Wl,--wrap=free')
                New-Item -ItemType Directory -Force (Join-Path $repo 'build/editor011') | Out-Null
            }
            $binary = Join-Path $buildDir "$test.exe"
            & g++ -std=c++20 -O2 -Wall -Wextra -Werror @commonFlags @includeArgs $testSource @extra `
                (Join-Path $smkP4 'main/ui_blueprint_performance_adapter.cpp') `
                (Join-Path $amyRuntimeDir 'amy_runtime.cpp') @uiObjects @amyObjects $lvglArchive -lm -o $binary
            if ($LASTEXITCODE -ne 0) { throw "Regression compile failed: $test" }
            $regressionShots = Join-Path $buildDir $test
            New-Item -ItemType Directory -Force $regressionShots | Out-Null
            & $binary $regressionShots
            if ($LASTEXITCODE -ne 0) { throw "Regression failed: $test" }
        }
    }
    $finalHashes = @($inputs | ForEach-Object {
        $_.FullName.Substring($repo.Length) + ':' + (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    })
    if (($finalHashes -join "`n") -ne ($inputHashes -join "`n")) {
        throw 'Build inputs changed during validation; rerun before declaring PASS'
    }
    if ($gitSha -ne (& git rev-parse HEAD)) {
        throw 'HEAD changed during validation; rerun to record the exact tested commit'
    }
    $manifest = [ordered]@{
        branch = $gitBranch; commit = $gitSha; dirty = @(& git status --porcelain)
        dirty_at_start = $gitDirtyAtStart
        compiler = $compiler; flags = $commonFlags; clean = [bool]$Clean
        source_hashes = $inputHashes; lvgl_objects = $lvglObjs.Count
        amy_objects = $amyObjects.Count; ui_objects = $uiObjects.Count
        result = 'PASS'; regressions = [bool]$Regressions; timestamp_utc = [DateTime]::UtcNow.ToString('o')
    }
    $manifest | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $buildDir 'test_manifest.json') -Encoding UTF8
    Set-Content -LiteralPath $stampFile -Value $configStamp -Encoding UTF8
} finally {
    Pop-Location
}
