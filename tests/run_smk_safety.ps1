$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
Push-Location $repo
try {
    New-Item -ItemType Directory -Force -Path 'build/smk_safety' | Out-Null
    $includes = @('-Itests/mock', '-Ismk-s3/components/midi/include', '-Ismk-s3/components/system/include')
    & g++ -std=c++17 -O2 -pthread @includes tests/test_smk_midi_safety.cpp smk-s3/components/midi/event_bus.cpp smk-s3/components/midi/midi_parser.cpp smk-s3/components/midi/midi_learn.cpp smk-s3/components/midi/controller_profile.cpp -o build/smk_safety/test_midi.exe
    if ($LASTEXITCODE -ne 0) { throw 'MIDI safety test compilation failed' }
    & ./build/smk_safety/test_midi.exe
    if ($LASTEXITCODE -ne 0) { throw 'MIDI safety tests failed' }
    & g++ -std=c++17 -O2 -pthread @includes -Ismk-s3/components/audio/include tests/test_smk_audio_safety.cpp smk-s3/components/audio/audio_task.cpp smk-s3/components/audio/pcm5102_output.cpp -o build/smk_safety/test_audio.exe
    if ($LASTEXITCODE -ne 0) { throw 'Audio safety test compilation failed' }
    & ./build/smk_safety/test_audio.exe
    if ($LASTEXITCODE -ne 0) { throw 'Audio safety tests failed' }
} finally { Pop-Location }
