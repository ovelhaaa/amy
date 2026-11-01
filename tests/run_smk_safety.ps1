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
    & g++ -std=c++17 -O2 -pthread @includes tests/test_smk_usb_safety.cpp smk-s3/components/midi/usb_midi_host.cpp smk-s3/components/midi/event_bus.cpp smk-s3/components/midi/midi_parser.cpp -o build/smk_safety/test_usb.exe
    if ($LASTEXITCODE -ne 0) { throw 'USB safety test compilation failed' }
    & ./build/smk_safety/test_usb.exe
    if ($LASTEXITCODE -ne 0) { throw 'USB safety tests failed' }
    & g++ -std=c++17 -O2 -pthread @includes -Ismk-s3/components/audio/include tests/test_smk_audio_safety.cpp smk-s3/components/audio/audio_task.cpp smk-s3/components/audio/pcm5102_output.cpp -o build/smk_safety/test_audio.exe
    if ($LASTEXITCODE -ne 0) { throw 'Audio safety test compilation failed' }
    & ./build/smk_safety/test_audio.exe
    if ($LASTEXITCODE -ne 0) { throw 'Audio safety tests failed' }
    & g++ -std=c++17 -O2 -pthread @includes smk-s3/components/system/console_args.cpp tests/test_smk_console_args.cpp -o build/smk_safety/test_console_args.exe
    if ($LASTEXITCODE -ne 0) { throw 'Console args test compilation failed' }
    & ./build/smk_safety/test_console_args.exe
    if ($LASTEXITCODE -ne 0) { throw 'Console args tests failed' }
    & g++ -std=c++17 -O2 -pthread -Ismk-s3/components/sequencer/include -Ismk-s3/components/midi/include tests/test_smk_clock_timing.cpp -o build/smk_safety/test_clock_timing.exe
    if ($LASTEXITCODE -ne 0) { throw 'Clock timing test compilation failed' }
    & ./build/smk_safety/test_clock_timing.exe
    if ($LASTEXITCODE -ne 0) { throw 'Clock timing tests failed' }
    & g++ -std=c++17 -O2 -pthread -DSMK_TEST_CLOCK @includes -Ismk-s3/components/sequencer/include tests/test_smk_clock_manager.cpp smk-s3/components/sequencer/src/clock_manager.cpp -o build/smk_safety/test_clock_manager.exe
    if ($LASTEXITCODE -ne 0) { throw 'Clock manager compilation failed' }
    & ./build/smk_safety/test_clock_manager.exe
    if ($LASTEXITCODE -ne 0) { throw 'Clock manager tests failed' }
    & g++ -std=c++17 -O2 -pthread @includes -Ismk-s3/components/sequencer/include tests/test_smk_musical_timing.cpp smk-s3/components/sequencer/src/arpeggiator.cpp smk-s3/components/sequencer/src/step_sequencer.cpp smk-s3/components/midi/event_bus.cpp -o build/smk_safety/test_musical_timing.exe
    if ($LASTEXITCODE -ne 0) { throw 'Musical timing compilation failed' }
    & ./build/smk_safety/test_musical_timing.exe
    if ($LASTEXITCODE -ne 0) { throw 'Musical timing tests failed' }
    & g++ -std=c++17 -O2 -Ismk-s3/components/audio/include tests/test_smk_amy_time_bridge.cpp -o build/smk_safety/test_amy_time_bridge.exe
    if ($LASTEXITCODE -ne 0) { throw 'AMY time bridge compilation failed' }
    & ./build/smk_safety/test_amy_time_bridge.exe
    if ($LASTEXITCODE -ne 0) { throw 'AMY time bridge tests failed' }
} finally { Pop-Location }
