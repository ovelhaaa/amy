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
} finally { Pop-Location }
