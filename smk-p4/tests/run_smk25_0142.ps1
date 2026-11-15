$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $repo
try {
    New-Item -ItemType Directory -Force build/smk25_0142 | Out-Null
    foreach ($name in @('profile', 'midi_routing')) {
        $test = "smk25_0142_${name}_test"
        $binary = "build/smk25_0142/$test.exe"
        & g++ -std=c++17 -O2 -Wall -Wextra -Werror -Isrc -Ismk-p4/main -Ismk-p4/components/amy_runtime -Ismk-p4/tests/nvs_mocks "smk-p4/tests/$test.cpp" -o $binary
        if ($LASTEXITCODE) { throw "$test compile failed" }
        & $binary
        if ($LASTEXITCODE) { throw "$test failed" }
        Write-Output "PASS $test"
    }
} finally { Pop-Location }
