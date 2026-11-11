$ErrorActionPreference='Stop'
$repo=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $repo
try {
    New-Item -ItemType Directory -Force build/smk_p4_usb | Out-Null
    $includes=@('-Isrc','-Ismk-p4/tests/usb_mocks','-Itests/mock','-Ismk-p4/components/amy_runtime','-Ismk-p4/components/usb_midi_host','-Ismk-s3/components/midi/include')
    & g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wno-unused-parameter -pthread @includes smk-p4/tests/usb_host_test.cpp smk-p4/components/usb_midi_host/usb_midi_host.cpp -o build/smk_p4_usb/host.exe
    if($LASTEXITCODE -ne 0) { throw 'P4 USB host compilation failed' }
    & ./build/smk_p4_usb/host.exe
    if($LASTEXITCODE -ne 0) { throw 'P4 USB host tests failed' }
} finally { Pop-Location }
