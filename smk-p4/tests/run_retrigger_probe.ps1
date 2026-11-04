param([string]$BaselineRef='cd9ed0ba', [string]$CandidateRef='3fa65331')
$ErrorActionPreference='Stop'
$repo=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $repo
try {
    $output=Join-Path $repo ('build/retrigger_ab/'+[guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Force $output | Out-Null
    $manifest=@{ compiler=(& gcc --version | Select-Object -First 1); variants=@{} }
    foreach($variant in @('baseline','integration')) {
        $ref=if($variant -eq 'baseline') { $BaselineRef } else { $CandidateRef }
        $sha=(& git rev-parse "$ref^{commit}").Trim()
        if($LASTEXITCODE -ne 0) { throw "Invalid commit: $ref" }
        & git archive --format=zip "--output=$output/$variant.zip" $sha src smk-p4/components/amy_runtime
        if($LASTEXITCODE -ne 0) { throw 'git archive failed' }
        $root="$output/$variant"
        Expand-Archive -LiteralPath "$output/$variant.zip" -DestinationPath $root
        $runtimeRoot="$root/smk-p4/components/amy_runtime"
        $flags=@('-O2','-DAMY_NO_MINIAUDIO','-DAMY_SAMPLE_RATE=48000','-DAMY_BLOCK_SIZE=256','-DBLOCK_SIZE_BITS=8','-DAMY_PATCH_CHECKED_RESOURCE_ADMISSION=1','-DAMY_EXCLUSIVE_FM_PHASE_RESTART=1',"-I$root/src","-I$runtimeRoot")
        $cmake=Get-Content "$runtimeRoot/CMakeLists.txt" -Raw
        $sources=[regex]::Matches($cmake,'\$\{AMY_SRC_DIR\}/([^"\r\n]+\.c)') | ForEach-Object { $_.Groups[1].Value }
        if(!$sources.Count) { throw 'No AMY sources found' }
        $objects=@()
        foreach($source in $sources) {
            $obj="$root/$source.o"
            & gcc -std=gnu11 @flags -c "$root/src/$source" -o $obj
            if($LASTEXITCODE -ne 0) { throw "AMY compile failed: $variant/$source" }
            $objects+=$obj
        }
        foreach($test in @('render_retrigger_probe','test_retrigger_scenarios')) {
            $exe="$output/${variant}_$test.exe"
            & g++ -std=c++17 @flags "smk-p4/tests/$test.cpp" "$runtimeRoot/amy_runtime.cpp" @objects -lm -o $exe
            if($LASTEXITCODE -ne 0) { throw "Link failed: $variant/$test" }
            & $exe $output $variant | Tee-Object "$output/${variant}_$test.log"
            if($LASTEXITCODE -ne 0) { throw "Test failed: $variant/$test" }
        }
        $hashes=@{}
        foreach($patch in @(128,135)) { $hashes["$patch"]=(Get-FileHash "$output/${variant}_patch_$patch.wav" -Algorithm SHA256).Hash }
        $manifest.variants[$variant]=@{ commit=$sha; wav_sha256=$hashes }
    }
    $manifest | ConvertTo-Json -Depth 6 | Set-Content "$output/manifest.json"
    & python smk-p4/tests/check_retrigger_ab.py $output
    if($LASTEXITCODE -ne 0) { throw "A/B acceptance failed: $output" }
    Write-Host "Evidence: $output"
} finally { Pop-Location }
