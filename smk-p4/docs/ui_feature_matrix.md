# SMK-P4 UI Blueprint 1.0 — Feature & Parameter Classification Matrix

Per Section 6 and Section 7 of the specification, **every feature and parameter in `smk-p4/components/ui_blueprint` is classified by actual repository evidence** (`AVAILABLE`, `PLANNED`, `UNAVAILABLE`, or `DEMO`). No future capability is misrepresented as active in the standalone P4 synthesis engine.

---

## 1. Classification Legend

| Status | Badge | Meaning | Repository Evidence Criterion |
|---|---|---|---|
| **`AVAILABLE`** | `AVAIL` (`#67C4A5`) | Implemented end-to-end in `smk-p4` standalone runtime today. | Wired in `smk-p4/components/amy_runtime/amy_runtime.cpp`, `smk-p4/main/performance_backend.h`, or `smk-p4/components/board/display_hal.cpp`. |
| **`PLANNED`** | `PLAN` (`#E4A44D`) | Present in upstream AMY (`src/`), `smk-s3/components/synth/`, or `specs.md`, awaiting P4 runtime bridge exposure. | Verified in `src/*.cpp` or `smk-s3/components/*`, not yet wired in `smk-p4/components/amy_runtime`. |
| **`UNAVAILABLE`** | `N/A` (`#80919F`) | Inapplicable to the currently selected synthesis family/mode or blocked by missing hardware. | Enforced dynamically by `UiModel::effectiveStatus(ParameterId)`. |
| **`DEMO`** | `DEMO` (`#EAB65B`) | Driven by `UiDemoBackend` for interactive UI preview without hardware. | Simulated meters (`DEMO SIM`), RAM-only user slots (`U01`, `U02`), and demo diagnostics. |

---

## 2. Subsystem Feature Matrix

| Subsystem | Feature / Screen | Classification | Repository Evidence |
|---|---|---|---|
| **Patch Selection** | Program `128` (`Juno A60 Strings`) & `135` (`DX7 Rhodes Mark I`) | `AVAILABLE` | `smk-p4/components/amy_runtime/amy_runtime.cpp` (`selectProgram`), `smk-p4/main/performance_backend.h` |
| **Patch Selection** | S3 Factory Patches (`0..7` Subtractive/FM/Organ/Bass/Lead) & AMY `256` | `PLANNED` | `smk-s3/components/synth/src/factory_patches.cpp` |
| **Patch Selection** | User Patch Slots (`U01`, `U02`) & Favorites | `DEMO` / `PLANNED` | `smk-s3/components/storage` (CRC32 format); RAM-only in `UiDemoBackend` |
| **Master & Expressive** | Master Volume (`CC7`), Expression (`CC11`), Pitch Bend, Mod Wheel (`CC1`), Sustain (`CC64`), Octave Shift, Panic | `AVAILABLE` | `smk-p4/components/amy_runtime/amy_runtime.cpp`, `shared/smk_protocol/smk_state.h` |
| **Performance Macros** | 4/8 Contextual Stage Macros (`TIMBRE`, `COLOR`, `SPACE`, `MOTION`) | `PLANNED` | `smk-s3/components/synth/include/macro_profile.h`, `patch_family.h` |
| **Oscillator (`OSC`)** | Waveform (`SINE`, `PULSE`, `SAW`, `TRI`, `NOISE`, `WT`), Pitch, Fine, Level, Pan, Duty, Unison | `PLANNED` (`Duty` `UNAVAILABLE` unless `PULSE`) | `src/oscillators.cpp`, `src/amy.c` |
| **FM Synthesis (`FM`)** | 6-Operator Algorithm, Ratio, Op Level, Feedback, Velocity Sens | `PLANNED` (`UNAVAILABLE` when family $\neq$ `FM`) | `src/algorithms.cpp` (DX7 6-op FM engine) |
| **Filter (`FILTER`)** | LPF 24dB / 12dB / HPF / BPF, Cutoff Hz, Resonance, Drive, Keytrack, Env Amt | `PLANNED` (`UNAVAILABLE` when `BYPASS`) | `src/filters.cpp`, `smk-s3/components/synth/include/patch_types.h` |
| **Envelopes (`ENV`)** | ADSR (`bp0` / `bp1` breakpoints: Attack, Decay, Sustain, Release) | `PLANNED` | `src/envelope.cpp` |
| **LFO & Mod Matrix** | LFO Rate/Depth/Sync + 8-Slot Source $\rightarrow$ Destination Matrix | `PLANNED` | `src/amy.c` (`mod_source` / `mod_target`), `specs.md` §10 |
| **Global FX (`EFFECTS`)** | Chorus (`src/chorus.cpp`), Reverb (`src/reverb.cpp`), Delay/Echo (`src/echo.cpp`), Drive | `PLANNED` | `src/chorus.cpp`, `src/reverb.cpp`, `src/echo.cpp` |
| **Layers & Split** | Layer A (Single Voice Group) | `AVAILABLE` | `smk-p4/components/amy_runtime/amy_runtime.cpp` |
| **Layers & Split** | Layer B Dual/Split Engine & Keyboard Split Point | `PLANNED` | `specs.md` §13 (`Layer A/B` architecture) |
| **Mixer** | Master Volume (`AVAILABLE`), Channel Strips (`PLANNED`), Stereo Meters (`DEMO`) | `AVAILABLE` / `PLANNED` / `DEMO` | Meters explicitly marked `DEMO SIM` (`widgets/signal_meter.cpp`) |
| **Arpeggiator (`ARP`)** | Up/Down/Random/As-Played, Division, Gate, Octave, Latch | `PLANNED` | `smk-s3/components/sequencer`, `specs.md` §13 |
| **Sequencer & Drums** | 16-Step Melodic Sequencer & 4-Track PCM Drum Matrix | `PLANNED` | `smk-s3/components/sequencer`, `src/pcm.cpp` |
| **Harmony Assist** | Scale Quantizer (`8` scales) & Chord Assist (`7` voicings) | `PLANNED` (Live preview active in `UiModel`) | `smk-s3/components/midi/include/scale_quantizer.h`, `chord_memory.h` |
| **MIDI Routing** | USB-MIDI Note/PitchBend/CC1/7/11/64 & Hot-plug Panic | `AVAILABLE` | `smk-p4/components/usb_midi_host/usb_midi_host.cpp` |
| **MIDI Routing** | Custom CC Learn & Velocity Curve Remapping | `PLANNED` | `smk-s3/components/midi` |
| **Display & Touch** | `800 × 480` RGB565 LVGL 9.3.0 + Backlight + LVGL Heap Monitor | `AVAILABLE` | `smk-p4/components/board/display_hal.cpp`, `lv_mem_monitor()` |

---

## 3. Canonical Parameter Registry (`132` Parameters in `ui_parameters.cpp`)

| Namespace | Count | `AVAILABLE` / `DEMO` Parameters | `PLANNED` Parameters | Dynamic `UNAVAILABLE` Rule |
|---|---|---|---|---|
| `perf.*` | 13 | `perf.patch_id`, `perf.master_volume`, `perf.octave_shift` | `perf.bpm`, `perf.layer_mode`, `perf.macro.1` .. `perf.macro.8` | — |
| `osc.*` | 9 | — | `osc.1.wave`, `osc.1.pitch_st`, `osc.1.detune_ct`, `osc.1.level`, `osc.1.pan`, `osc.1.unison`, `osc.1.pulse_width`, `osc.sub_level`, `osc.noise_level` | `osc.1.pulse_width` is `UNAVAILABLE` when `osc.1.wave != PULSE (1)` |
| `fm.*` | 8 | `fm.selected_op` (`DEMO`) | `fm.algo`, `fm.op.ratio`, `fm.op.level`, `fm.feedback`, `fm.op.attack`, `fm.op.decay`, `fm.op.vel_sens` | All `fm.*` are `UNAVAILABLE` when `osc.1.wave != FM ALGO (8)` / active patch family $\neq$ `FM` |
| `filter.*` | 6 | — | `filter.type`, `filter.cutoff_hz`, `filter.resonance`, `filter.drive`, `filter.keytrack`, `filter.env_amount` | Filter params are `UNAVAILABLE` when `filter.type == NONE (1)` or family lacks filter |
| `amp.*` | 4 | `amp.vel_sens` | `amp.level`, `amp.pan`, `amp.portamento_ms` | — |
| `env.*` | 5 | — | `env.amp.attack_ms`, `env.amp.decay_ms`, `env.amp.sustain`, `env.amp.release_ms`, `env.amp.curve` | — |
| `lfo.*` | 6 | `lfo.1.rate_hz`, `lfo.1.depth` | `lfo.1.wave`, `lfo.1.phase_deg`, `lfo.1.sync`, `lfo.1.dest` | — |
| `mod.*` | 4 | `mod.slot1.source`, `mod.slot1.dest` | `mod.slot1.amount`, `mod.slot1.polarity` | — |
| `layer_a.*` | 7 | `layer_a.enable`, `layer_a.patch_id` | `layer_a.channel`, `layer_a.octave`, `layer_a.transpose`, `layer_a.key_low`, `layer_a.key_high` | — |
| `layer_b.*` | 7 | — | `layer_b.enable`, `layer_b.patch_id`, `layer_b.channel`, `layer_b.octave`, `layer_b.transpose`, `layer_b.key_low`, `layer_b.key_high` | — |
| `mixer.*` | 9 | — | `mixer.layer_a.level`, `mixer.layer_a.pan`, `mixer.layer_a.mute`, `mixer.layer_b.level`, `mixer.layer_b.pan`, `mixer.layer_b.mute`, `mixer.drums.level`, `mixer.drums.mute`, `mixer.fx_send` | — |
| `fx.*` | 10 | — | `fx.chorus.mode`, `fx.chorus.depth`, `fx.delay.time_ms`, `fx.delay.feedback`, `fx.delay.mix`, `fx.reverb.size`, `fx.reverb.mix`, `fx.drive`, `fx.master_tone`, `fx.routing` | — |
| `arp.*` | 8 | — | `arp.enable`, `arp.clock_source`, `arp.rate`, `arp.gate_pct`, `arp.octaves`, `arp.direction`, `arp.latch`, `arp.swing_pct` | — |
| `seq.*` | 7 | — | `seq.playing`, `seq.pattern`, `seq.length`, `seq.step_vel`, `seq.step_prob`, `seq.step_ratchet`, `seq.transpose` | — |
| `drums.*` | 5 | — | `drums.track`, `drums.level`, `drums.pan`, `drums.tune_st`, `drums.decay_ms` | — |
| `harmony.*` | 12 | — | `harmony.scale_enable`, `harmony.root_note`, `harmony.scale_type`, `harmony.quant_mode`, `harmony.preserve_outside`, `harmony.chord_enable`, `harmony.chord_type`, `harmony.voicing`, `harmony.inversion`, `harmony.spread`, `harmony.vel_behavior`, `harmony.dest_layer` | Sub-parameters depend on `harmony.scale_enable` or `harmony.chord_enable` |
| `midi.*` | 7 | `midi.input_channel`, `midi.bend_range_st`, `midi.sustain_enable`, `midi.modwheel_enable` | `midi.vel_curve`, `midi.cc_learn_active`, `midi.clock_sync` | — |
| `sys.*` | 5 | `sys.sample_rate_hz` (`AVAILABLE`), `sys.brightness` (`DEMO`), `sys.theme` (`DEMO`) | `sys.master_tuning_hz`, `sys.boot_mode` | `sys.sample_rate_hz` is `kCapReadOnly` |
