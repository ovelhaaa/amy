$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
Push-Location $repo
try {
    New-Item -ItemType Directory -Force build/smk_link | Out-Null
    & g++ -std=c++17 -O2 -Wall -Wextra -Werror -Ishared/smk_protocol tests/test_smk_link.cpp shared/smk_protocol/smk_protocol.cpp shared/smk_protocol/smk_link.cpp shared/smk_protocol/smk_client.cpp -o build/smk_link/test_link.exe
    if ($LASTEXITCODE -ne 0) { throw 'SMK link compilation failed' }
    & ./build/smk_link/test_link.exe
    if ($LASTEXITCODE -ne 0) { throw 'SMK link tests failed' }
    & g++ -std=c++17 -O2 -Wall -Wextra -Werror -Ishared/smk_protocol -Ismk-p4/main tests/test_smk_touch_note.cpp shared/smk_protocol/smk_protocol.cpp shared/smk_protocol/smk_link.cpp shared/smk_protocol/smk_client.cpp -o build/smk_link/test_touch.exe
    if ($LASTEXITCODE -ne 0) { throw 'Touch gesture compilation failed' }
    & ./build/smk_link/test_touch.exe
    if ($LASTEXITCODE -ne 0) { throw 'Touch gesture tests failed' }
} finally { Pop-Location }
