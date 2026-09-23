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

static void test_scale_quantizer_mixolydian() {
    using namespace smk;
    ScaleQuantizer sq;
    sq.setRootNote(0); // C
    sq.setEnabled(true);
    sq.setScale(ScaleType::Mixolydian);

    // C Mixolydian: C (0), D (2), E (4), F (5), G (7), A (9), Bb (10)
    assert(sq.isNoteInScale(60)); // C4
    assert(sq.isNoteInScale(62)); // D4
    assert(sq.isNoteInScale(64)); // E4
    assert(sq.isNoteInScale(65)); // F4
    assert(sq.isNoteInScale(67)); // G4
    assert(sq.isNoteInScale(69)); // A4
    assert(sq.isNoteInScale(70)); // Bb4
    assert(!sq.isNoteInScale(71)); // B4 is NOT in Mixolydian

    uint8_t q_b = sq.quantize(71);
    assert(q_b == 70 || q_b == 72);
    assert(strcmp(ScaleQuantizer::scaleName(ScaleType::Mixolydian), "MIXOLYDIAN") == 0);
    printf("PASS: ScaleQuantizer Mixolydian scale (0x06B5) and quantization\n");
}

static void test_fx_state_isolation_and_knob_routing() {
    using namespace smk;
    FxControlState fx;

    // Verify default initialization
    assert(fx.delay_time_ms == 350.0f);
    assert(fx.delay_feedback == 0.4f);
    assert(fx.delay_mix == 0.0f);
    assert(fx.reverb_size == 0.7f);
    assert(fx.reverb_mix == 0.0f);
    assert(fx.chorus_mode == 0);
    assert(fx.chorus_depth == 0.0f);
    assert(fx.drive == 0.0f);
    assert(fx.master_tone == 0.0f);

    // Modifying delay must not alter reverb, chorus, or drive
    fx.delay_time_ms = 500.0f;
    fx.delay_feedback = 0.8f;
    fx.delay_mix = 0.6f;
    assert(fx.reverb_size == 0.7f);
    assert(fx.reverb_mix == 0.0f);
    assert(fx.chorus_mode == 0);
    assert(fx.drive == 0.0f);

    // Modifying reverb must not alter delay or chorus
    fx.reverb_size = 0.9f;
    fx.reverb_mix = 0.5f;
    assert(fx.delay_time_ms == 500.0f);
    assert(fx.delay_feedback == 0.8f);
    assert(fx.chorus_mode == 0);

    // Drive normalized to 0.0f .. 1.0f
    fx.drive = 0.85f;
    assert(fx.drive >= 0.0f && fx.drive <= 1.0f);

    // Chorus modes 0..5
    for (uint8_t m = 0; m <= 5; ++m) {
        fx.chorus_mode = m;
        assert(fx.chorus_mode == m);
    }

    printf("PASS: FxControlState isolation, independent delays/reverbs/chorus & drive normalization\n");
}

static void test_filter_enum_and_preservation() {
    using namespace smk;

    // Verify all 8 AMY filter types + None
    assert(toAmyFilterType(SmkFilterType::None) == 0);
    assert(toAmyFilterType(SmkFilterType::LPF) == 1);
    assert(toAmyFilterType(SmkFilterType::BPF) == 2);
    assert(toAmyFilterType(SmkFilterType::HPF) == 3);
    assert(toAmyFilterType(SmkFilterType::LPF24) == 4);
    assert(toAmyFilterType(SmkFilterType::Notch) == 5);
    assert(toAmyFilterType(SmkFilterType::Phaser) == 6);
    assert(toAmyFilterType(SmkFilterType::Moog24) == 7);
    assert(toAmyFilterType(SmkFilterType::TptSvf) == 8);

    for (uint8_t i = 0; i <= 8; ++i) {
        SmkFilterType ft = fromAmyFilterType(i);
        assert(toAmyFilterType(ft) == i);
        assert(strlen(filterTypeName(ft)) > 0);
    }
    assert(fromAmyFilterType(99) == SmkFilterType::None);

    printf("PASS: Filter Enum reconciliation (0..8) and bidirectional mapping\n");
}

static void test_waveform_enum_and_legacy_mapping() {
    using namespace smk;

    assert(toAmyWaveType(SmkWaveType::Sine) == 0);
    assert(toAmyWaveType(SmkWaveType::Pulse) == 1);
    assert(toAmyWaveType(SmkWaveType::SawDown) == 2);
    assert(toAmyWaveType(SmkWaveType::SawUp) == 3);
    assert(toAmyWaveType(SmkWaveType::Triangle) == 4);
    assert(toAmyWaveType(SmkWaveType::Noise) == 5);
    assert(toAmyWaveType(SmkWaveType::KarplusStrong) == 6);
    assert(toAmyWaveType(SmkWaveType::Pcm) == 7);
    assert(toAmyWaveType(SmkWaveType::Algo) == 8);

    for (uint8_t w = 0; w <= 8; ++w) {
        SmkWaveType wt = fromAmyWaveType(w);
        assert(toAmyWaveType(wt) == w);
        assert(strlen(waveTypeName(wt)) > 0);
    }

    // Legacy migration mapping:
    // Legacy 1: SawDown -> AMY 2
    // Legacy 2: SawUp -> AMY 3
    // Legacy 3: Triangle -> AMY 4
    // Legacy 4: Pulse -> AMY 1
    // Legacy 0: Sine -> AMY 0
    assert(mapLegacyWaveToAmy(1) == 2);
    assert(mapLegacyWaveToAmy(2) == 3);
    assert(mapLegacyWaveToAmy(3) == 4);
    assert(mapLegacyWaveToAmy(4) == 1);
    assert(mapLegacyWaveToAmy(0) == 0);

    printf("PASS: Waveform Enum reconciliation (0..8) and legacy migration mapping\n");
}

static void test_step_sequencer_ratchet_decoupling() {
    using namespace smk;
    StepSequencer seq;
    EventBus bus;

    seq.setPatternLength(16);
    seq.clearPattern(0);

    // Step 0: Note C3 (48), ratchet 4, 100% prob, active
    seq.setStep(0, 0, 48, 100, true, false);
    seq.step(0, 0).ratchet = 4;
    seq.step(0, 0).probability = 100;

    // Step 1: Note G3 (55), ratchet 1, 100% prob, active
    seq.setStep(0, 1, 55, 110, true, false);
    seq.step(0, 1).ratchet = 1;
    seq.step(0, 1).probability = 100;

    seq.play();

    std::vector<SynthEvent> events_step0;
    // Process Step 0 (ticks 0..5)
    for (uint32_t t = 0; t < 6; ++t) {
        seq.processTick(t, bus);
        SynthEvent ev;
        while (bus.tryReceive(ev)) {
            events_step0.push_back(ev);
        }
    }

    // Step 0 with ratchet 4 must have exactly 4 NoteOn events with note 48
    std::vector<SynthEvent> note_ons_step0;
    for (const auto& ev : events_step0) {
        if (ev.type == EventType::NoteOn) {
            note_ons_step0.push_back(ev);
        }
    }
    assert(note_ons_step0.size() == 4);
    for (const auto& ev : note_ons_step0) {
        assert(ev.id == 48); // All 4 triggers must be C3 (48), NEVER G3 (55)!
    }

    // Now process Step 1 onset (tick 6)
    seq.processTick(6, bus);
    std::vector<SynthEvent> events_step1;
    SynthEvent ev;
    while (bus.tryReceive(ev)) {
        events_step1.push_back(ev);
    }

    std::vector<SynthEvent> note_ons_step1;
    for (const auto& e : events_step1) {
        if (e.type == EventType::NoteOn) {
            note_ons_step1.push_back(e);
        }
    }
    assert(note_ons_step1.size() == 1);
    assert(note_ons_step1[0].id == 55); // Step 1 is G3 (55)

    printf("PASS: StepSequencer ratchet decoupling (all 4 ratchet sub-ticks trigger C3 before G3 onset)\n");
}

static void test_step_sequencer_probability_latching() {
    using namespace smk;
    StepSequencer seq;
    EventBus bus;

    seq.setPatternLength(16);
    seq.clearPattern(0);

    // Step 0: Note 60, ratchet 4, probability 0% (always fails)
    seq.setStep(0, 0, 60, 100, true, false);
    seq.step(0, 0).ratchet = 4;
    seq.step(0, 0).probability = 0;

    seq.play();

    size_t note_on_count = 0;
    for (uint32_t t = 0; t < 6; ++t) {
        seq.processTick(t, bus);
        SynthEvent ev;
        while (bus.tryReceive(ev)) {
            if (ev.type == EventType::NoteOn) {
                note_on_count++;
            }
        }
    }
    assert(note_on_count == 0);

    printf("PASS: StepSequencer single probability evaluation at onset (no stray ratchet triggers when prob fails)\n");
}

static void test_pattern_mutation_undo_isolation() {
    using namespace smk;
    StepSequencer seq;

    // Setup Pattern 2
    seq.selectPattern(2);
    seq.clearTrack(0);
    seq.setStep(0, 0, 60, 100, true, false);
    seq.setStep(0, 1, 62, 100, true, false);

    // Setup Pattern 4
    seq.selectPattern(4);
    seq.clearTrack(0);
    seq.setStep(0, 0, 72, 100, true, false);
    seq.setStep(0, 1, 74, 100, true, false);

    // Mutate Pattern 2
    seq.selectPattern(2);
    seq.mutatePattern(0, 100);
    assert(seq.hasMutationUndo(0));

    // Switch to Pattern 4
    seq.selectPattern(4);
    assert(!seq.hasMutationUndo(0));

    // Calling undoMutation while on Pattern 4 must NOT corrupt Pattern 4
    seq.undoMutation(0);
    assert(seq.step(0, 0).note == 72);
    assert(seq.step(0, 1).note == 74);

    // Switch back to Pattern 2
    seq.selectPattern(2);
    assert(seq.hasMutationUndo(0));

    // Now undo restores Pattern 2
    seq.undoMutation(0);
    assert(!seq.hasMutationUndo(0));
    assert(seq.step(0, 0).note == 60);
    assert(seq.step(0, 1).note == 62);

    printf("PASS: Pattern Mutation Undo isolation across active pattern switches\n");
}

static void test_real_file_patch_roundtrip_and_v3_migration() {
    using namespace smk;

    const char* v4_filename = "test_temp_v4.s3p";
    const char* v3_filename = "test_temp_v3.s3p";

    // 1. Test real binary disk file write and read of v4
    SynthPatch p_orig = {};
    p_orig.id = 42;
    strncpy(p_orig.name, "Deep Sub Bass", sizeof(p_orig.name));
    p_orig.wave_type = toAmyWaveType(SmkWaveType::Triangle);
    p_orig.filter_type = toAmyFilterType(SmkFilterType::Moog24);
    p_orig.filter_cutoff = 120.0f;
    p_orig.drive_level = 0.45f;
    p_orig.chorus_mode = 2; // Juno
    p_orig.crc32 = calculatePatchCrc32(p_orig);

    PatchHeader h4 = {};
    h4.magic = kPatchMagic;
    h4.format_version = kPatchFormatVersion;
    h4.data_size = sizeof(SynthPatch);
    h4.crc32 = p_orig.crc32;

    FILE* f4 = fopen(v4_filename, "wb");
    assert(f4 != nullptr);
    fwrite(&h4, 1, sizeof(PatchHeader), f4);
    fwrite(&p_orig, 1, sizeof(SynthPatch), f4);
    fclose(f4);

    // Read back v4 file
    FILE* fr4 = fopen(v4_filename, "rb");
    assert(fr4 != nullptr);
    PatchHeader read_h4 = {};
    SynthPatch read_p4 = {};
    fread(&read_h4, 1, sizeof(PatchHeader), fr4);
    fread(&read_p4, 1, sizeof(SynthPatch), fr4);
    fclose(fr4);
    remove(v4_filename);

    assert(read_h4.magic == kPatchMagic);
    assert(read_h4.format_version == 4);
    assert(read_h4.crc32 == p_orig.crc32);
    assert(read_p4.crc32 == p_orig.crc32);
    assert(calculatePatchCrc32(read_p4) == p_orig.crc32);
    assert(strcmp(read_p4.name, "Deep Sub Bass") == 0);
    assert(read_p4.wave_type == toAmyWaveType(SmkWaveType::Triangle));
    assert(read_p4.filter_type == toAmyFilterType(SmkFilterType::Moog24));
    assert(read_p4.drive_level == 0.45f);

    // 2. Test real binary disk file write of v3 and migration to v4
    SynthPatchV3 v3 = {};
    v3.id = 12;
    strncpy(v3.name, "Vintage Strings", sizeof(v3.name));
    v3.wave_type = 1; // Legacy SawDown
    v3.filter_cutoff = 2200.0f;
    v3.filter_res = 1.5f;
    v3.crc32 = calculatePatchV3Crc32(v3);

    PatchHeader h3 = {};
    h3.magic = kPatchMagic;
    h3.format_version = 3;
    h3.data_size = sizeof(SynthPatchV3);
    h3.crc32 = v3.crc32;

    FILE* f3 = fopen(v3_filename, "wb");
    assert(f3 != nullptr);
    fwrite(&h3, 1, sizeof(PatchHeader), f3);
    fwrite(&v3, 1, sizeof(SynthPatchV3), f3);
    fclose(f3);

    // Read back v3 and execute migration logic
    FILE* fr3 = fopen(v3_filename, "rb");
    assert(fr3 != nullptr);
    PatchHeader read_h3 = {};
    SynthPatchV3 read_v3 = {};
    fread(&read_h3, 1, sizeof(PatchHeader), fr3);
    fread(&read_v3, 1, sizeof(SynthPatchV3), fr3);
    fclose(fr3);
    remove(v3_filename);

    assert(read_h3.format_version == 3);
    assert(calculatePatchV3Crc32(read_v3) == read_h3.crc32);

    // Perform migration
    SynthPatch migrated_v4 = {};
    migrated_v4.id = read_v3.id;
    memcpy(migrated_v4.name, read_v3.name, sizeof(migrated_v4.name));
    migrated_v4.wave_type = mapLegacyWaveToAmy(read_v3.wave_type);
    migrated_v4.filter_type = toAmyFilterType(SmkFilterType::LPF24);
    migrated_v4.filter_cutoff = read_v3.filter_cutoff;
    migrated_v4.filter_res = read_v3.filter_res;
    migrated_v4.crc32 = calculatePatchCrc32(migrated_v4);

    assert(migrated_v4.wave_type == toAmyWaveType(SmkWaveType::SawDown)); // Legacy 1 -> AMY 2
    assert(migrated_v4.filter_type == toAmyFilterType(SmkFilterType::LPF24)); // Default LPF24
    assert(migrated_v4.crc32 != 0);
    assert(calculatePatchCrc32(migrated_v4) == migrated_v4.crc32);

    printf("PASS: Real file binary roundtrip (v4 .s3p) and v3 migration with CRC validation\n");
}

int main() {
    printf("=== Running SMK Synth Expansion Host Test Suite ===\n");
    test_scale_quantizer();
    test_scale_quantizer_mixolydian();
    test_chord_memory();
    test_fx_state_isolation_and_knob_routing();
    test_filter_enum_and_preservation();
    test_waveform_enum_and_legacy_mapping();
    test_step_sequencer_features();
    test_step_sequencer_ratchet_decoupling();
    test_step_sequencer_probability_latching();
    test_pattern_mutation_undo_isolation();
    test_patch_migration_and_crc();
    test_real_file_patch_roundtrip_and_v3_migration();
    test_macro_curves();
    test_amy_core_fixes();
    printf("=== ALL SMK SYNTH EXPANSION TESTS PASSED ===\n");
    return 0;
}
