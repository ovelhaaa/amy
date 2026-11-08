param([Parameter(Mandatory=$true)][string]$Baseline,[switch]$EditorBaseline)
$ErrorActionPreference='Stop'
$repo=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $repo
try {
    $baselineRepo=(Resolve-Path $Baseline).Path
    $objects=Get-ChildItem build/harmony09/*.c.o | ForEach-Object FullName
    if(!$objects){throw 'Run run_harmony.ps1 first'}
    $flags=@('-std=c++17','-O2','-DAMY_NO_MINIAUDIO','-DAMY_SAMPLE_RATE=48000','-DAMY_BLOCK_SIZE=256','-DBLOCK_SIZE_BITS=8','-DAMY_PATCH_CHECKED_RESOURCE_ADMISSION=1','-DAMY_EXCLUSIVE_FM_PHASE_RESTART=1','-DSMKP4_VOICES=8','-DSMKP4_SOUND_LIBRARY=1','-DSMKP4_UNIFIED_ENGINE=1')
    foreach($which in @('baseline','candidate')){
        $source=if($which -eq 'baseline'){$baselineRepo}else{$repo}
        $objects=Get-ChildItem "$source/build/harmony09/*.c.o" | ForEach-Object FullName
        if(!$objects){throw "Run run_harmony.ps1 in $source first"}
        $extra=@();if($which -eq 'baseline' -and $EditorBaseline){$extra+=@('-DSMKP4_HARMONY=1','-DSMKP4_SOUND_EDITOR=1')}
        if($which -eq 'candidate'){$extra+=@('-DSMKP4_HARMONY=1','-DSMKP4_SOUND_EDITOR=1')}
        $out="build/editor011/pcm_$which"
        New-Item -ItemType Directory -Force $out | Out-Null
        & g++ @flags @extra "-I$source/src" "-I$source/smk-p4/components/amy_runtime" smk-p4/tests/harmony_off_pcm.cpp "$source/smk-p4/components/amy_runtime/amy_runtime.cpp" @objects -lm -o "$out/pcm.exe"
        if($LASTEXITCODE){throw "PCM compile failed: $which"}
        & "$out/pcm.exe" $out
        if($LASTEXITCODE){throw "PCM render failed: $which"}
    }
    foreach($file in Get-ChildItem build/editor011/pcm_baseline/*.pcm){
        $hash=(Get-FileHash $file.FullName -Algorithm SHA256).Hash
        $candidate=(Get-FileHash "build/editor011/pcm_candidate/$($file.Name)" -Algorithm SHA256).Hash
        if($hash -ne $candidate){throw "PCM differs: $($file.Name)"}
        Write-Host "PASS byte-identical 0.11 no-overrides $($file.Name) SHA256=$hash"
    }
}finally{Pop-Location}
