$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $repo
try {
    New-Item -ItemType Directory -Force build/arp_015 | Out-Null
    $binary = 'build/arp_015/arp_015_engine_test.exe'
    & g++ -std=c++17 -O2 -Wall -Wextra -Werror -Ismk-p4/components/amy_runtime smk-p4/tests/arp_015_engine_test.cpp -o $binary
    if ($LASTEXITCODE) { throw 'Arp engine compile failed' }
    & $binary
    if ($LASTEXITCODE) { throw 'Arp engine test failed' }
    Write-Output 'PASS arp_015_engine_test'
} finally { Pop-Location }
