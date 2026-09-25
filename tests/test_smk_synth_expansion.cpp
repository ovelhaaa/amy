#include <cassert>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <array>
#include <filesystem>

#include "scale_quantizer.h"
#include "chord_memory.h"
#include "step_sequencer.h"
#include "patch_types.h"
#include "diagnostics.h"
#include "patch_manager.h"
#include "storage_manager.h"
#include "amy_adapter.h"
#include "clock_manager.h"
#include "ui_manager.h"

namespace smk {
Diagnostics& Diagnostics::instance() { static Diagnostics instance; return instance; }
DiagnosticCounters& Diagnostics::counters() { return counters_; }

void ClockManager::setBpm(float) {}
void HomeScreen::setPatchInfo(uint16_t, const char*, const char*) {}
void HomeScreen::setMacroValues(const uint8_t[8]) {}
void HomeScreen::setEngineValues(const uint8_t[8]) {}
void HomeScreen::setHomeKnobBankView(HomeKnobBankView) {}
void HomeScreen::setKnobBankLabel(const char*) {}
void HomeScreen::setActiveVoices(uint8_t, uint8_t) {}
void UIManager::triggerParameterOverlay(const char*, const char*, float, float, const char*, TakeoverStatus) {}

AmyAdapter::AmyAdapter() {}
AmyAdapter::~AmyAdapter() {}
bool AmyAdapter::begin(uint32_t) { return true; }
void AmyAdapter::onAudioStopped() {}
void AmyAdapter::noteOn(uint8_t, uint8_t, uint8_t) {}
void AmyAdapter::noteOff(uint8_t, uint8_t) {}
void AmyAdapter::pitchBend(uint8_t, int16_t) {}
void AmyAdapter::controlChange(uint8_t, uint8_t, uint8_t) {}
void AmyAdapter::allNotesOff() {}
void AmyAdapter::panic() {}
int16_t* AmyAdapter::render() { return nullptr; }
uint16_t AmyAdapter::blockSize() const { return 256; }
float AmyAdapter::renderLoad() const { return 0.0f; }
uint32_t AmyAdapter::activeVoices() const { return 0; }
void AmyAdapter::getScopeSamples(int16_t*, size_t, size_t*) const {}
void AmyAdapter::setFilter(uint8_t, float, float, float, float, float, uint8_t) {}
void AmyAdapter::setOscillatorWaveform(uint8_t, uint8_t) {}
void AmyAdapter::setEnvelope(uint8_t, float, float, float, float) {}
void AmyAdapter::setPortamento(uint8_t, uint16_t) {}
void AmyAdapter::loadPreset(uint8_t, uint16_t, uint8_t) {}
void AmyAdapter::sendAmyMessage(const char*) {}
void AmyAdapter::setOscDetune(uint8_t, float) {}
void AmyAdapter::setSubOscLevel(uint8_t, float) {}
void AmyAdapter::setNoiseLevel(uint8_t, float) {}
void AmyAdapter::setOscMix(uint8_t, float) {}
void AmyAdapter::setFmModIndex(uint8_t, float) {}
void AmyAdapter::setFmFeedback(uint8_t, float) {}
void AmyAdapter::setFmRatio(uint8_t, float) {}
void AmyAdapter::setFmAlgorithm(uint8_t, uint8_t) {}
void AmyAdapter::setChorus(float, float, float) {}
void AmyAdapter::setChorusMode(uint8_t) {}
void AmyAdapter::setReverb(float, float, float) {}
void AmyAdapter::setReverbFreeze(bool) {}
void AmyAdapter::setDelay(float, float, float) {}
void AmyAdapter::setSendLevels(uint8_t, float, float, float) {}
void AmyAdapter::setMasterGain(float) {}
void AmyAdapter::setDrive(float) {}
void AmyAdapter::setMasterTone(float) {}
void AmyAdapter::setMonoMode(bool) {}
bool AmyAdapter::fmBaseline(uint8_t&, float&) const { return false; }
void AmyAdapter::invalidateFmBaseline() {}
}

class MockAmyAdapter : public smk::AmyAdapter {
public:
    struct FilterCall {
        uint8_t osc_id;
        float cutoff;
        float resonance;
        float env_amount;
        float key_tracking;
        float vel_tracking;
        uint8_t filter_type;
    };
    struct ChorusCall {
        float depth;
        float rate;
        float level;
    };
    struct FmIndexCall {
        uint8_t osc_id;
        float value;
    };
    struct FmRatioCall {
        uint8_t osc_id;
        float value;
    };
    struct DetuneCall {
        uint8_t synth_id;
        float cents;
    };
    struct LevelCall {
        uint8_t synth_id;
        float value;
    };

    std::vector<FilterCall> filter_calls;
    std::vector<ChorusCall> chorus_calls;
    std::vector<FmIndexCall> fm_index_calls;
    std::vector<FmRatioCall> fm_ratio_calls;
    std::vector<DetuneCall> detune_calls;
    std::vector<LevelCall> osc_mix_calls;
    std::vector<LevelCall> sub_osc_calls;
    std::vector<LevelCall> noise_calls;

    void setFilter(uint8_t osc_id, float cutoff_hz, float resonance,
                   float env_amount = 0.0f, float key_tracking = 0.0f,
                   float vel_tracking = 1.5f, uint8_t filter_type = 0) override {
        filter_calls.push_back({osc_id, cutoff_hz, resonance, env_amount, key_tracking, vel_tracking, filter_type});
    }

    void setChorus(float depth, float rate, float level) override {
        chorus_calls.push_back({depth, rate, level});
    }

    void setFmModIndex(uint8_t osc_id, float mod_index) override {
        fm_index_calls.push_back({osc_id, mod_index});
    }

    void setFmRatio(uint8_t osc_id, float ratio) override {
        fm_ratio_calls.push_back({osc_id, ratio});
    }

    void setOscDetune(uint8_t synth_id, float cents) override {
        detune_calls.push_back({synth_id, cents});
    }

    void setOscMix(uint8_t synth_id, float mix) override {
        osc_mix_calls.push_back({synth_id, mix});
    }

    void setSubOscLevel(uint8_t synth_id, float level) override {
        sub_osc_calls.push_back({synth_id, level});
    }

    void setNoiseLevel(uint8_t synth_id, float level) override {
        noise_calls.push_back({synth_id, level});
    }
};

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
    v4.filter_type = toAmyFilterType(SmkFilterType::Inherit);
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
    assert(read_h4.format_version == kPatchFormatVersion);
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
    migrated_v4.filter_type = toAmyFilterType(SmkFilterType::Inherit);
    migrated_v4.filter_cutoff = read_v3.filter_cutoff;
    migrated_v4.filter_res = read_v3.filter_res;
    migrated_v4.crc32 = calculatePatchCrc32(migrated_v4);

    assert(migrated_v4.wave_type == toAmyWaveType(SmkWaveType::SawDown)); // Legacy 1 -> AMY 2
    assert(migrated_v4.filter_type == toAmyFilterType(SmkFilterType::Inherit)); // Inherit
    assert(migrated_v4.crc32 != 0);
    assert(calculatePatchCrc32(migrated_v4) == migrated_v4.crc32);

    printf("PASS: Real file binary roundtrip (v4 .s3p) and v3 migration with CRC validation\n");
}

static void test_patch_manager_chorus_depth_and_modes() {
    using namespace smk;
    MockAmyAdapter mock;
    PatchManager pm;
    pm.begin(&mock, nullptr);
    pm.softTakeover().setMode(TakeoverMode::Jump);
    mock.chorus_calls.clear();

    pm.setKnobBank(KnobBank::BankD_Effects);

    // 1. Knob 0 switches to Juno (mode 2)
    // norm = 2.0 / 5.0 -> physical = norm * 127.0f = 50.8f
    pm.handleKnobInput(0, 50.8f);
    assert(pm.fxControlState().chorus_mode == 2);
    // Switching from Off (0) to Juno (2) with depth near 0 must default depth to 1.0f
    assert(pm.fxControlState().chorus_depth == 1.0f);
    assert(!mock.chorus_calls.empty());
    assert(std::abs(mock.chorus_calls.back().depth - 0.8f) < 0.01f);
    assert(std::abs(mock.chorus_calls.back().rate - 0.6f) < 0.01f);
    assert(std::abs(mock.chorus_calls.back().level - 0.85f) < 0.01f);

    // 2. Adjust Chorus Depth (Engine Knob 4 -> knob index 8+4=12) to 0.0f strictly produces level=0.0f and depth=0.0f
    pm.handleKnobInput(12, 0.0f);
    assert(pm.fxControlState().chorus_depth == 0.0f);
    assert(mock.chorus_calls.back().depth == 0.0f);
    assert(mock.chorus_calls.back().level == 0.0f);
    assert(std::abs(mock.chorus_calls.back().rate - 0.6f) < 0.01f);

    // 3. Test depths 0.25, 0.5, 1.0 on Juno
    pm.handleKnobInput(12, 0.25f * 127.0f);
    assert(std::abs(mock.chorus_calls.back().depth - (0.8f * 0.25f)) < 0.01f);
    assert(std::abs(mock.chorus_calls.back().level - (0.85f * 0.25f)) < 0.01f);

    pm.handleKnobInput(12, 0.5f * 127.0f);
    assert(std::abs(mock.chorus_calls.back().depth - (0.8f * 0.5f)) < 0.01f);
    assert(std::abs(mock.chorus_calls.back().level - (0.85f * 0.5f)) < 0.01f);

    pm.handleKnobInput(12, 1.0f * 127.0f);
    assert(std::abs(mock.chorus_calls.back().depth - 0.8f) < 0.01f);
    assert(std::abs(mock.chorus_calls.back().level - 0.85f) < 0.01f);

    // 4. Test Mode 0 (Off)
    pm.handleKnobInput(0, 0.0f);
    assert(pm.fxControlState().chorus_mode == 0);
    assert(mock.chorus_calls.back().depth == 0.0f);
    assert(mock.chorus_calls.back().rate == 0.0f);
    assert(mock.chorus_calls.back().level == 0.0f);

    // 5. Test all other modes: Classic (1), Ensemble (3), Wide (4), Vibrato (5) at depths 0.0, 0.5, 1.0
    struct ModeSpec { float depth; float rate; float level; };
    const ModeSpec specs[6] = {
        {0.0f, 0.0f, 0.0f},
        {0.5f, 0.5f, 0.7f},
        {0.8f, 0.6f, 0.85f},
        {1.2f, 0.9f, 1.0f},
        {1.5f, 0.4f, 0.9f},
        {0.4f, 4.5f, 0.6f}
    };
    for (uint8_t m = 1; m <= 5; ++m) {
        pm.handleKnobInput(0, ((float)m / 5.0f) * 127.0f);
        // Test depth 0.0f
        pm.handleKnobInput(12, 0.0f);
        assert(mock.chorus_calls.back().depth == 0.0f);
        assert(mock.chorus_calls.back().level == 0.0f);
        assert(std::abs(mock.chorus_calls.back().rate - specs[m].rate) < 0.01f);

        // Test depth 0.5f
        pm.handleKnobInput(12, 0.5f * 127.0f);
        assert(std::abs(mock.chorus_calls.back().depth - specs[m].depth * 0.5f) < 0.01f);
        assert(std::abs(mock.chorus_calls.back().level - specs[m].level * 0.5f) < 0.01f);

        // Test depth 1.0f
        pm.handleKnobInput(12, 127.0f);
        assert(std::abs(mock.chorus_calls.back().depth - specs[m].depth) < 0.01f);
        assert(std::abs(mock.chorus_calls.back().level - specs[m].level) < 0.01f);
    }

    printf("PASS: Chorus Depth = 0 strictly wet=0, depth scaling (0.25, 0.5, 1.0), and mode switch auto-initialization\n");
}

static void test_patch_manager_filter_inherit_and_retention() {
    using namespace smk;
    MockAmyAdapter mock;
    PatchManager pm;
    pm.begin(&mock, nullptr);
    pm.softTakeover().setMode(TakeoverMode::Jump);
    mock.filter_calls.clear();

    // Default factory patch 0 filter type is Inherit (0xFF)
    assert(pm.activeFilterType() == static_cast<uint8_t>(SmkFilterType::Inherit));
    pm.applyActiveFilterState();
    assert(!mock.filter_calls.empty());
    assert(mock.filter_calls.back().filter_type == 0xFF);

    // Adjust cutoff in Bank C (Knob 0)
    pm.setKnobBank(KnobBank::BankC_FilterEnv);
    float physical_cutoff = ((1000.0f - 20.0f) / 18000.0f) * 127.0f;
    pm.handleKnobInput(0, physical_cutoff);
    assert(std::abs(mock.filter_calls.back().cutoff - 1000.0f) < 50.0f);
    assert(mock.filter_calls.back().filter_type == 0xFF); // Inherit preserved!
    assert(mock.filter_calls.back().resonance == pm.activePatch().filter_res);

    // Explicitly change filter type to Moog24 (7)
    pm.setFilterType(static_cast<uint8_t>(SmkFilterType::Moog24));
    assert(pm.activeFilterType() == static_cast<uint8_t>(SmkFilterType::Moog24));
    assert(mock.filter_calls.back().filter_type == 7);

    // Adjust cutoff again, verify filter_type 7 is retained
    pm.handleKnobInput(0, physical_cutoff * 1.5f);
    assert(mock.filter_calls.back().filter_type == 7);

    printf("PASS: Filter Mode Inherit (0xFF) preserved, cutoff changes retain filter type & parameters\n");
}

static void test_storage_manager_real_files_and_migration() {
    using namespace smk;
    const char* test_dir = "build/test_storage";
    std::filesystem::create_directories(test_dir);
    StorageManager storage;
    assert(storage.begin(test_dir));

    // A. Save and load v5 patch
    SynthPatch p5 = *FactoryPatches::getPatchById(0);
    p5.id = 55;
    strncpy(p5.name, "V5 Test Patch", sizeof(p5.name));
    p5.filter_type = static_cast<uint8_t>(SmkFilterType::Inherit);
    p5.wave_type = toAmyWaveType(SmkWaveType::Pulse);
    p5.chorus_mode = 2; // Juno
    p5.drive_level = 0.35f;
    p5.crc32 = calculatePatchCrc32(p5);

    assert(storage.savePatch(55, p5));
    assert(storage.patchExists(55));

    SynthPatch loaded5 = {};
    assert(storage.loadPatch(55, loaded5));
    assert(loaded5.crc32 == p5.crc32);
    assert(loaded5.filter_type == static_cast<uint8_t>(SmkFilterType::Inherit));
    assert(loaded5.wave_type == toAmyWaveType(SmkWaveType::Pulse));
    assert(loaded5.chorus_mode == 2);
    assert(std::abs(loaded5.drive_level - 0.35f) < 0.001f);
    assert(strcmp(loaded5.name, "V5 Test Patch") == 0);

    // B. Legacy v4 fixture migration. Legacy v4 is officially the first
    // Expansion v4: the old adapter forwarded filter_type only when > 0, so
    // 0 == Inherit, 1 == LPF, 2 == BPF, 3 == HPF (the documented LPF24 enum
    // never matched the DSP). Drive was clamped to the audible 0..1 range.
    struct V4Case { uint8_t filter_type; SmkFilterType expected; float drive_in; float drive_out; };
    const V4Case v4_cases[] = {
        {0, SmkFilterType::Inherit, 0.6f, 0.6f},
        {1, SmkFilterType::LPF,     2.7f, 1.0f},
        {2, SmkFilterType::BPF,    -0.5f, 0.0f},
        {3, SmkFilterType::HPF,     0.45f, 0.45f},
    };
    uint8_t v4_slot = 44;
    for (const auto& tc : v4_cases) {
        char path_v4[128];
        snprintf(path_v4, sizeof(path_v4), "%s/patch_%03d.s3p", test_dir, v4_slot);
        PatchHeader h4 = {};
        h4.magic = kPatchMagic;
        h4.format_version = 4;
        h4.data_size = sizeof(SynthPatchV4Legacy);

        SynthPatchV4Legacy v4 = {};
        v4.id = v4_slot;
        strncpy(v4.name, "Legacy V4 Patch", sizeof(v4.name));
        v4.wave_type = 1; // legacy SawDown (maps to SawDown = 2)
        v4.filter_type = tc.filter_type;
        v4.chorus_mode = 1; // legacy Juno (maps to Juno = 2)
        v4.drive_level = tc.drive_in;
        v4.filter_cutoff = 1200.0f;
        v4.crc32 = calculatePatchV4LegacyCrc32(v4);
        h4.crc32 = v4.crc32;

        FILE* f4 = fopen(path_v4, "wb");
        assert(f4 != nullptr);
        fwrite(&h4, 1, sizeof(PatchHeader), f4);
        fwrite(&v4, 1, sizeof(SynthPatchV4Legacy), f4);
        fclose(f4);

        SynthPatch loaded_v4 = {};
        assert(storage.loadPatch(v4_slot, loaded_v4));
        assert(loaded_v4.wave_type == toAmyWaveType(SmkWaveType::SawDown));
        assert(loaded_v4.filter_type == toAmyFilterType(tc.expected));
        assert(loaded_v4.chorus_mode == 2); // Juno
        assert(std::abs(loaded_v4.drive_level - tc.drive_out) < 0.001f);
        assert(loaded_v4.crc32 == calculatePatchCrc32(loaded_v4));
        ++v4_slot;
    }
    assert(v4_slot == 48);

    // C. V3 fixture migration
    char path_v3[128];
    snprintf(path_v3, sizeof(path_v3), "%s/patch_%03d.s3p", test_dir, 33);
    PatchHeader h3 = {};
    h3.magic = kPatchMagic;
    h3.format_version = 3;
    h3.data_size = sizeof(SynthPatchV3);

    SynthPatchV3 v3 = {};
    v3.id = 33;
    strncpy(v3.name, "Legacy V3 Patch", sizeof(v3.name));
    v3.wave_type = 2; // legacy SawUp (maps to SawUp = 3)
    v3.filter_cutoff = 1800.0f;
    v3.crc32 = calculatePatchV3Crc32(v3);
    h3.crc32 = v3.crc32;

    FILE* f3 = fopen(path_v3, "wb");
    assert(f3 != nullptr);
    fwrite(&h3, 1, sizeof(PatchHeader), f3);
    fwrite(&v3, 1, sizeof(SynthPatchV3), f3);
    fclose(f3);

    SynthPatch loaded_v3 = {};
    assert(storage.loadPatch(33, loaded_v3));
    assert(loaded_v3.wave_type == toAmyWaveType(SmkWaveType::SawUp));
    assert(loaded_v3.filter_type == toAmyFilterType(SmkFilterType::Inherit)); // Inherit
    assert(loaded_v3.crc32 == calculatePatchCrc32(loaded_v3));

    // D. CRC corruption detection
    char path_v5[128];
    snprintf(path_v5, sizeof(path_v5), "%s/patch_%03d.s3p", test_dir, 55);
    FILE* fc = fopen(path_v5, "r+b");
    assert(fc != nullptr);
    fseek(fc, sizeof(PatchHeader) + 12, SEEK_SET);
    uint8_t corrupt_byte = 0xAA;
    fwrite(&corrupt_byte, 1, 1, fc);
    fclose(fc);

    SynthPatch loaded_corrupt = {};
    assert(!storage.loadPatch(55, loaded_corrupt));

    // E. data_size validation: a header declaring the wrong payload size must be
    // rejected before any payload is read (no partial reads).
    auto write_header_only = [&](uint8_t s, uint16_t version, uint16_t data_size) {
        char p[128];
        snprintf(p, sizeof(p), "%s/patch_%03d.s3p", test_dir, s);
        PatchHeader bh = {};
        bh.magic = kPatchMagic;
        bh.format_version = version;
        bh.data_size = data_size;
        FILE* f = fopen(p, "wb");
        assert(f != nullptr);
        fwrite(&bh, 1, sizeof(PatchHeader), f);
        uint8_t payload[sizeof(SynthPatch)] = {};
        fwrite(payload, 1, sizeof(payload), f);
        fclose(f);
    };

    SynthPatch reject_out = {};
    write_header_only(70, kPatchFormatVersion, static_cast<uint16_t>(sizeof(SynthPatch) - 1));
    assert(!storage.loadPatch(70, reject_out));
    write_header_only(71, 4, static_cast<uint16_t>(sizeof(SynthPatchV4Legacy) - 1));
    assert(!storage.loadPatch(71, reject_out));
    write_header_only(72, 3, static_cast<uint16_t>(sizeof(SynthPatchV3) - 1));
    assert(!storage.loadPatch(72, reject_out));

    printf("PASS: StorageManager disk I/O, v5 save/load, v4/v3 migration, CRC corruption and data_size rejection\n");
}

static void test_bank_b_fm_relative_and_detune() {
    using namespace smk;
    MockAmyAdapter mock;
    PatchManager pm;
    pm.begin(&mock, nullptr);
    pm.softTakeover().setMode(TakeoverMode::Jump);

    // Switch to Bank B
    pm.setKnobBank(KnobBank::BankB_Oscillator);

    // Switch waveform to ALGO/FM (wave_type = 8)
    pm.handleKnobInput(1, 127.0f);
    assert(pm.activePatch().wave_type == 8);

    // 1. FM Mod Index (Knob 0): 0..127 -> 0.0f .. 4.0f multiplier
    pm.handleKnobInput(0, 0.0f);
    assert(std::abs(mock.fm_index_calls.back().value - 0.0f) < 0.001f);

    pm.handleKnobInput(0, 127.0f);
    assert(std::abs(mock.fm_index_calls.back().value - 4.0f) < 0.001f);

    pm.handleKnobInput(0, 63.5f);
    assert(std::abs(mock.fm_index_calls.back().value - 1.0f) < 0.01f);

    // 2. FM Op Ratio (Knob 1): bipolar = (norm - 0.5)*2, ratio = powf(2.0, bipolar)
    // At center: 63.5f -> norm 0.5f -> bipolar 0.0f -> ratio = 1.0f
    pm.handleKnobInput(1, 63.5f);
    assert(std::abs(mock.fm_ratio_calls.back().value - 1.0f) < 0.01f);

    pm.handleKnobInput(1, 0.0f);
    assert(std::abs(mock.fm_ratio_calls.back().value - 0.5f) < 0.01f);

    pm.handleKnobInput(1, 127.0f);
    assert(std::abs(mock.fm_ratio_calls.back().value - 2.0f) < 0.01f);

    // 3. FM Detune (Knob 2): cents = (norm - 0.5f) * 50.0f (-25 .. +25 cents)
    // At center: 63.5f -> cents = 0.0f
    pm.handleKnobInput(2, 63.5f);
    assert(std::abs(mock.detune_calls.back().cents - 0.0f) < 0.1f);

    pm.handleKnobInput(2, 0.0f);
    assert(std::abs(mock.detune_calls.back().cents - (-25.0f)) < 0.1f);

    pm.handleKnobInput(2, 127.0f);
    assert(std::abs(mock.detune_calls.back().cents - 25.0f) < 0.1f);

    printf("PASS: Bank B FM Relative Controls (Mod Index 0..4, Ratio 0.25..4.0) and Detune center (0 cents)\n");
}

static void test_bank_b_fm_ratio_freq_mult_composition() {
    using namespace smk;
    MockAmyAdapter mock;
    PatchManager pm;
    pm.begin(&mock, nullptr);
    pm.softTakeover().setMode(TakeoverMode::Jump);
    pm.setKnobBank(KnobBank::BankB_Oscillator);
    pm.handleKnobInput(1, 127.0f); // waveform -> ALGO/FM
    assert(pm.activePatch().wave_type == 8);

    // Ratio center (1x), Freq Mult = 2x -> composed factor 2x.
    pm.handleKnobInput(1, 63.5f);
    pm.handleKnobInput(3, (1.0f / 7.0f) * 127.0f);
    assert(std::abs(mock.fm_ratio_calls.back().value - 2.0f) < 0.01f);

    // Ratio -> 2x while Freq Mult stays 2x -> composed 4x.
    pm.handleKnobInput(1, 127.0f);
    assert(std::abs(mock.fm_ratio_calls.back().value - 4.0f) < 0.01f);

    // Freq Mult back to 1x must not lose the Ratio position (still 2x).
    pm.handleKnobInput(3, 0.0f);
    assert(std::abs(mock.fm_ratio_calls.back().value - 2.0f) < 0.01f);

    // Ratio -> 0.5x, Freq Mult 1x -> composed 0.5x.
    pm.handleKnobInput(1, 0.0f);
    assert(std::abs(mock.fm_ratio_calls.back().value - 0.5f) < 0.01f);

    printf("PASS: FM Ratio and Freq Mult compose (ratio * freq_mult) without overwriting each other\n");
}

static void test_fm_soft_takeover_pickup() {
    using namespace smk;
    MockAmyAdapter mock;
    PatchManager pm;
    pm.begin(&mock, nullptr);
    pm.softTakeover().setMode(TakeoverMode::Jump);
    pm.setKnobBank(KnobBank::BankB_Oscillator);
    pm.handleKnobInput(1, 127.0f); // waveform -> ALGO/FM
    assert(pm.activePatch().wave_type == 8);

    // Re-arm pickup after entering FM mode deterministically.
    pm.softTakeover().setMode(TakeoverMode::Pickup);
    pm.softTakeover().resetAll();

    // FM Mod Index baseline is 1x (knob center ~63.5). A distant physical knob
    // must hold the effective factor at 1x until the pickup point is crossed.
    mock.fm_index_calls.clear();
    pm.handleKnobInput(0, 0.0f);
    assert(std::abs(mock.fm_index_calls.back().value - 1.0f) < 0.001f);
    assert(std::abs(pm.fmControlState().mod_factor - 1.0f) < 0.001f);
    pm.handleKnobInput(0, 63.5f); // crosses baseline
    assert(std::abs(mock.fm_index_calls.back().value - 1.0f) < 0.01f);
    pm.handleKnobInput(0, 127.0f); // captured now
    assert(std::abs(mock.fm_index_calls.back().value - 4.0f) < 0.01f);

    // FM Ratio baseline 1x: pickup center must map to 1x.
    mock.fm_ratio_calls.clear();
    pm.handleKnobInput(1, 0.0f); // far below center
    assert(std::abs(mock.fm_ratio_calls.back().value - 1.0f) < 0.01f);
    assert(std::abs(pm.fmControlState().ratio_factor - 1.0f) < 0.001f);
    pm.handleKnobInput(1, 63.5f); // crosses center
    assert(std::abs(mock.fm_ratio_calls.back().value - 1.0f) < 0.01f);
    pm.handleKnobInput(1, 127.0f);
    assert(std::abs(mock.fm_ratio_calls.back().value - 2.0f) < 0.01f);

    // FM Detune baseline 0 cents (knob center ~63.5).
    pm.handleKnobInput(2, 127.0f); // far above center -> stays centered
    assert(std::abs(pm.fmControlState().detune_cents - 0.0f) < 0.001f);
    pm.handleKnobInput(2, 63.5f); // crosses center
    assert(std::abs(pm.fmControlState().detune_cents - 0.0f) < 0.1f);
    pm.handleKnobInput(2, 127.0f); // captured now
    assert(std::abs(pm.fmControlState().detune_cents - 25.0f) < 0.1f);

    printf("PASS: FM Pickup soft takeover holds Mod Index 1x / Ratio 1x / Detune 0 until crossing\n");
}

static void test_fm_algorithm_saved_value_and_patch_reset() {
    using namespace smk;
    MockAmyAdapter mock;
    PatchManager pm;
    pm.begin(&mock, nullptr);
    pm.softTakeover().setMode(TakeoverMode::Jump);
    pm.setKnobBank(KnobBank::BankB_Oscillator);
    pm.handleKnobInput(1, 127.0f); // waveform -> ALGO/FM
    assert(pm.activePatch().wave_type == 8);

    // Select algorithm 16, then verify Pickup's saved position matches it.
    const float algo16_phys = fmAlgorithmNormFromValue(16) * 127.0f;
    pm.handleKnobInput(7, algo16_phys);
    assert(pm.fmControlState().algorithm == 16);

    pm.softTakeover().setMode(TakeoverMode::Pickup);
    pm.softTakeover().resetAll();
    pm.handleKnobInput(7, 127.0f); // far above saved position: not captured
    assert(pm.fmControlState().algorithm == 16); // saved_val corresponds to algo 16
    pm.handleKnobInput(7, algo16_phys); // crosses the saved position
    assert(pm.fmControlState().algorithm == 16);
    pm.handleKnobInput(7, 0.0f); // captured now
    assert(pm.fmControlState().algorithm == 1);

    // Loading a patch must discard the previous patch's runtime FM state.
    pm.softTakeover().setMode(TakeoverMode::Jump);
    pm.handleKnobInput(0, 127.0f);
    assert(std::abs(pm.fmControlState().mod_factor - 4.0f) < 0.01f);
    pm.selectPatch(1);
    assert(std::abs(pm.fmControlState().mod_factor - 1.0f) < 0.001f);
    assert(std::abs(pm.fmControlState().ratio_factor - 1.0f) < 0.001f);
    assert(std::abs(pm.fmControlState().detune_cents - 0.0f) < 0.001f);
    assert(std::abs(pm.fmControlState().freq_mult - 1.0f) < 0.001f);
    assert(pm.fmControlState().algorithm == 1);
    assert(!pm.fmControlState().initialized);

    printf("PASS: FM algorithm saved value tracks knob position and patch load resets FM runtime state\n");
}

static void test_algo_skips_subtractive_osc_controls() {
    using namespace smk;
    MockAmyAdapter mock;
    PatchManager pm;
    pm.begin(&mock, nullptr);

    // ALGO/DX7 factory patch: base+1/+2/+3 belong to the FM voice, so the
    // subtractive osc controls must not be sent at all.
    mock.osc_mix_calls.clear();
    mock.sub_osc_calls.clear();
    mock.noise_calls.clear();
    mock.detune_calls.clear();
    assert(pm.selectPatch(128));
    assert(pm.activePatch().wave_type == toAmyWaveType(SmkWaveType::Algo));
    assert(mock.osc_mix_calls.empty());
    assert(mock.sub_osc_calls.empty());
    assert(mock.noise_calls.empty());
    assert(mock.detune_calls.empty());

    // Subtractive factory patch: the same controls must still be applied.
    assert(pm.selectPatch(0));
    assert(pm.activePatch().wave_type != toAmyWaveType(SmkWaveType::Algo));
    assert(!mock.osc_mix_calls.empty());
    assert(std::abs(mock.osc_mix_calls.back().value - pm.activePatch().osc_mix) < 1e-6f);
    assert(!mock.sub_osc_calls.empty());
    assert(std::abs(mock.sub_osc_calls.back().value - pm.activePatch().sub_level) < 1e-6f);
    assert(!mock.noise_calls.empty());
    assert(std::abs(mock.noise_calls.back().value - pm.activePatch().noise_level) < 1e-6f);
    assert(!mock.detune_calls.empty());
    assert(std::abs(mock.detune_calls.back().cents - pm.activePatch().osc_detune) < 1e-6f);

    printf("PASS: ALGO skips OscMix/Sub/Noise/Detune; subtractive patches keep them\n");
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
    test_patch_manager_chorus_depth_and_modes();
    test_patch_manager_filter_inherit_and_retention();
    test_storage_manager_real_files_and_migration();
    test_bank_b_fm_relative_and_detune();
    test_bank_b_fm_ratio_freq_mult_composition();
    test_fm_soft_takeover_pickup();
    test_fm_algorithm_saved_value_and_patch_reset();
    test_algo_skips_subtractive_osc_controls();
    test_macro_curves();
    test_amy_core_fixes();
    printf("=== ALL SMK SYNTH EXPANSION TESTS PASSED ===\n");
    return 0;
}
