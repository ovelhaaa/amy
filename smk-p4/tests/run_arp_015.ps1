param([switch]$Runtime,[string]$BaselineRoot)
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
    if ($Runtime) {
        $objects = Get-ChildItem build/harmony09/*.c.o | ForEach-Object FullName
        if (!$objects) { throw 'Run run_harmony.ps1 first' }
        $flags = @('-std=c++17','-O2','-Wall','-Wextra','-Werror','-DSMKP4_SOUND_EDITOR=1','-DSMKP4_FULL_FACTORY_LIBRARY=1','-DSMKP4_HARMONY=1','-DSMKP4_UNIFIED_ENGINE=1','-DSMKP4_SOUND_LIBRARY=1','-DSMKP4_VOICES=8','-DAMY_NO_MINIAUDIO','-DAMY_SAMPLE_RATE=48000','-DAMY_BLOCK_SIZE=256','-DBLOCK_SIZE_BITS=8','-DAMY_PATCH_CHECKED_RESOURCE_ADMISSION=1','-DAMY_EXCLUSIVE_FM_PHASE_RESTART=1','-Isrc','-Ismk-p4/main','-Ismk-p4/components/amy_runtime','-Ishared/smk_protocol','-Ismk-p4/tests/nvs_mocks')
        $binary = 'build/arp_015/arp_015_runtime_test.exe'
        & g++ @flags smk-p4/tests/arp_015_runtime_test.cpp smk-p4/components/amy_runtime/amy_runtime.cpp smk-p4/tests/heap_probe.cpp @objects '-Wl,--wrap=malloc' '-Wl,--wrap=calloc' '-Wl,--wrap=realloc' '-Wl,--wrap=free' -lm -o $binary
        if ($LASTEXITCODE) { throw 'Arp runtime compile failed' }
        & $binary
        if ($LASTEXITCODE) { throw 'Arp runtime test failed' }
        if ($BaselineRoot) {
            $BaselineRoot = (Resolve-Path $BaselineRoot).Path
            $currentRuntime = 'smk-p4/components/amy_runtime'
            $baselineRuntime = Join-Path $BaselineRoot $currentRuntime
            foreach ($entry in @(@($currentRuntime,'current'),@($baselineRuntime,'baseline'))) {
                $root, $name = $entry
                $exe = "build/arp_015/pcm_$name.exe"
                & g++ "-I$root" @flags smk-p4/tests/arp_015_pcm_fixture.cpp "$root/amy_runtime.cpp" @objects -lm -o $exe
                if ($LASTEXITCODE) { throw "PCM fixture compile failed: $name" }
                & $exe "build/arp_015/$name.pcm"
                if ($LASTEXITCODE) { throw "PCM fixture failed: $name" }
            }
            $current = (Get-FileHash build/arp_015/current.pcm).Hash
            $baseline = (Get-FileHash build/arp_015/baseline.pcm).Hash
            if ($current -ne $baseline) { throw 'ARP-off PCM differs from baseline' }
            Write-Output "PASS ARP-off PCM vs $BaselineRoot : $current"
        }
    }
} finally { Pop-Location }
