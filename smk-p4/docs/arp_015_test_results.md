# SMK-P4 0.15 validation

The baseline is `f4eeb0d8d450a9bd06a3553415c151d28ed3d9f3`.
Run the host suite from the repository root with MinGW GCC and the pinned
LVGL host dependencies used by the existing editor tests.

```powershell
powershell -File smk-p4/tests/run_harmony.ps1
git worktree add --detach build/arp_015/baseline f4eeb0d8d450a9bd06a3553415c151d28ed3d9f3
powershell -File smk-p4/tests/run_arp_015.ps1 -Runtime -BaselineRoot build/arp_015/baseline
```

- **PASS** `run_arp_015.ps1 -Runtime -BaselineRoot build/arp_015/baseline`.
  Across 1,000 steps per combination of six tempos, six rates and three
  swing settings, onsets stayed within 256 frames of ideal phase. Maximum
  measured deviation was 5,291.666 µs in the sample trace. The host
  fixtures also cover live tempo/division changes, deterministic directions,
  capacity, latch replacement and MIDI pitch bounds.
- **PASS** The real AMY fixture produced the same PCM as direct MIDI for
  roots 60, 64 and 67 with velocities 101, 82 and 93. On blocks were
  0/24/47 and Off blocks were 16/39/63. An allocation probe surrounded
  owner apply/render. USB queue saturation before Stop, independent Touch,
  reconnect via the new generation, Panic, Patch and four-note Harmony
  admission were exercised in the owner.
- **PASS** ARP-off Juno/DX7 PCM matched the 0.14.2 code byte for byte using
  the same real AMY C objects. Fixture SHA-256
  `9943db1480f10e31e234f1cfb4be781dfffb672444989b63637a266b86f06e40`.
- **PASS** `run_sound_editor_ui.ps1`. LVGL 9.3 owner confirmation,
  Tap Tempo, B4 press/repeat/release, Play and touchscreen Stop with a
  full normal command queue were exercised alongside the previous editor
  and MIDI regressions.
- **PASS** `run_smk25_0142.ps1`, `run_performance_014.ps1`,
  `run_fx_tests.ps1 all`, `run_harmony.ps1`, `run_usb.ps1`.
- **PASS** `run_sound_editor.ps1`. A primeira execução interrompeu no
  seletor FM. O seletor isolado e as reexecuções completas passaram.
- **PASS** Ambos os perfis compilados e auditados com a configuração
  efetiva. Os comandos estão abaixo. Os hashes estão no [candidato físico](arp_015_hardware_candidate.md).

```powershell
$env:PLATFORMIO_CORE_DIR='C:/.platformio'
& C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-editor-8 -j 8
& C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-editor-8-bench -j 8
python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library --unified-engine --harmony --full-factory --sound-editor --environment guition-editor-8
python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library --unified-engine --harmony --full-factory --sound-editor --musical --environment guition-editor-8-bench
```

The final PCM fixture run observed 18 Âµs maximum for the host ARP service,
including Harmony admission when scheduled. A separate run observed 107 Âµs
under concurrent host build load. These are Windows host timings. On the
same MinGW ABI, `Runtime` increased from 11,304 to 11,688 bytes.
The owner snapshot and the PSRAM-preferred backend each grew by 96 bytes.
`Command` remained at 24 bytes.
The P4 bench still needs hardware measurements of render average/maximum,
clock jitter, RAM/PSRAM admission, stack, deadlines, queue overflows, OOM,
clipping and audio continuity. The 0.14.2 Guition reference was 358,515 B
free against 352,256 B required, with 211 Âµs average and 1,184 Âµs max render
in its documented idle check.
