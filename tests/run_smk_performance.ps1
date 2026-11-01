$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
Push-Location $repo
try {
    New-Item -ItemType Directory -Force -Path build/smk_performance | Out-Null
    & g++ -std=c++17 -O2 -Wall -Wextra -Werror -pthread -Ismk-s3/components/audio/include tests/test_smk_performance.cpp -o build/smk_performance/test_performance.exe
    if ($LASTEXITCODE -ne 0) { throw 'Performance test compilation failed' }
    & ./build/smk_performance/test_performance.exe
    if ($LASTEXITCODE -ne 0) { throw 'Performance tests failed' }
} finally { Pop-Location }
