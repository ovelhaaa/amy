$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $repo
try {
    New-Item -ItemType Directory -Force build/smk_p4_display | Out-Null
    foreach ($test in @('display_geometry_test', 'display_dirty_test')) {
        & g++ -std=c++17 -O2 -Wall -Wextra -Werror "smk-p4/tests/$test.cpp" -o "build/smk_p4_display/$test.exe"
        if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $test" }
        & "./build/smk_p4_display/$test.exe"
        if ($LASTEXITCODE -ne 0) { throw "Test failed: $test" }
        Write-Host "PASS: $test"
    }
    & g++ -std=c++17 -O2 -Wall -Wextra -Werror smk-p4/tests/pcm_test.cpp smk-p4/components/audio_pcm5102/pcm_test.cpp -o build/smk_p4_display/pcm_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'PCM compilation failed' }
    & ./build/smk_p4_display/pcm_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'PCM tests failed' }
    Write-Host 'PASS: PCM routing, conversion, bounds, fades, frequency, silence and restart'
    & g++ -std=c++17 -O2 -Wall -Wextra -Werror -Ismk-p4/tests/mocks smk-p4/tests/pcm_driver_test.cpp smk-p4/components/audio_pcm5102/pcm5102_output.cpp -o build/smk_p4_display/pcm_driver_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'PCM driver compilation failed' }
    & ./build/smk_p4_display/pcm_driver_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'PCM driver tests failed' }
    Write-Host 'PASS: I2S lifecycle, silent preload, PCM contract and injected failures (mock driver)'
    & python -m unittest discover -s smk-p4/tests -p 'test_*.py'
    if ($LASTEXITCODE -ne 0) { throw 'P4 measurement parser tests failed' }
} finally { Pop-Location }
