param([switch]$Manual)
$ErrorActionPreference='Stop'
$RepoRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$global:LASTEXITCODE=0
& "$PSScriptRoot/run_preview.ps1" -LvglOnly
if($LASTEXITCODE -ne 0) { throw 'LVGL host preparation failed' }
$BuildDir=Join-Path $RepoRoot 'build/p4my_splash'
New-Item -ItemType Directory -Force $BuildDir | Out-Null
$Ui=Join-Path $RepoRoot 'smk-p4/components/ui_blueprint'
$Lvgl=Join-Path $RepoRoot 'smk-p4/managed_components/lvgl__lvgl'
& g++ -O2 -std=c++20 -Wall -Wextra -Werror -DLV_CONF_INCLUDE_SIMPLE=1 "-I$PSScriptRoot" "-I$Ui/include" "-I$Lvgl" "-I$RepoRoot/smk-p4/components/amy_runtime" "-I$RepoRoot/src" "$PSScriptRoot/splash_main.cpp" "$Ui/src/p4my_splash.cpp" "$RepoRoot/build/ui_blueprint_host/liblvgl_host.a" -lm -o "$BuildDir/splash.exe"
if($LASTEXITCODE -ne 0) { throw 'Splash compile failed' }
& "$BuildDir/splash.exe" $BuildDir
if($LASTEXITCODE -ne 0) { throw 'Splash lifecycle tests failed' }
& python "$PSScriptRoot/export_splash.py" $BuildDir
if($LASTEXITCODE -ne 0) { throw 'Splash export failed' }
if($Manual) { Start-Process (Join-Path $BuildDir 'replay.html') }
