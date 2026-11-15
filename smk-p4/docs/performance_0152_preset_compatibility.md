# SMK-P4 0.15.2 — Preset & MIDI Regression Qualification

## 1. Scope & Objective

This document qualifies preset persistence (V1, V2, V3, V4), transactional integrity, and MIDI behavior across SMK-P4 0.15.2 changes (Audio Owner Stack Safety, Delay Tap Refinement, and Performance Lock selective patch loading).

---

## 2. Preset Formats Qualification

| Format | Content | Behavior in 0.15.2 | Test Result |
| :--- | :--- | :--- | :--- |
| **V1** | Legacy 64-byte offsets | Decoded without loss; converted to current synthesis state | **PASS** (`gate_h_codec_hardening_test`) |
| **V2** | 160-byte dual-bank Juno/FM edits | Decoded and written to dual-bank ping-pong NVS slots | **PASS** (`gate_b_transaction_test`) |
| **V3** | FM Algorithm state | Algorithm numbers (1..32) preserved with token tracking | **PASS** (`gate_b_transaction_test`) |
| **V4** | Synth Parameters + Performance Macros + FX Record V2 | 2-phase transactional commit; CRC-verified | **PASS** (`gate_b_transaction_test`, `run_sound_editor.ps1`) |
| **FX Record V2** | Chorus, Delay FREE/SYNC, Reverb, EQ, Delay Division | Full round-trip persistence and recall | **PASS** (`fx_preset_persistence_test`) |

---

## 3. Transactional & Real-Time Invariants

1. **Dual-Bank Atomic Ping-Pong:** Slots 0..15 use dual-bank storage with monotonic sequence counter and CRC32 verification. Torn writes, simulated storage write failures, and out-of-space conditions safely roll back to the previously verified bank.
2. **Audio Worker Stack Safety:** By relocating `Snapshot state` (2,608 B) to PSRAM and eliminating unreferenced stack buffers, audio owner stack margin increased from 932 B to 4,564 B under full musical load (8 voices, 4-note harmony, arpeggiator, delay sync, chorus, reverb).
3. **No Allocation During Render:** Confirmed across all 50 live parameter changes, delay tap retargeting, and active playback.
4. **Panic & Invalidation:** Patch changes advance `commands_.panic(OutputReason::Patch)` and flush both touch and USB queues, ensuring zero stuck notes, voice leaks, or hanging sustain leases.
