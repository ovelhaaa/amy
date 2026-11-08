# Gate B - native Juno filter controls

Status: **PASS - software acceptance complete**. Physical verification remains
**MANUAL / NOT RUN** and does not block this software gate. Other 0.11.1 gates
are not declared complete.

| Acceptance check | Result |
|---|---|
| Two new native filter controls | PASS |
| C2/C4/C6, 1/8 voices, Harmony, EG0/EG1 | PASS - 240 real-AMY renders |
| PCM changes, exact Compare/Revert/Recall | PASS |
| Native coefficient preservation, held-note identity | PASS |
| Sustain/retrigger/release, clipping, render allocation | PASS |
| V1 retention, V2 16 slots, CRC/torn/space/read/commit recovery | PASS - simulated NVS API |
| Confirmed UI values and V2 Save/Recall/reboot | PASS - LVGL host suite |
| 0.11 factory no-override PCM | PASS - seven patches, editor enabled in both revisions |
| Full editor/Harmony/USB/factory host regressions | PASS - 229 admitted IDs, 687 swaps |
| guition-editor-8 build and effective config check | PASS - RAM 35,944 bytes, image code/data 1,190,676 bytes |
| Bench build | NOT RUN - Gate H candidate remains separate |
| Physical NVS, power cuts, P4 audio/deadlines | MANUAL / NOT RUN |

## Scope and architecture

Two controls are connected to the AMY owner and its confirmed UI snapshot:
`filter.env_amount` (native envelope contribution, -11..11 octaves) and
`filter.keytrack` (native NOTE coefficient, 0..200 percent). Cutoff and resonance
retain their original commands. Filter type remains planned.

Materialized Juno filter routing chooses EG1 when the gated VCA leaves its ADSR
there; otherwise it chooses EG0. Capture checks filter type and coefficients
across all eight instrument maps. Edits update only the selected coefficient;
Compare/Revert restore captured native floats exactly. Keytracking multiplies
the AMY MIDI note input, rather than modifying only a fixed cutoff. Existing
cutoff behavior and the ROM's native reference pitch are retained. No AMY core,
USB/I2S driver, partition, GPIO, or voice-allocation changes.

## V2 Save/Recall

The production NvsUserPresets now delegates to the versioned store. Reads scan
both legacy banks and both V2 banks, rejecting read errors and recognizable
future versions. Highest valid sequence wins; V1 wins a tie. Writes target only
an absent/invalid or older V2 bank, followed by commit and exact byte readback.
No V1 value or key is erased or migrated automatically.

V1 remains 128 bytes in namespace `sound011`, keys `s00a`..`s15b`. V2 is 160
bytes in separate namespace `sound011v2`, keys `v00a`..`v15b`: unchanged header,
name and macros at 0..51, ordered ID/binary32 pairs from 52, ROM discriminator
at 152, CRC32 at 156. Maximum active fields currently: 8 Juno or 13 FM. Missing
bits preserve the factory default, including when importing V1. Both filter
fields and Juno Decay/Sustain are admitted by the V2 validator. V1 encoding
still rejects new IDs and retains its offsets.

Before each write, stats and the V2 namespace's used-entry count must satisfy:

- free + V2 used >= 16 * 2 * 8 + 126 + 8 = 390 entries;
- free >= 126 + 8 = 134 entries.

The 24 KiB partition contains six 126-entry pages (756 entries). A fully retained
legacy store budgets 32 * 7 = 224 entries; V2 budgets 32 * 8 = 256 plus 134 for
GC and a replacement record. Two namespaces bring this conservative bound to
616 entries. SDK 5.5.2's nvs_page.cpp / nvs_storage.cpp use 32-byte entries,
chunk headers and a blob index: 160 bytes normally needs 7 entries; 8 allows a
split chunk. Actual stats decide admission, so unrelated NVS use cannot be
assumed absent. Exhaustion, fragmentation, stats/commit/readback failures report
Save failure while the previous valid bank remains available. Recall is allowed
when writes lack space. No partition expansion or init-error erase is performed.
The namespace is owned exclusively by this store.

The existing owner-acknowledged quiet barrier surrounds Save on the UI/control
task. No NVS access is added to render, DMA, ISR or active-note edits. Saving is
explicit and may cut release tails. No gapless Flash-write claim is made.

## Software acceptance

`./smk-p4/tests/run_sound_editor.ps1 -GateBOnly` compiles the real AMY engine
adapter test against the existing native AMY objects and a simulated NVS API.
`gate_b_test.log` records 240 renders: patches 0, 1, 4 and 8; both controls;
C2/C4/C6 (MIDI 36/60/84); one voice and eight voices admitted through Harmony;
Original, Edited, Compare, Revert, Save/Recall across fresh runtime/backend/store
instances. PCM must change when edited, match Original byte-for-byte after
Compare/Revert, and match Edited after Recall. Each case checks every native
filter coefficient, held-note/group preservation, sustain, retrigger, Note Off, zero clipping and zero AMY
allocation in apply/render. EG1 must actually be exercised.

The NVS adapter test prepopulates all 32 legacy banks, fills all 32 V2 banks,
checks retained V1 bytes, simulates restart, torn records, CRC corruption,
read/stat/space/commit failures and validates recovery. These are storage
simulation tests, not execution of ESP-IDF Flash on hardware.

The LVGL suite tests Available/Unavailable status, owner-confirmed values,
Save/Recall/reboot for both new filter controls and disabled filter controls
on FM. Factory no-override PCM is compared against the 0.11 editor worktree.

## Reproduction

```powershell
./smk-p4/tests/run_sound_editor.ps1
./smk-p4/tests/run_sound_editor.ps1 -GateBOnly
./smk-p4/tests/run_sound_editor_ui.ps1
./smk-p4/tests/compare_editor_factory.ps1 -Baseline ../smk-p4-live-sound-editor-011 -EditorBaseline
C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-editor-8 -j 8
python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library --unified-engine --harmony --full-factory --sound-editor --environment guition-editor-8
```

## Hardware verification (MANUAL / NOT RUN)

After separate authorization to flash the exact image, preserve full Flash/NVS
and rollback images. Select Moving Strings; play C2/C4/C6 and vary envelope
amount and keytrack. Compare must restore factory behavior, and Revert must
clear edits. Enable Harmony, hold chords with sustain, release pedal/notes and
confirm release/Panic. Save a named edited patch while silent; reboot and Recall
must restore its values and sound. Test short power interruptions only with
backup and separate operator authorization; confirm at least one valid bank.
Older firmware continues reading original V1 keys; it does not see V2 edits.
Physical NVS capacity, power-loss recovery, audio continuity during quiet Save
and P4 deadlines are not certified by host tests. No firmware was flashed.
