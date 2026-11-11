$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $repo
try {
    New-Item -ItemType Directory -Force build/performance014 | Out-Null
    & g++ -std=c++17 -O2 -Wall -Wextra -Werror -Ismk-p4/components/amy_runtime smk-p4/tests/performance_014_macro_test.cpp -o build/performance014/macro_test.exe
    if ($LASTEXITCODE) { throw 'Performance macro test compile failed' }
    & build/performance014/macro_test.exe
    if ($LASTEXITCODE) { throw 'Performance macro test failed' }
} finally {
    Pop-Location
}
