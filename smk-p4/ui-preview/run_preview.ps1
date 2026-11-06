param([ValidateSet(96,128)][int]$PoolKB=128)
$ErrorActionPreference = "Stop"

$PreviewDir = $PSScriptRoot
$SmkP4Dir = Split-Path -Parent $PreviewDir
$RepoRoot = Split-Path -Parent $SmkP4Dir

$BuildDir = Join-Path $RepoRoot "build\ui_blueprint_host"
$LvglObjDir = Join-Path $BuildDir "lvgl_obj"
$UiObjDir = Join-Path $BuildDir "ui_obj"
$ShotsDir = Join-Path $RepoRoot "build\ui_preview_shots"
if($PoolKB -ne 128) {
    $BuildDir = Join-Path $RepoRoot "build\ui_blueprint_host_$PoolKB"
    $LvglObjDir = Join-Path $BuildDir "lvgl_obj"
    $UiObjDir = Join-Path $BuildDir "ui_obj"
    $ShotsDir = Join-Path $RepoRoot "build\ui_preview_shots_$PoolKB"
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
New-Item -ItemType Directory -Force -Path $LvglObjDir | Out-Null
New-Item -ItemType Directory -Force -Path $UiObjDir | Out-Null
New-Item -ItemType Directory -Force -Path $ShotsDir | Out-Null

# Locate LVGL 9.3.0 source tree (either in current worktree or main repo)
$LvglRoot = Join-Path $SmkP4Dir "managed_components\lvgl__lvgl"
if (-not (Test-Path $LvglRoot)) {
    # Fallback to main repository managed_components if running from a linked worktree
    $MainRepoRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $RepoRoot))
    $Candidate = Join-Path $MainRepoRoot "smk-p4\managed_components\lvgl__lvgl"
    if (Test-Path $Candidate) {
        $LvglRoot = $Candidate
    } else {
        throw "Could not find managed_components/lvgl__lvgl at $LvglRoot or $Candidate"
    }
}

Write-Host "[1/4] Compiling LVGL 9.3.0 core C files from $LvglRoot ..."
$LvglSrcDir = Join-Path $LvglRoot "src"
$LvglCFiles = Get-ChildItem -Path $LvglSrcDir -Recurse -Filter "*.c" | Where-Object {
    # Exclude hardware/OS specific drivers not used by software RGB565 buffer rendering
    $_.FullName -notmatch "\\drivers\\" -and
    $_.FullName -notmatch "\\osal\\" -and
    $_.FullName -notmatch "\\libs\\" -and
    $_.FullName -notmatch "\\draw\\nxp\\" -and
    $_.FullName -notmatch "\\draw\\renesas\\" -and
    $_.FullName -notmatch "\\draw\\sdl\\" -and
    $_.FullName -notmatch "\\draw\\vg_lite\\" -and
    $_.FullName -notmatch "\\draw\\dma2d\\" -and
    $_.FullName -notmatch "\\draw\\espressif\\" -and
    $_.FullName -notmatch "\\draw\\nema_gfx\\" -and
    $_.FullName -notmatch "\\draw\\opengles\\" -and
    $_.FullName -notmatch "\\draw\\sw\\arm2d\\" -and
    $_.FullName -notmatch "\\draw\\sw\\blend\\neon\\" -and
    $_.FullName -notmatch "\\draw\\sw\\blend\\helium\\" -and
    $_.FullName -notmatch "\\draw\\sw\\blend\\riscv_v\\"
}

# Include lv_os.c, lv_os_none.c, and lv_bin_decoder.c
$OsCoreFile = Get-Item (Join-Path $LvglSrcDir "osal\lv_os.c")
$OsNoneFile = Get-Item (Join-Path $LvglSrcDir "osal\lv_os_none.c")
$BinDecoderFile = Get-Item (Join-Path $LvglSrcDir "libs\bin_decoder\lv_bin_decoder.c")
$AllLvglFiles = @($LvglCFiles) + @($OsCoreFile, $OsNoneFile, $BinDecoderFile)

$LvglObjFiles = @()
$CompiledCount = 0
foreach ($file in $AllLvglFiles) {
    $relPath = $file.FullName.Substring($LvglSrcDir.Length + 1).Replace("\", "_").Replace("/", "_")
    $objFile = Join-Path $LvglObjDir ($relPath + ".o")
    $LvglObjFiles += $objFile
    if (-not (Test-Path $objFile) -or ($file.LastWriteTime -gt (Get-Item $objFile).LastWriteTime) -or ((Get-Item (Join-Path $PreviewDir "lv_conf.h")).LastWriteTime -gt (Get-Item $objFile).LastWriteTime)) {
        & gcc "-DSMKP4_LVGL_POOL_KB=$PoolKB" -O2 -std=c99 -DLV_CONF_INCLUDE_SIMPLE=1 `
            "-I$PreviewDir" `
            "-I$LvglRoot" `
            "-I$LvglSrcDir" `
            -c $file.FullName -o $objFile
        if ($LASTEXITCODE -ne 0) { throw "Failed compiling LVGL file: $($file.FullName)" }
        $CompiledCount++
    }
}
$LvglArchive = Join-Path $BuildDir "liblvgl_host.a"
if ($CompiledCount -gt 0 -or -not (Test-Path $LvglArchive)) {
    if (Test-Path $LvglArchive) { Remove-Item $LvglArchive }
    # Archive in batches to avoid Windows command-line length limits
    $BatchSize = 60
    for ($i = 0; $i -lt $LvglObjFiles.Count; $i += $BatchSize) {
        $batch = $LvglObjFiles[$i..([Math]::Min($i + $BatchSize - 1, $LvglObjFiles.Count - 1))]
        & ar rcs $LvglArchive $batch
        if ($LASTEXITCODE -ne 0) { throw "Failed archiving liblvgl_host.a" }
    }
}
Write-Host "      LVGL 9.3.0 ready ($($LvglObjFiles.Count) objects, $CompiledCount compiled)."

Write-Host "[2/4] Compiling SMK-P4 UI Blueprint 1.0 C++ component ..."
$UiCompDir = Join-Path $SmkP4Dir "components\ui_blueprint"
$UiIncDir = Join-Path $UiCompDir "include"
$UiCppFiles = @()
$UiCppFiles += Get-ChildItem -Path (Join-Path $UiCompDir "src") -Filter "*.cpp"
$UiCppFiles += Get-ChildItem -Path (Join-Path $UiCompDir "widgets") -Filter "*.cpp"
$UiCppFiles += Get-ChildItem -Path (Join-Path $UiCompDir "screens") -Filter "*.cpp"

$LatestHeaderTime = (Get-ChildItem -Path $UiIncDir -Filter "*.h" | Sort-Object LastWriteTime -Descending | Select-Object -First 1).LastWriteTime
$UiObjFiles = @()
foreach ($file in $UiCppFiles) {
    $objFile = Join-Path $UiObjDir ($file.BaseName + ".o")
    $UiObjFiles += $objFile
    if (-not (Test-Path $objFile) -or ($file.LastWriteTime -gt (Get-Item $objFile).LastWriteTime) -or ($LatestHeaderTime -gt (Get-Item $objFile).LastWriteTime)) {
        & g++ "-DSMKP4_LVGL_POOL_KB=$PoolKB" -O2 -std=c++20 -Wall -Wextra -Werror -DLV_CONF_INCLUDE_SIMPLE=1 `
            "-I$PreviewDir" `
            "-I$UiIncDir" `
            "-I$LvglRoot" `
            "-I$LvglSrcDir" `
            -c $file.FullName -o $objFile
        if ($LASTEXITCODE -ne 0) { throw "Failed compiling UI Blueprint file: $($file.FullName)" }
    }
}

Write-Host "[3/4] Building and running blueprint_test_suite.exe ..."
$TestSuiteSrc = Join-Path $UiCompDir "tests\blueprint_test_suite.cpp"
$TestSuiteExe = Join-Path $BuildDir "blueprint_test_suite.exe"
& g++ "-DSMKP4_LVGL_POOL_KB=$PoolKB" -O2 -std=c++20 -Wall -Wextra -Werror -DLV_CONF_INCLUDE_SIMPLE=1 `
    "-I$PreviewDir" `
    "-I$UiIncDir" `
    "-I$LvglRoot" `
    "-I$LvglSrcDir" `
    $TestSuiteSrc $UiObjFiles $LvglArchive -o $TestSuiteExe
if ($LASTEXITCODE -ne 0) { throw "Failed linking blueprint_test_suite.exe" }

& $TestSuiteExe
if ($LASTEXITCODE -ne 0) { throw "blueprint_test_suite.exe FAILED" }

Write-Host "[4/4] Building and running preview_main.exe (800x480 framebuffer capture) ..."
$PreviewSrc = Join-Path $PreviewDir "preview_main.cpp"
$PreviewExe = Join-Path $BuildDir "preview_main.exe"
& g++ "-DSMKP4_LVGL_POOL_KB=$PoolKB" -O2 -std=c++20 -Wall -Wextra -Werror -DLV_CONF_INCLUDE_SIMPLE=1 `
    "-I$PreviewDir" `
    "-I$UiIncDir" `
    "-I$LvglRoot" `
    "-I$LvglSrcDir" `
    $PreviewSrc $UiObjFiles $LvglArchive -o $PreviewExe
if ($LASTEXITCODE -ne 0) { throw "Failed linking preview_main.exe" }

& $PreviewExe $ShotsDir
if ($LASTEXITCODE -ne 0) { throw "preview_main.exe FAILED" }

# Convert PPM screenshots to PNG so they can be inspected directly
$ConvertPy = Join-Path $BuildDir "ppm_to_png.py"
@"
import sys, os, glob, struct, zlib

def ppm_to_png(ppm_path, png_path):
    with open(ppm_path, 'rb') as f:
        magic = f.readline().strip()
        assert magic == b'P6'
        dims = f.readline().strip().split()
        w, h = int(dims[0]), int(dims[1])
        maxval = int(f.readline().strip())
        raw = f.read(w * h * 3)
    raw_scanlines = bytearray()
    stride = w * 3
    for y in range(h):
        raw_scanlines.append(0)
        raw_scanlines.extend(raw[y * stride : (y + 1) * stride])
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)
    ihdr = struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', ihdr) + chunk(b'IDAT', zlib.compress(bytes(raw_scanlines), 6)) + chunk(b'IEND', b'')
    with open(png_path, 'wb') as f:
        f.write(png)

shots_dir = sys.argv[1]
for ppm in sorted(glob.glob(os.path.join(shots_dir, '*.ppm'))):
    png = ppm[:-4] + '.png'
    ppm_to_png(ppm, png)
    os.remove(ppm)
print(f"Converted PPM captures to PNG in {shots_dir}")
"@ | Set-Content -Path $ConvertPy -Encoding UTF8

& python $ConvertPy $ShotsDir
if ($LASTEXITCODE -ne 0) { throw "PPM to PNG conversion failed" }
