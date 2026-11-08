# Gate C - oscillator and amplitude controls

Status: **PASS - software acceptance complete**. Physical audio/Flash/deadline
qualification remains MANUAL / NOT RUN. Other 0.11.1 gates are not completed by
this change.

## Implemented controls

| Parameter | Native operation | Availability / neutral |
|---|---|---|
| osc.1.level | Scale original main pulse/saw AMP constant coefficients together | Identified audible main group; 100% retains its ROM balance |
| osc.sub_level | Native AMP constant, 0..100% | Reachable pulse exactly one octave below main; can activate a materialized zero-level component |
| osc.noise_level | Native AMP constant, 0..100% | Reachable noise component; no new oscillator allocated |
| osc.1.pulse_width | Native main PULSE duty constant, 5..95% | Audible main pulse and all non-constant duty coefficients zero; preserves dynamic PWM otherwise |
| osc.1.detune_ct | Add cents/1200 to captured main log-frequency constants | Identified main group; 0 cents preserves original pitch; sub remains independent |
| amp.level | AMY instrument_set_level for each of the eight instruments | Juno and FM; relative 0..100%, captured levels restored exactly |
| amp.pan | Native output-head PAN constant | Identified Juno control head; -64..63 maps to 0.5 + value/128 |
| amp.vel_sens | Native output-head amplitude velocity coefficient | Identified Juno control head; 0 ignores velocity, 100% uses coefficient 1 |

Patch gain is applied by the AMY owner at block boundaries and is independent
of Master. Zero patch gain produces exact stereo silence with active notes.
Patch reconstruction already deletes/recreates instruments, resetting a prior
manual gain without changing the AMY core. No new instrument, modulation
oscillator, voice redistribution, dependency, GPIO or partition change.

## Component discovery

`juno_components.h` classifies the materialized local audio graph. It finds a
unique filtered SILENT control head, follows bounded chain links, excludes
modulators, then identifies main pulse/saw nodes by native note coefficient and
common log frequency. A sub must be PULSE exactly one octave below that group;
noise must be a reachable NOISE node. Unknown nodes, cycles, out-of-voice links,
ambiguous roots, duplicate components and incompatible pitch relations reject
component editing. The graph is checked across all eight voices; relevant
native coefficient arrays and oscillator roles must agree before admission.
No physical oscillator index is assumed to be main/sub/noise.

Sub/noise controls are absent or N/A when the graph has no such component. A
zero-level but materialized oscillator is real and can be enabled without
allocation. MAIN MIX scales the existing main blend, so a ROM-disabled main
waveform is not created. Its pulse-width control remains N/A while inaudible.

## Contextual limits

- Pulse Width was qualified on patch 31. On tested patches 0, 1, 4 and 8 the
  main pulse is inaudible or uses dynamic duty; rewriting its duty as a static
  scalar would not preserve the native PWM range, so the control stays N/A.
- Unison remains planned: no eight-voice redistribution policy was introduced.
- Wave/coarse-pitch reconstruction and per-component stereo pan remain planned.
  These are outside the eight requested Gate C controls.
- Malformed/unrecognized layouts retain patch gain through the instrument API;
  component controls are unavailable rather than inferred from slot numbers.

The live OSC tab highlights real mix/detune/pulse-width controls. Planned wave,
coarse pitch and unison widgets are omitted from that live tab; the demo layout
is retained. Incompatible sliders show N/A, rather than the PLAN placeholder
used for genuinely planned controls. AMP exposes real gain/pan/velocity with
owner-confirmed values.

## V2 compatibility and resource bounds

Stable IDs 21..28 extend the mask to 29 fields. V1 still encodes only IDs 0..16
in 128 bytes; V2 remains 160 bytes, preserving all existing IDs, offsets, CRC,
ROM discrimination, separate namespace and 16 dual-bank admission. Maximum
active fields are now 16 for Juno or 14 for FM, fitting within the codec's
20-pair bound. The schema validates family restrictions and rejects overflow.
Native level/duty/frequency/pan coefficients, envelope segments and routing
are retained for Compare/Revert and reconstructed before Recall overrides.
There is no automatic migration or rewriting of V1 banks.

The edit mask has an explicit compile-time capacity guard. Future per-operator
FM expansion must revisit mask and record capacity; it must not simply grow
Count past those bounds.

Host Runtime size: 6768 bytes (Gate B 6476, +292). Edit State: 388 bytes (292,
+96). P4 normal build reports static RAM 36,640 bytes (Gate B 35,944, +696) and
Flash code/data 1,194,218 bytes (1,190,676, +3542). These PIO figures are not
runtime free RAM or a complete DRAM/PSRAM budget. Capture graph work runs on the
patch worker; live edits use bounded native deltas and existing instrument APIs.
Unmuting a ROM-zero sub/noise component increases DSP work; host tests do not
establish P4 render-deadline margin at the maximum mix.

## Validation

| Check | Result |
|---|---|
| Synthetic graph: 720 permutations, missing components, cycle, ambiguous pitch, LFO exclusion | PASS |
| Five Juno patches 0/1/4/8/31; one voice and eight-voice Harmony | PASS |
| All eight controls change PCM and confirmed native state when available | PASS |
| Compare and Revert restore Original PCM; new runtime/backend/store Recall restores Edited PCM | PASS |
| 360 main PCM renders, plus velocity, FM gain and stress cases | PASS |
| Eight controls changed during held Harmony notes without retrigger or ownership changes | PASS |
| Velocity 127 invariant; lower-velocity sensitivity changes amplitude | PASS |
| FM patch 135 instrument gain, restoration/Recall and operator preservation | PASS |
| Gain zero mute, factory reload resets gain, Master unchanged | PASS |
| Max mix/detune, static duty 95%, velocity 127, eight voices on five patches | PASS - measured clipping count zero in these scenarios |
| No allocation/free in observed apply/render path; release/sustain/Panic | PASS |
| LVGL owner confirmation, contextual status, combined V2 Save/Recall/reboot, OSC layout | PASS |
| Full editor, Gate B, Harmony/USB/factory host suites | PASS |
| No-override PCM against 0.11 (seven comparator patches) | PASS - byte identical |
| Normal P4 build and effective configuration | PASS |
| Hardware, actual NVS power cuts, P4 deadlines, bench candidate | MANUAL / NOT RUN |

Qualification covers the listed scenarios, not every extreme edit combination
on all 229 factory entries. No physical firmware or NVS was written. The display identifies this build as
`SMK-P4 EDITOR 0.11.1 DEV`. The legacy 0.11 review packager is not admitted for
this development image; a full candidate manifest remains part of Gate H.

Reproduce from this worktree (existing AMY objects are generated by the documented
run_harmony.ps1 if absent):

```powershell
./smk-p4/tests/run_sound_editor.ps1 -GateCOnly
./smk-p4/tests/run_sound_editor.ps1
./smk-p4/tests/run_sound_editor_ui.ps1
./smk-p4/tests/compare_editor_factory.ps1 -Baseline ../smk-p4-live-sound-editor-011 -EditorBaseline
C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-editor-8 -j 8
python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library --unified-engine --harmony --full-factory --sound-editor --environment guition-editor-8
```

Logs: build/editor011/gate_c_test.log and the existing editor/PCM/Harmony/USB/
factory/UI logs. The UI suite also saves build/editor011/osc.ppm.

## Short physical smoke (requires separate flash authorization)

On the qualified Guition setup, select Moving Strings, play held notes and vary
MAIN MIX/Sub/Noise, gain, pan, detune and velocity sensitivity. MAIN MIX must
preserve the main blend; zero gain must mute without changing Master. Compare
and Revert must restore factory sound. Select patch 31 for static Pulse Width;
on dynamic PWM patches the field must remain N/A. Enable Harmony, sustain and
release chords; Panic must clear them. Save a named edited patch while silent,
reboot and Recall; values and sound must return. Back up Flash/NVS and preserve
rollback before authorizing any image. Long hardware soak is not implied.

Next smallest implementation gate: FM expansion (Gate D), including a bounded
parameter-mask/storage design for additional per-operator fields.
