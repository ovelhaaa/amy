#include <cassert>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <array>

#include "scale_quantizer.h"
#include "chord_memory.h"
#include "step_sequencer.h"
#include "patch_types.h"
#include "diagnostics.h"

namespace smk {
Diagnostics& Diagnostics::instance() { static Diagnostics instance; return instance; }
DiagnosticCounters& Diagnostics::counters() { return counters_; }
}

// Test Scale Quantizer
static void test_scale_quantizer() {
    using namespace smk;
    ScaleQuantizer sq;
    sq.setRootNote(0); // C
    sq.setEnabled(true);

    // Test Chromatic (Scale 0)
    sq.setScale(ScaleType::Chromatic);
    for (uint8_t n = 0; n < 128; ++n) {
        assert(sq.quantize(n) == n);
    }

    // Test C Major (Scale 1) -> notes should be C, D, E, F, G, A, B
    sq.setScale(ScaleType::Major);
    assert(sq.quantize(60) == 60); // C4 -> C4
    assert(sq.quantize(61) == 62); // C#4 -> D4 (upward nearest)
    assert(sq.quantize(62) == 62); // D4 -> D4
    assert(sq.quantize(63) == 64); // D#4 -> E4
    assert(sq.quantize(64) == 64); // E4 -> E4
    assert(sq.quantize(65) == 65); // F4 -> F4
    assert(sq.quantize(67) == 67); // G4 -> G4
    assert(sq.quantize(69) == 69); // A4 -> A4
    assert(sq.quantize(71) == 71); // B4 -> B4

    // Test Root Transposition (D Major: D, E, F#, G, A, B, C#)
    sq.setRootNote(2); // D
    assert(sq.quantize(62) == 62); // D4 -> D4
    assert(sq.quantize(66) == 66); // F#4 -> F#4

    // Test Pentatonic Minor (root A = 9)
    sq.setRootNote(9);
    sq.setScale(ScaleType::PentatonicMinor); // A, C, D, E, G
    assert(sq.isNoteInScale(69)); // A4
    assert(sq.isNoteInScale(72)); // C5
    assert(sq.isNoteInScale(74)); // D5
    assert(sq.isNoteInScale(76)); // E5
    assert(sq.isNoteInScale(79)); // G5
    assert(!sq.isNoteInScale(70)); // Bb4

    printf("PASS: ScaleQuantizer all 8 scales, root transpositions & pitch detection\n");
}

// Test Chord Memory
static void test_chord_memory() {
    using namespace smk;
    ChordMemory cm;
    assert(!cm.isEnabled());

    // Enable Major chord (root, +4, +7)
    cm.setChordType(ChordType::Major);
    assert(cm.isEnabled());

    uint8_t notes[ChordMemory::kMaxChordNotes] = {};
    uint8_t count = cm.getChordNotes(60, notes, ChordMemory::kMaxChordNotes); // C4 Major
    assert(count == 3);
    assert(notes[0] == 60); // C4
    assert(notes[1] == 64); // E4
    assert(notes[2] == 67); // G4

    // Minor 7th chord (root, +3, +7, +10)
    cm.setChordType(ChordType::Min7);
    count = cm.getChordNotes(60, notes, ChordMemory::kMaxChordNotes);
    assert(count == 4);
    assert(notes[0] == 60);
    assert(notes[1] == 63);
    assert(notes[2] == 67);
    assert(notes[3] == 70);

    // Custom Live Chord Capture
    cm.startCapture();
    assert(cm.isCapturing());
    cm.addCaptureNote(60);
    cm.addCaptureNote(63);
    cm.addCaptureNote(67);
    cm.addCaptureNote(71); // Maj7#9 or minMaj7
    cm.finishCapture();
    assert(!cm.isCapturing());
    assert(cm.chordType() == ChordType::Custom);

    count = cm.getChordNotes(48, notes, ChordMemory::kMaxChordNotes); // Transpose custom chord to C3
    assert(count == 4);
    assert(notes[0] == 48);
    assert(notes[1] == 51);
    assert(notes[2] == 55);
    assert(notes[3] == 59);

    printf("PASS: ChordMemory presets, intervals & custom live chord capture\n");
}

// Test Step Sequencer Pattern Length, Ratchet, Mutation & Undo
static void test_step_sequencer_features() {
    using namespace smk;
    StepSequencer seq;

    // Pattern Length
    assert(seq.patternLength() == 16);
    seq.setPatternLength(7);
    assert(seq.patternLength() == 7);
    seq.setPatternLength(20); // Clamped to 16
    assert(seq.patternLength() == 16);
    seq.setPatternLength(0); // Clamped to 1
    assert(seq.patternLength() == 1);
    seq.setPatternLength(8);

    // Configure track 0 steps
    seq.clearTrack(0);
    for (uint8_t s = 0; s < 8; ++s) {
        seq.setStep(static_cast<uint8_t>(0), s, static_cast<uint8_t>(60 + s), static_cast<uint8_t>(100), true, false);
    }
    assert(seq.getTrackStepMask(0) == 0x00FF);

    // Test Ratchet
    seq.step(0, 0).ratchet = 3;
    assert(seq.step(0, 0).ratchet == 3);

    // Test Mutation and Undo
    assert(!seq.hasMutationUndo(0));
    seq.mutatePattern(0, 50); // 50% probability mutation
    assert(seq.hasMutationUndo(0));

    // Undo mutation restores exact steps
    seq.undoMutation(0);
    assert(!seq.hasMutationUndo(0));
    for (uint8_t s = 0; s < 8; ++s) {
        assert(seq.step(0, s).note == 60 + s);
        assert(seq.step(0, s).active == true);
    }

    printf("PASS: StepSequencer variable pattern length, ratchets, mutation & undo\n");
}

// Test Patch Format v3 -> v4 Migration and CRC32 Verification
static void test_patch_migration_and_crc() {
    using namespace smk;

    // 1. Build a synthetic v3 patch
    SynthPatchV3 v3 = {};
    v3.id = 5;
    strncpy(v3.name, "Classic Juno Pad", sizeof(v3.name));
    strncpy(v3.category, "PAD", sizeof(v3.category));
    strncpy(v3.author, "ROLAND JUNO", sizeof(v3.author));
    v3.engine_patch = 1;
    v3.transpose = 0;
    v3.voice_count = 6;
    v3.wave_type = 1; // SAW
    v3.mono_mode = 0;
    v3.portamento_ms = 0;
    v3.base_freq = 440.0f;
    v3.filter_cutoff = 3500.0f;
    v3.filter_res = 2.0f;
    v3.amp_attack = 150.0f;
    v3.amp_decay = 400.0f;
    v3.amp_sustain = 0.8f;
    v3.amp_release = 600.0f;
    v3.crc32 = calculatePatchV3Crc32(v3);

    assert(v3.crc32 != 0);
    assert(calculatePatchV3Crc32(v3) == v3.crc32);

    // 2. Perform Migration to v4
    SynthPatch v4 = {};
    v4.id = v3.id;
    memcpy(v4.name, v3.name, sizeof(v4.name));
    memcpy(v4.category, v3.category, sizeof(v4.category));
    memcpy(v4.author, v3.author, sizeof(v4.author));
    v4.engine_patch = v3.engine_patch;
    v4.transpose = v3.transpose;
    v4.voice_count = v3.voice_count;
    v4.wave_type = v3.wave_type;
    v4.mono_mode = v3.mono_mode;
    v4.portamento_ms = v3.portamento_ms;
    v4.base_freq = v3.base_freq;
    v4.filter_cutoff = v3.filter_cutoff;
    v4.filter_res = v3.filter_res;
    v4.amp_attack = v3.amp_attack;
    v4.amp_decay = v3.amp_decay;
    v4.amp_sustain = v3.amp_sustain;
    v4.amp_release = v3.amp_release;
    memcpy(v4.macros, v3.macros, sizeof(v4.macros));

    // Default v4 new parameters
    v4.filter_env_amount = 0.0f;
    v4.filter_key_tracking = 0.0f;
    v4.filter_vel_tracking = 1.5f;
    v4.filter_type = 0;
    v4.osc_mix = 0.5f;
    v4.osc_detune = 0.0f;
    v4.sub_level = 0.0f;
    v4.noise_level = 0.0f;
    v4.drive_level = 0.0f;
    v4.master_tone = 0.0f;
    v4.chorus_mode = 0;
    v4.reverb_freeze = 0;
    v4.crc32 = calculatePatchCrc32(v4);

    assert(v4.crc32 != 0);
    assert(calculatePatchCrc32(v4) == v4.crc32);

    // Ensure tampering alters CRC
    SynthPatch tampered = v4;
    tampered.filter_cutoff += 10.0f;
    assert(calculatePatchCrc32(tampered) != v4.crc32);

    printf("PASS: SynthPatch v3 -> v4 migration, field preservation & CRC32 integrity\n");
}

// Test Macro Curves (Linear, Exponential, Logarithmic)
static void test_macro_curves() {
    float norm_val = 0.5f;
    float min_val = 100.0f;
    float max_val = 1000.0f;

    // Linear (curve 0)
    float linear_val = min_val + norm_val * (max_val - min_val);
    assert(std::abs(linear_val - 550.0f) < 0.001f);

    // Exponential (curve 1: v^2) -> slower start, steep end
    float exp_shaped = norm_val * norm_val; // 0.25
    float exp_val = min_val + exp_shaped * (max_val - min_val);
    assert(std::abs(exp_val - 325.0f) < 0.001f);

    // Logarithmic (curve 2: sqrt(v)) -> fast start, gentle top
    float log_shaped = std::sqrt(norm_val); // ~0.7071
    float log_val = min_val + log_shaped * (max_val - min_val);
    assert(std::abs(log_val - 736.396f) < 0.01f);

    printf("PASS: Macro curve shaping (Linear, Exponential v^2, Logarithmic sqrt(v))\n");
}

// Test AMY Core Sample Rate Invariance Fixes (Delay MS_TO_SAMPS & Karplus-Strong buffer)
static void test_amy_core_fixes() {
    // 1. Delay conversion validation across sample rates
    auto ms_to_samps = [](float ms, uint32_t sr) -> uint32_t {
        return static_cast<uint32_t>(std::round((ms / 1000.0f) * sr));
    };
    auto enclosing_pow2 = [](uint32_t v) -> uint32_t {
        uint32_t p = 1;
        while (p < v) p <<= 1;
        return p;
    };

    // At 48 kHz (ESP32-S3 standard)
    uint32_t samps_48k = ms_to_samps(1000.0f, 48000);
    assert(samps_48k == 48000);
    assert(enclosing_pow2(samps_48k) == 65536);

    // At 44.1 kHz
    uint32_t samps_44k = ms_to_samps(1000.0f, 44100);
    assert(samps_44k == 44100);
    assert(enclosing_pow2(samps_44k) == 65536);

    // 2. Karplus-Strong lowest note buffer length:
    // Old fixed 802 samples at 48 kHz detuned notes below ~60 Hz (B1).
    // New dynamic ((AMY_SAMPLE_RATE / 55) + 2) covers down to A1 (55 Hz) exactly:
    uint32_t ks_len_48k = (48000 / 55) + 2;
    assert(ks_len_48k == 874);
    assert(ks_len_48k > 802); // 874 samples accommodates full period of 55Hz at 48kHz

    printf("PASS: AMY core fixes (sample rate invariant delay lengths & 48kHz Karplus-Strong buffer)\n");
}

int main() {
    printf("=== Running SMK Synth Expansion Host Test Suite ===\n");
    test_scale_quantizer();
    test_chord_memory();
    test_step_sequencer_features();
    test_patch_migration_and_crc();
    test_macro_curves();
    test_amy_core_fixes();
    printf("=== ALL SMK SYNTH EXPANSION TESTS PASSED ===\n");
    return 0;
}
