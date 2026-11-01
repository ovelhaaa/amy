$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
Push-Location $repo
try {
    New-Item -ItemType Directory -Force -Path build/smk_ui | Out-Null
    $includes = @('-Itests/mock', '-Ismk-s3/components/ui/include', '-Ismk-s3/components/ui/include/screens', '-Ismk-s3/components/audio/include', '-Ismk-s3/components/system/include', '-Ismk-s3/components/sequencer/include', '-Ismk-s3/components/midi/include', '-Ismk-s3/components/synth/include', '-Ismk-s3/components/storage/include')
    $sources = @('smk-s3/components/ui/display_driver.cpp', 'smk-s3/components/ui/dummy_display_driver.cpp', 'smk-s3/components/ui/font_renderer.cpp', 'smk-s3/components/ui/widgets.cpp', 'smk-s3/components/ui/ui_manager.cpp')
    $sources += Get-ChildItem smk-s3/components/ui/screens -Filter *.cpp | ForEach-Object FullName
    & g++ -std=c++17 -O2 -pthread @includes tests/test_smk_ui.cpp @sources -o build/smk_ui/test_ui.exe
    if ($LASTEXITCODE -ne 0) { throw 'UI compilation failed' }
    & ./build/smk_ui/test_ui.exe
    if ($LASTEXITCODE -ne 0) { throw 'UI tests failed' }
} finally { Pop-Location }
