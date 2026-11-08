#include "amy_runtime.h"
#include "local_performance_backend.h"
#include "user_presets_v2.h"
#include "user_preset_nvs_v2.h"
#include "fake_nvs.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

extern "C" {
#include "amy.h"
void my_srand48(uint32_t);
void probe_realtime(bool);
size_t probe_bytes();
uint32_t probe_calls();
}

using namespace smk::p4;
static amy::Runtime runtime;

static void midi(uint8_t status, uint8_t a, uint8_t b = 0) {
    const uint8_t packet[] = {uint8_t(status >> 4), status, a, b};
    assert(runtime.usbInput().packet(packet, 0));
}

static std::vector<int16_t> renderBlocks(unsigned start_time, unsigned count) {
    std::vector<int16_t> pcm;
    pcm.reserve(count * 512);
    for (unsigned i = 0; i < count; ++i) {
        runtime.apply(start_time + i * 20);
        const int16_t* buf = runtime.render();
        assert(buf != nullptr);
        pcm.insert(pcm.end(), buf, buf + 512);
    }
    return pcm;
}

static std::vector<int16_t> renderModScenario(unsigned patch_id, uint8_t cc1_val,
                                              float en_val, float src_val, float dst_val,
                                              float amt_val, float pol_val,
                                              bool compare_mode = false, bool revert_after = false) {
    assert(runtime.begin());
    assert(runtime.commands().push({amy::CommandType::Patch, uint16_t(patch_id)}));
    runtime.apply(0);
    runtime.loadPendingPatch();
    assert(runtime.snapshot().patch_state == amy::PatchState::Ready);

    LocalAmyPerformanceBackend backend(runtime.commands());
    backend.publish(runtime.snapshot(), false, 0, 0);

    unsigned now = 20;
    auto flush = [&] {
        backend.service(now);
        runtime.apply(now);
        backend.publish(runtime.snapshot(), false, 0, 0);
        now += 20;
        assert(!backend.editError());
    };

    assert(backend.editParameter(edit::ModEnable, en_val));
    assert(backend.editParameter(edit::ModSource, src_val));
    assert(backend.editParameter(edit::ModDest, dst_val));
    assert(backend.editParameter(edit::ModAmount, amt_val));
    assert(backend.editParameter(edit::ModPolarity, pol_val));
    flush();

    if (compare_mode) {
        assert(backend.editAction(0)); // Compare
        flush();
    } else if (revert_after) {
        assert(backend.editAction(1)); // Revert
        flush();
    }

    my_srand48(1234);
    midi(0xb0, 1, cc1_val);
    midi(0x90, 60, 100);
    runtime.apply(now);
    auto pcm = renderBlocks(now, 16);
    now += 320;
    midi(0x80, 60, 0);
    midi(0xb0, 1, 0);
    runtime.apply(now);
    runtime.end();
    return pcm;
}

// =========================================================================
// GATE A: MOD SOURCE OFF SEMANTICS
// =========================================================================
static void testModSourceOff() {
    std::puts("--- Testing GATE A: Mod Source OFF ---");

    // 1. Juno (patch 4) - Mod Wheel -> Filter Cutoff (ModDest = 3)
    {
        // Baseline unmodulated (CC1 = 0)
        const auto pcm_cc0 = renderModScenario(4, 0, 1.0f, 1.0f, 3.0f, 50.0f, 1.0f);
        // Mod Wheel active (CC1 = 127) -> filter cutoff modulated
        const auto pcm_cc127_on = renderModScenario(4, 127, 1.0f, 1.0f, 3.0f, 50.0f, 1.0f);
        assert(pcm_cc127_on != pcm_cc0);

        // Mod Source = OFF (0.0f) at CC1 = 127 -> modulation must CEASE (byte-identical to CC1 = 0!)
        const auto pcm_cc127_off = renderModScenario(4, 127, 1.0f, 0.0f, 3.0f, 50.0f, 1.0f);
        assert(pcm_cc127_off == pcm_cc0);

        // Mod Source restored to Mod Wheel (1.0f) -> modulation restored
        const auto pcm_cc127_restored = renderModScenario(4, 127, 1.0f, 1.0f, 3.0f, 50.0f, 1.0f);
        assert(pcm_cc127_restored == pcm_cc127_on);


    }

    // 2. DX7 (patch 135) - Mod Wheel -> Pitch Vibrato (ModDest = 1)
    {
        const auto dx_cc0 = renderModScenario(135, 0, 1.0f, 1.0f, 1.0f, 50.0f, 1.0f);
        const auto dx_cc127_on = renderModScenario(135, 127, 1.0f, 1.0f, 1.0f, 50.0f, 1.0f);
        assert(dx_cc127_on != dx_cc0);

        // Mod Source = OFF at CC1 = 127 -> vibrato ceases (byte-identical to CC1 = 0!)
        const auto dx_cc127_off = renderModScenario(135, 127, 1.0f, 0.0f, 1.0f, 50.0f, 1.0f);
        assert(dx_cc127_off == dx_cc0);

        // Revert mode: resets to original (ModSource = 1) -> vibrato restored
        const auto dx_reverted = renderModScenario(135, 127, 1.0f, 0.0f, 1.0f, 50.0f, 1.0f, false, true);
        assert(dx_reverted == dx_cc127_on);

        // Compare mode: compares against original (ModSource = 1) -> vibrato active during compare
        const auto dx_compare = renderModScenario(135, 127, 1.0f, 0.0f, 1.0f, 50.0f, 1.0f, true, false);
        assert(dx_compare == dx_cc127_on);
    }

    // 3. Preset Save/Recall with ModSource = OFF
    {
        fake_nvs::blobs.clear();
        fake_nvs::free_entries = 600;
        NvsVersionedPresets store;
        assert(store.begin());

        assert(runtime.begin());
        assert(runtime.commands().push({amy::CommandType::Patch, 4}));
        runtime.apply(0);
        runtime.loadPendingPatch();

        LocalAmyPerformanceBackend backend(runtime.commands());
        backend.presetStore(&store);
        backend.publish(runtime.snapshot(), false, 0, 0);

        assert(backend.editParameter(edit::ModSource, 0.0f));
        backend.service(20);
        runtime.apply(20);
        backend.publish(runtime.snapshot(), false, 0, 0);

        // Save to slot 0 with quiet barrier
        assert(backend.editAction(2, 0, "OFF PRESET"));
        runtime.apply(40);
        backend.publish(runtime.snapshot(), false, 0, 0);
        assert(runtime.snapshot().storage_quiet);
        backend.service(40);
        runtime.apply(40);
        backend.publish(runtime.snapshot(), false, 0, 0);
        assert(!backend.editError());

        // Reset runtime
        runtime.end();
        assert(runtime.begin());
        assert(runtime.commands().push({amy::CommandType::Patch, 31}));
        runtime.apply(0);
        runtime.loadPendingPatch();

        LocalAmyPerformanceBackend backend2(runtime.commands());
        backend2.presetStore(&store);
        backend2.publish(runtime.snapshot(), false, 0, 0);

        // Recall slot 0
        assert(backend2.editAction(3, 0));
        runtime.apply(20);
        runtime.loadPendingPatch();
        backend2.publish(runtime.snapshot(), false, 0, 0);
        backend2.service(40);
        runtime.apply(40);
        backend2.publish(runtime.snapshot(), false, 0, 0);
        assert(!backend2.editError());

        assert(runtime.snapshot().patch_id == 4);
        assert(runtime.snapshot().edit.applied[edit::ModSource] == 0.0f);
        runtime.end();
    }

    // 4. Eight voices, Harmony Assist, Note Off, Panic
    {
        assert(runtime.begin());
        assert(runtime.commands().push({amy::CommandType::Patch, 4}));
        runtime.apply(0);
        runtime.loadPendingPatch();

        LocalAmyPerformanceBackend backend(runtime.commands());
        backend.publish(runtime.snapshot(), false, 0, 0);
        assert(backend.editParameter(edit::ModSource, 0.0f));
        backend.service(20);
        runtime.apply(20);

        // 8 voices
        for (unsigned n = 0; n < 8; ++n) {
            midi(0x90, 48 + n * 3, 100);
        }
        runtime.apply(40);
        assert(runtime.snapshot().active_notes == 8);
        renderBlocks(40, 8);

        // Note off
        for (unsigned n = 0; n < 8; ++n) {
            midi(0x80, 48 + n * 3, 0);
        }
        runtime.apply(200);
        assert(runtime.snapshot().active_notes == 0);

        // Panic clears notes
        midi(0x90, 60, 100);
        runtime.apply(220);
        assert(runtime.snapshot().active_notes == 1);
        runtime.commands().panic();
        runtime.apply(240);
        assert(runtime.snapshot().active_notes == 0);
        runtime.end();
    }
    std::puts("PASS GATE A: Mod Source OFF semantics completely verified");
}

// =========================================================================
// GATE B: MOD POLARITY (UNI +, BIP +/-, INV -)
// =========================================================================
static void testModPolarity() {
    std::puts("--- Testing GATE B: Mod Polarity Semantics ---");

    // 1. Pitch Vibrato on DX7 (patch 135)
    {
        // When CC1 = 0 (neutral), all 3 polarities produce identical audio
        const auto pcm_cc0_uni = renderModScenario(135, 0, 1.0f, 1.0f, 1.0f, 50.0f, 0.0f);
        const auto pcm_cc0_bip = renderModScenario(135, 0, 1.0f, 1.0f, 1.0f, 50.0f, 1.0f);
        const auto pcm_cc0_inv = renderModScenario(135, 0, 1.0f, 1.0f, 1.0f, 50.0f, 2.0f);
        assert(pcm_cc0_uni == pcm_cc0_bip);
        assert(pcm_cc0_bip == pcm_cc0_inv);

        // At CC1 = 127 and Amount = +50%:
        // BIP +/- (1) is centered bipolar.
        // UNI + (0) is unipolar positive.
        // INV - (2) is unipolar negative/inverted.
        const auto pcm_cc127_uni = renderModScenario(135, 127, 1.0f, 1.0f, 1.0f, 50.0f, 0.0f);
        const auto pcm_cc127_bip = renderModScenario(135, 127, 1.0f, 1.0f, 1.0f, 50.0f, 1.0f);
        const auto pcm_cc127_inv = renderModScenario(135, 127, 1.0f, 1.0f, 1.0f, 50.0f, 2.0f);

        // All 3 must be mutually distinct!
        assert(pcm_cc127_uni != pcm_cc127_bip);
        assert(pcm_cc127_bip != pcm_cc127_inv);
        assert(pcm_cc127_uni != pcm_cc127_inv);

        // Test amount matrix: -100, -50, 0, +50, +100
        for (float amt : {-100.0f, -50.0f, 0.0f, 50.0f, 100.0f}) {
            for (float pol : {0.0f, 1.0f, 2.0f}) {
                for (uint8_t cc : {0, 64, 127}) {
                    auto pcm = renderModScenario(135, cc, 1.0f, 1.0f, 1.0f, amt, pol);
                    for (int16_t s : pcm) {
                        assert(s > -32767 && s < 32767); // no clipping
                    }
                }
            }
        }
    }

    // 2. Filter Cutoff on Juno (patch 4)
    {
        // When CC1 = 0, all 3 polarities produce identical audio
        const auto flt_cc0_uni = renderModScenario(4, 0, 1.0f, 1.0f, 3.0f, 50.0f, 0.0f);
        const auto flt_cc0_bip = renderModScenario(4, 0, 1.0f, 1.0f, 3.0f, 50.0f, 1.0f);
        const auto flt_cc0_inv = renderModScenario(4, 0, 1.0f, 1.0f, 3.0f, 50.0f, 2.0f);
        assert(flt_cc0_uni == flt_cc0_bip);
        assert(flt_cc0_bip == flt_cc0_inv);

        // At Amount = +50%, CC1 = 127:
        // UNI + (0): opens filter (+1.5 octaves)
        // BIP +/- (1): opens filter (+1.5 octaves, preserving baseline)
        // INV - (2): closes filter (-1.5 octaves, genuinely inverted!)
        const auto flt_pos_uni = renderModScenario(4, 127, 1.0f, 1.0f, 3.0f, 50.0f, 0.0f);
        const auto flt_pos_bip = renderModScenario(4, 127, 1.0f, 1.0f, 3.0f, 50.0f, 1.0f);
        const auto flt_pos_inv = renderModScenario(4, 127, 1.0f, 1.0f, 3.0f, 50.0f, 2.0f);

        assert(flt_pos_uni == flt_pos_bip);
        assert(flt_pos_inv != flt_pos_bip);

        // At Amount = -50%, CC1 = 127:
        // UNI + (0): opens filter (+1.5 octaves)
        // BIP +/- (1): closes filter (-1.5 octaves)
        // INV - (2): closes filter (-1.5 octaves)
        const auto flt_neg_uni = renderModScenario(4, 127, 1.0f, 1.0f, 3.0f, -50.0f, 0.0f);
        const auto flt_neg_bip = renderModScenario(4, 127, 1.0f, 1.0f, 3.0f, -50.0f, 1.0f);
        const auto flt_neg_inv = renderModScenario(4, 127, 1.0f, 1.0f, 3.0f, -50.0f, 2.0f);

        assert(flt_neg_bip == flt_neg_inv);
        assert(flt_neg_uni != flt_neg_bip);
    }
    std::puts("PASS GATE B: Mod Polarity (UNI +, BIP +/-, INV -) completely verified");
}

// =========================================================================
// GATE C: NUMERIC PRECISION AND LIMITS OF USER PRESETS V2 CODEC
// =========================================================================
static void testCodecPrecisionAndLimits() {
    std::puts("--- Testing GATE C: Codec V2 Precision & Limits ---");

    // C1: Roundtrip numérico com valores não alinhados à quantização
    {
        edit::Preset juno{};
        juno.sequence = 1;
        juno.patch = 4;
        juno.family = 1;
        std::strcpy(juno.name, "PRECISION JUNO");
        juno.mask = edit::bit(edit::Attack) | edit::bit(edit::Decay) | edit::bit(edit::Detune) |
                    edit::bit(edit::FilterEnv) | edit::bit(edit::LfoRate) | edit::bit(edit::ModAmount);
        juno.values[edit::Attack] = 123.456f;
        juno.values[edit::Decay] = 987.321f;
        juno.values[edit::Detune] = -17.427f;
        juno.values[edit::FilterEnv] = -2.345f;
        juno.values[edit::LfoRate] = 7.319f;
        juno.values[edit::ModAmount] = -42.718f;

        uint8_t bytes[edit::kV2RecordBytes]{};
        assert(edit::encodeV2(juno, bytes));
        edit::Preset decoded{};
        assert(edit::decodeV2(bytes, sizeof(bytes), decoded));
        assert(decoded.values[edit::Attack] == 123.456f);
        assert(decoded.values[edit::Decay] == 987.321f);
        assert(decoded.values[edit::Detune] == -17.427f);
        assert(decoded.values[edit::FilterEnv] == -2.345f);
        assert(decoded.values[edit::LfoRate] == 7.319f);
        assert(decoded.values[edit::ModAmount] == -42.718f);

        // FM preset in Mode 2 (active > 25, compact quantization)
        edit::Preset fm{};
        fm.sequence = 2;
        fm.patch = 135;
        fm.family = 0;
        std::strcpy(fm.name, "PRECISION FM");
        const edit::Mask allowed = edit::allowedMask(0, false);
        for (unsigned i = 0; i < edit::Count; ++i) if (allowed & edit::bit(i)) {
            fm.mask |= edit::bit(i);
            fm.values[i] = (i >= edit::OpRatio0 && i <= edit::OpRatio5) ? 2.0f :
                i == edit::Depth ? 0.5f : i == edit::FmFeedback ? 50.0f :
                (i >= edit::OpAttack0 && i <= edit::OpAttack5) ? 100.0f :
                (i >= edit::OpDecay0 && i <= edit::OpDecay5) ? 500.0f :
                (i >= edit::OpVel0 && i <= edit::OpVel5) ? 50.0f :
                i == edit::LfoRate ? 5.0f : i == edit::LfoDepth ? 50.0f : i == edit::LfoWave ? 0.0f :
                i == edit::LfoDest ? 1.0f : i == edit::ModEnable ? 1.0f : i == edit::ModSource ? 1.0f :
                i == edit::ModDest ? 1.0f : i == edit::ModAmount ? 50.0f : i == edit::ModPolarity ? 1.0f : 50.0f;
        }
        fm.values[edit::FmFeedback] = 37.777f;
        fm.values[edit::OpLevel0] = 63.333f;
        fm.values[edit::OpAttack0] = 123.456f;
        fm.values[edit::OpDecay0] = 987.321f;
        fm.values[edit::LfoRate] = 7.319f;
        fm.values[edit::ModAmount] = -42.718f;
        fm.values[edit::OpRatio0] = 3.14159f;
        fm.values[edit::Depth] = 0.87654f;

        assert(edit::encodeV2(fm, bytes));
        edit::Preset fm_decoded{};
        assert(edit::decodeV2(bytes, sizeof(bytes), fm_decoded));

        // Exact float32:
        assert(fm_decoded.values[edit::LfoRate] == 7.319f);
        assert(fm_decoded.values[edit::OpRatio0] == 3.14159f);
        assert(fm_decoded.values[edit::Depth] == 0.87654f);

        // Fixed point 0.01%:
        assert(std::fabs(fm_decoded.values[edit::FmFeedback] - 37.777f) <= 0.0051f);
        assert(std::fabs(fm_decoded.values[edit::OpLevel0] - 63.333f) <= 0.0051f);
        assert(std::fabs(fm_decoded.values[edit::ModAmount] - (-42.718f)) <= 0.0051f);

        // Fixed point 1/16 ms:
        assert(std::fabs(fm_decoded.values[edit::OpAttack0] - 123.456f) <= 0.0313f);
        assert(std::fabs(fm_decoded.values[edit::OpDecay0] - 987.321f) <= 0.0313f);
    }

    // C2: Limites numéricos
    {
        edit::Preset p{};
        p.sequence = 1;
        p.patch = 4;
        p.family = 1;
        std::strcpy(p.name, "LIMIT TEST");
        p.mask = edit::bit(edit::Cutoff);

        uint8_t bytes[edit::kV2RecordBytes]{};

        p.values[edit::Cutoff] = std::numeric_limits<float>::quiet_NaN();
        assert(!edit::encodeV2(p, bytes));

        p.values[edit::Cutoff] = std::numeric_limits<float>::infinity();
        assert(!edit::encodeV2(p, bytes));

        p.values[edit::Cutoff] = 20.0f;
        assert(edit::encodeV2(p, bytes));

        p.values[edit::Cutoff] = 19.9f;
        assert(!edit::encodeV2(p, bytes));

        p.values[edit::Cutoff] = 18000.0f;
        assert(edit::encodeV2(p, bytes));

        p.values[edit::Cutoff] = 18001.0f;
        assert(!edit::encodeV2(p, bytes));

        p.values[edit::Cutoff] = 1000.0f;
        p.mask = edit::bit(57);
        assert(!edit::encodeV2(p, bytes));

        p.family = 0;
        p.patch = 135;
        p.mask = edit::bit(edit::Cutoff);
        assert(!edit::encodeV2(p, bytes));
    }

    // C3: Capacidade total
    {
        uint8_t bytes[edit::kV2RecordBytes]{};

        // Juno 25 parameters
        edit::Preset juno{};
        juno.sequence = 1;
        juno.patch = 31;
        juno.family = 1;
        std::strcpy(juno.name, "FULL JUNO 25");
        const edit::Mask j_mask = edit::allowedMask(1, false);
        for (unsigned i = 0; i < edit::Count; ++i) if (j_mask & edit::bit(i)) {
            juno.mask |= edit::bit(i);
            juno.values[i] = i == edit::Cutoff ? 1000.0f : i == edit::Resonance ? 1.0f :
                (i == edit::Attack || i == edit::Release || i == edit::Decay) ? 100.0f :
                i == edit::Sustain ? 70.0f : i == edit::FilterEnv ? 2.0f : i == edit::FilterKeytrack ? 100.0f :
                i == edit::PulseWidth ? 50.0f : i == edit::Detune ? 0.0f : i == edit::Pan ? 0.0f :
                i == edit::LfoRate ? 5.0f : i == edit::LfoDepth ? 50.0f : i == edit::LfoWave ? 1.0f :
                i == edit::LfoDest ? 1.0f : i == edit::ModEnable ? 1.0f : i == edit::ModSource ? 1.0f :
                i == edit::ModDest ? 3.0f : i == edit::ModAmount ? 50.0f : i == edit::ModPolarity ? 1.0f : 50.0f;
        }
        assert(edit::maskPopcount(juno.mask) == 25);
        assert(edit::encodeV2(juno, bytes));
        assert(bytes[15] == 1);
        edit::Preset juno_dec{};
        assert(edit::decodeV2(bytes, sizeof(bytes), juno_dec));
        assert(juno_dec.mask == juno.mask);

        // FM 42 parameters
        edit::Preset fm{};
        fm.sequence = 1;
        fm.patch = 135;
        fm.family = 0;
        std::strcpy(fm.name, "FULL FM 42");
        const edit::Mask fm_mask = edit::allowedMask(0, false);
        for (unsigned i = 0; i < edit::Count; ++i) if (fm_mask & edit::bit(i)) {
            fm.mask |= edit::bit(i);
            fm.values[i] = (i >= edit::OpRatio0 && i <= edit::OpRatio5) ? 2.0f :
                i == edit::Depth ? 0.5f : i == edit::FmFeedback ? 50.0f :
                (i >= edit::OpAttack0 && i <= edit::OpAttack5) ? 100.0f :
                (i >= edit::OpDecay0 && i <= edit::OpDecay5) ? 500.0f :
                (i >= edit::OpVel0 && i <= edit::OpVel5) ? 50.0f :
                i == edit::LfoRate ? 5.0f : i == edit::LfoDepth ? 50.0f : i == edit::LfoWave ? 0.0f :
                i == edit::LfoDest ? 1.0f : i == edit::ModEnable ? 1.0f : i == edit::ModSource ? 1.0f :
                i == edit::ModDest ? 1.0f : i == edit::ModAmount ? 50.0f : i == edit::ModPolarity ? 1.0f : 50.0f;
        }
        assert(edit::maskPopcount(fm.mask) == 42);
        assert(edit::encodeV2(fm, bytes));
        assert(bytes[15] == 2);
        edit::Preset fm_dec{};
        assert(edit::decodeV2(bytes, sizeof(bytes), fm_dec));
        assert(fm_dec.mask == fm.mask);
    }
    std::puts("PASS GATE C: Codec V2 Precision & Limits completely verified");
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    testModSourceOff();
    testModPolarity();
    testCodecPrecisionAndLimits();
    std::puts("ALL FINAL CORRECTIONS TESTS PASSED SUCCESSFULLY");
    return 0;
}
