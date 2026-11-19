param(
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$PreviewDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$SmkP4Dir = Split-Path -Parent $PreviewDir
$RepoRoot = Split-Path -Parent $SmkP4Dir
$LvglRoot = Join-Path $SmkP4Dir "managed_components\lvgl__lvgl"
$LvglSrcDir = Join-Path $LvglRoot "src"
$BuildDir = Join-Path $RepoRoot "build\ui_blueprint_host"
$UiObjDir = Join-Path $BuildDir "ui_obj"
$ShotsDir = Join-Path $SmkP4Dir "docs\ui_018a_shots"
$AmyBuildDir = Join-Path $RepoRoot "build\harmony09"

if ($Clean -and (Test-Path $BuildDir)) {
    Remove-Item -Recurse -Force $BuildDir
}

New-Item -ItemType Directory -Force -Path $BuildDir, $UiObjDir, $ShotsDir | Out-Null

# Reuse LVGL archive & UI Blueprint component build via run_preview baseline script if not present
$LvglArchive = Join-Path $BuildDir "liblvgl_host.a"
if (-not (Test-Path $LvglArchive)) {
    & (Join-Path $PreviewDir "run_preview.ps1") -LvglOnly
    if ($LASTEXITCODE -ne 0) { throw "Failed preparing LVGL host library" }
}

$AmyObjFiles = @()
if (Test-Path $AmyBuildDir) {
    $AmyObjFiles = @(Get-ChildItem -Path $AmyBuildDir -Filter "*.c.o" | ForEach-Object { $_.FullName })
}
if ($AmyObjFiles.Count -lt 20) {
    & (Join-Path $SmkP4Dir "tests\run_harmony.ps1") -ReuseObjects
    if ($LASTEXITCODE -ne 0) { throw "Failed preparing AMY C objects" }
    $AmyObjFiles = @(Get-ChildItem -Path $AmyBuildDir -Filter "*.c.o" | ForEach-Object { $_.FullName })
}

$UiCompDir = Join-Path $SmkP4Dir "components\ui_blueprint"
$UiIncDir = Join-Path $UiCompDir "include"
$AmyRuntimeDir = Join-Path $SmkP4Dir "components\amy_runtime"
$AmySrcDir = Join-Path $RepoRoot "src"
$MainDir = Join-Path $SmkP4Dir "main"
$ProtocolDir = Join-Path $RepoRoot "shared\smk_protocol"
$NvsMocksDir = Join-Path $SmkP4Dir "tests\nvs_mocks"
$TestsDir = Join-Path $SmkP4Dir "tests"

$AmyFlags = @(
    "-DAMY_NO_MINIAUDIO",
    "-DAMY_SAMPLE_RATE=48000",
    "-DAMY_BLOCK_SIZE=256",
    "-DBLOCK_SIZE_BITS=8",
    "-DAMY_PATCH_CHECKED_RESOURCE_ADMISSION=1",
    "-DAMY_EXCLUSIVE_FM_PHASE_RESTART=1",
    "-DSMKP4_VOICES=8",
    "-DSMKP4_SOUND_LIBRARY=1",
    "-DSMKP4_UNIFIED_ENGINE=1",
    "-DSMKP4_HARMONY=1",
    "-DSMKP4_FULL_FACTORY_LIBRARY=1",
    "-DSMKP4_SOUND_EDITOR=1"
)

Write-Host "[1/5] Ensuring UI Blueprint + Adapter + Runtime objects ..."
$AmyRuntimeSrc = Join-Path $AmyRuntimeDir "amy_runtime.cpp"
$AmyRuntimeObj = Join-Path $UiObjDir "amy_runtime.o"
if (-not (Test-Path $AmyRuntimeObj) -or ((Get-Item $AmyRuntimeSrc).LastWriteTime -gt (Get-Item $AmyRuntimeObj).LastWriteTime)) {
    & g++ @AmyFlags -O2 -std=c++20 -Wall -Wextra `
        "-I$AmySrcDir" "-I$AmyRuntimeDir" "-I$MainDir" "-I$ProtocolDir" "-I$NvsMocksDir" `
        -c $AmyRuntimeSrc -o $AmyRuntimeObj
    if ($LASTEXITCODE -ne 0) { throw "Failed compiling amy_runtime.cpp" }
}

$AdapterSrc = Join-Path $MainDir "ui_blueprint_performance_adapter.cpp"
$AdapterObj = Join-Path $UiObjDir "ui_blueprint_performance_adapter.o"
if (-not (Test-Path $AdapterObj) -or ((Get-Item $AdapterSrc).LastWriteTime -gt (Get-Item $AdapterObj).LastWriteTime)) {
    & g++ @AmyFlags "-DSMKP4_LVGL_POOL_KB=128" -O2 -std=c++20 -Wall -Wextra -DLV_CONF_INCLUDE_SIMPLE=1 `
        "-I$PreviewDir" "-I$UiIncDir" "-I$AmyRuntimeDir" "-I$AmySrcDir" "-I$MainDir" "-I$ProtocolDir" "-I$NvsMocksDir" "-I$LvglRoot" "-I$LvglSrcDir" `
        -c $AdapterSrc -o $AdapterObj
    if ($LASTEXITCODE -ne 0) { throw "Failed compiling ui_blueprint_performance_adapter.cpp" }
}

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
        & g++ "-DSMKP4_LVGL_POOL_KB=128" -O2 -std=c++20 -Wall -Wextra -Werror -DLV_CONF_INCLUDE_SIMPLE=1 `
            "-I$PreviewDir" "-I$UiIncDir" "-I$AmyRuntimeDir" "-I$AmySrcDir" "-I$LvglRoot" "-I$LvglSrcDir" `
            -c $file.FullName -o $objFile
        if ($LASTEXITCODE -ne 0) { throw "Failed compiling $($file.Name)" }
    }
}

Write-Host "[2/5] Compiling ui_prototype_018a.cpp ..."
$ProtoSrc = Join-Path $PreviewDir "ui_prototype_018a.cpp"
$ProtoObj = Join-Path $BuildDir "ui_prototype_018a.o"
& g++ "-DSMKP4_LVGL_POOL_KB=128" -O2 -std=c++20 -Wall -Wextra -Werror -DLV_CONF_INCLUDE_SIMPLE=1 `
    "-I$PreviewDir" "-I$UiIncDir" "-I$AmyRuntimeDir" "-I$AmySrcDir" "-I$LvglRoot" "-I$LvglSrcDir" `
    -c $ProtoSrc -o $ProtoObj
if ($LASTEXITCODE -ne 0) { throw "Failed compiling ui_prototype_018a.cpp" }

Write-Host "[3/5] Building and running test_018a_suite.exe ..."
$Test018Src = Join-Path $PreviewDir "test_018a_suite.cpp"
$Test018Exe = Join-Path $BuildDir "test_018a_suite.exe"
& g++ @AmyFlags "-DSMKP4_LVGL_POOL_KB=128" -O2 -std=c++20 -Wall -Wextra -Werror -DLV_CONF_INCLUDE_SIMPLE=1 `
    "-I$PreviewDir" "-I$UiIncDir" "-I$AmyRuntimeDir" "-I$AmySrcDir" "-I$MainDir" "-I$ProtocolDir" "-I$NvsMocksDir" "-I$TestsDir" "-I$LvglRoot" "-I$LvglSrcDir" `
    $Test018Src $ProtoObj $AdapterObj $UiObjFiles $AmyRuntimeObj $AmyObjFiles $LvglArchive -lm -o $Test018Exe
if ($LASTEXITCODE -ne 0) { throw "Failed linking test_018a_suite.exe" }

& $Test018Exe | Tee-Object -FilePath (Join-Path $BuildDir "test_018a.log")
if ($LASTEXITCODE -ne 0) { throw "test_018a_suite.exe FAILED" }

Write-Host "[4/5] Building and running preview_018a_main.exe ..."
$Prev018Src = Join-Path $PreviewDir "preview_018a_main.cpp"
$Prev018Exe = Join-Path $BuildDir "preview_018a_main.exe"
& g++ @AmyFlags "-DSMKP4_LVGL_POOL_KB=128" -O2 -std=c++20 -Wall -Wextra -Werror -DLV_CONF_INCLUDE_SIMPLE=1 `
    "-I$PreviewDir" "-I$UiIncDir" "-I$AmyRuntimeDir" "-I$AmySrcDir" "-I$MainDir" "-I$ProtocolDir" "-I$NvsMocksDir" "-I$TestsDir" "-I$LvglRoot" "-I$LvglSrcDir" `
    $Prev018Src $ProtoObj $AdapterObj $UiObjFiles $AmyRuntimeObj $AmyObjFiles $LvglArchive -lm -o $Prev018Exe
if ($LASTEXITCODE -ne 0) { throw "Failed linking preview_018a_main.exe" }

& $Prev018Exe $ShotsDir | Tee-Object -FilePath (Join-Path $BuildDir "preview_018a.log")
if ($LASTEXITCODE -ne 0) { throw "preview_018a_main.exe FAILED" }

Write-Host "[5/5] Converting PPM to PNG and verifying 800x480 geometry in $ShotsDir ..."
$PyVerify = Join-Path $BuildDir "ppm_to_png_verify_018a.py"
@"
import sys, os, glob, struct, zlib

def ppm_to_png(ppm_path, png_path):
    with open(ppm_path, 'rb') as f:
        magic = f.readline().strip()
        assert magic == b'P6', f"Bad PPM magic in {ppm_path}"
        dims = f.readline().strip().split()
        w, h = int(dims[0]), int(dims[1])
        assert w == 800 and h == 480, f"Unexpected dimensions {w}x{h} in {ppm_path}"
        maxval = int(f.readline().strip())
        assert maxval == 255
        raw = f.read(w * h * 3)
        assert len(raw) == w * h * 3
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
ppms = sorted(glob.glob(os.path.join(shots_dir, '*.ppm')))
assert len(ppms) == 19, f"Expected 19 PPMs, got {len(ppms)}"
for ppm in ppms:
    png = os.path.splitext(ppm)[0] + '.png'
    ppm_to_png(ppm, png)
    os.remove(ppm)
    with open(png, 'rb') as f:
        hdr = f.read(24)
        pw, ph = struct.unpack('>II', hdr[16:24])
        assert (pw, ph) == (800, 480), f"PNG dims mismatch {pw}x{ph}"
print(f"Verified {len(ppms)} PNG screenshots at 800x480 in {shots_dir}")
"@ | Set-Content -Path $PyVerify -Encoding ASCII

python $PyVerify $ShotsDir
if ($LASTEXITCODE -ne 0) { throw "Failed converting/verifying 800x480 PNGs" }
