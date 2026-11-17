$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $repo
try {
    New-Item -ItemType Directory -Force build | Out-Null
    $binary = 'build/performance_scenes_017b_test.exe'
    & g++ -std=c++17 -O2 -Wall -Wextra -Werror -DAMY_NO_MINIAUDIO -Isrc -Ismk-p4/components/amy_runtime -Ismk-p4/main -Ismk-p4/tests/nvs_mocks -Ishared/smk_protocol smk-p4/tests/performance_scenes_017b_test.cpp -o $binary
    if ($LASTEXITCODE) { throw 'Scene selection compile failed' }
    & $binary
    if ($LASTEXITCODE) { throw 'Scene selection test failed' }
} finally {
    Pop-Location
}
