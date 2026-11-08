#include "local_performance_backend.h"
#include "user_preset_nvs.h"
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
uint32_t probe_calls();
size_t probe_bytes();
}
using namespace smk::p4;
static amy::Runtime runtime;

static void midi(uint8_t status, uint8_t a, uint8_t b = 0) {
    const uint8_t packet[] = {uint8_t(status >> 4), status, a, b};
    assert(runtime.usbInput().packet(packet, 0));
}

static void testCodecExtendedCapacity() {
    // 1. All 25 Juno + LFO + MOD parameters active simultaneously (mode 1: 4-byte float per active field in 160B record).
    {
        edit::Preset p{};
        p.sequence = 7;
        p.patch = 31;
        p.family = 1;
        std::strcpy(p.name, "JUNO FULL 25");
        const edit::Mask allowed = edit::allowedMask(1, false);
        unsigned count = 0;
        for (unsigned i = 0; i < edit::Count; ++i) if (allowed & edit::bit(i)) {
            p.mask |= edit::bit(i);
            ++count;
            p.values[i] = i == edit::Cutoff ? 650.0f : i == edit::Resonance ? 1.2f :
                (i == edit::Attack || i == edit::Release || i == edit::Decay) ? 120.0f :
                i == edit::Sustain ? 65.0f : i == edit::FilterEnv ? 2.5f : i == edit::FilterKeytrack ? 125.0f :
                i == edit::PulseWidth ? 40.0f : i == edit::Detune ? -15.0f : i == edit::Pan ? 16.0f :
                i == edit::LfoRate ? 6.25f : i == edit::LfoDepth ? 55.0f : i == edit::LfoWave ? 2.0f :
                i == edit::LfoDest ? 2.0f : i == edit::ModEnable ? 1.0f : i == edit::ModSource ? 1.0f :
                i == edit::ModDest ? 3.0f : i == edit::ModAmount ? -60.0f : i == edit::ModPolarity ? 0.0f : 50.0f;
        }
        assert(count == 25);
        uint8_t bytes[edit::kV2RecordBytes]{};
        assert(edit::encodeV2(p, bytes));
        edit::Preset decoded{};
        assert(edit::decodeV2(bytes, sizeof(bytes), decoded));
        assert(decoded.mask == p.mask && decoded.sequence == p.sequence && decoded.patch == p.patch);
        for (unsigned i = 0; i < edit::Count; ++i) if (p.mask & edit::bit(i)) {
            assert(decoded.values[i] == p.values[i]);
        }
    }

    // 2. All 42 FM + LFO + MOD parameters active simultaneously (mode 2: compact 160B record).
    {
        edit::Preset p{};
        p.sequence = 12;
        p.patch = 135;
        p.family = 0;
        std::strcpy(p.name, "DX7 FULL 42");
        const edit::Mask allowed = edit::allowedMask(0, false);
        unsigned count = 0;
        for (unsigned i = 0; i < edit::Count; ++i) if (allowed & edit::bit(i)) {
            p.mask |= edit::bit(i);
            ++count;
            p.values[i] = (i >= edit::OpRatio0 && i <= edit::OpRatio5) ? 2.5f :
                i == edit::Depth ? 0.75f : i == edit::FmFeedback ? 65.0f :
                (i >= edit::OpAttack0 && i <= edit::OpAttack5) ? 125.5f :
                (i >= edit::OpDecay0 && i <= edit::OpDecay5) ? 850.25f :
                (i >= edit::OpVel0 && i <= edit::OpVel5) ? 70.0f :
                i == edit::LfoRate ? 4.5f : i == edit::LfoDepth ? 45.0f : i == edit::LfoWave ? 0.0f :
                i == edit::LfoDest ? 1.0f : i == edit::ModEnable ? 1.0f : i == edit::ModSource ? 1.0f :
                i == edit::ModDest ? 1.0f : i == edit::ModAmount ? 80.0f : i == edit::ModPolarity ? 1.0f : 90.0f;
        }
        assert(count == 42);
        uint8_t bytes[edit::kV2RecordBytes]{};
        assert(edit::encodeV2(p, bytes));
        edit::Preset decoded{};
        assert(edit::decodeV2(bytes, sizeof(bytes), decoded));
        assert(decoded.mask == p.mask && decoded.sequence == p.sequence && decoded.patch == p.patch);
        for (unsigned i = 0; i < edit::Count; ++i) if (p.mask & edit::bit(i)) {
            assert(std::fabs(decoded.values[i] - p.values[i]) < 1e-4f);
        }
    }
    std::puts("PASS Gate D0/F extended codec: 25 Juno+LFO+MOD and 42 FM+LFO+MOD parameters roundtrip in 160B V2 record");
}

static std::vector<int16_t> renderBlocks(unsigned now, unsigned blocks = 48) {
    std::vector<int16_t> pcm;
    pcm.reserve(blocks * 512);
    const auto allocs = probe_calls();
    const auto bytes = probe_bytes();
    for (unsigned b = 0; b < blocks; ++b) {
        probe_realtime(true);
        runtime.apply(now + b * 5);
        const int16_t* out = runtime.render();
        probe_realtime(false);
        assert(out);
        pcm.insert(pcm.end(), out, out + 512);
    }
    assert(probe_calls() == allocs && probe_bytes() == bytes);
    assert(runtime.snapshot().clipped == 0);
    assert(runtime.snapshot().oom_count == 0);
    return pcm;
}

enum class LfoMode { Original, Edited, Compared, Reverted, Recalled };

static std::vector<int16_t> renderLfoScenario(
    unsigned patch_id,
    LfoMode mode,
    float dest_val,
    float depth_val,
    float rate_val,
    float wave_val
) {
    assert(runtime.begin());
    assert(runtime.commands().push({amy::CommandType::Patch, uint16_t(patch_id)}));
    runtime.apply(0);
    runtime.loadPendingPatch();
    assert(runtime.snapshot().patch_state == amy::PatchState::Ready);

    NvsUserPresets store;
    assert(store.begin());
    LocalAmyPerformanceBackend backend(runtime.commands());
    backend.presetStore(&store);
    unsigned now = 20;
    auto publish = [&] { backend.publish(runtime.snapshot(), false, 0, 0); };
    auto flush = [&] {
        backend.service(now);
        runtime.apply(now);
        publish();
        now += 20;
        assert(!backend.editError());
    };
    publish();

    const auto avail = runtime.snapshot().edit.available;
    assert((avail & (edit::bit(edit::LfoRate) | edit::bit(edit::LfoDepth) | edit::bit(edit::LfoWave) | edit::bit(edit::LfoDest))) ==
           (edit::bit(edit::LfoRate) | edit::bit(edit::LfoDepth) | edit::bit(edit::LfoWave) | edit::bit(edit::LfoDest)));

    if (mode == LfoMode::Recalled) {
        assert(backend.editAction(3, 2));
        runtime.apply(now);
        runtime.loadPendingPatch();
        publish();
        flush();
        assert(std::fabs(runtime.snapshot().edit.applied[edit::LfoDepth] - depth_val) < 0.01f);
        assert(std::fabs(runtime.snapshot().edit.applied[edit::LfoRate] - rate_val) < 0.01f);
        assert(std::fabs(runtime.snapshot().edit.applied[edit::LfoWave] - wave_val) < 0.01f);
    } else if (mode != LfoMode::Original) {
        assert(backend.editParameter(edit::LfoDest, dest_val));
        assert(backend.editParameter(edit::LfoDepth, depth_val));
        assert(backend.editParameter(edit::LfoRate, rate_val));
        assert(backend.editParameter(edit::LfoWave, wave_val));
        flush();
        if (mode == LfoMode::Compared) {
            assert(backend.editAction(0));
            flush();
        } else if (mode == LfoMode::Reverted) {
            assert(backend.editAction(1));
            flush();
        } else if (mode == LfoMode::Edited) {
            assert(backend.editAction(2, 2, "GATE E LFO"));
            runtime.apply(now);
            publish();
            assert(runtime.snapshot().storage_quiet);
            backend.service(now);
            runtime.apply(now);
            publish();
            now += 20;
            assert(!backend.editError());
        }
    }

    auto c = harmony::preset(1);
    c.chord = 13;
    assert(runtime.commands().push({amy::CommandType::Harmony, 0, 0, 0, harmony::pack(c)}));
    runtime.apply(now);
    my_srand48(321);
    midi(0x90, 60, 64);
    midi(0x90, 62, 64);
    runtime.apply(now);
    assert(runtime.snapshot().active_notes == 8);
    const auto pcm = renderBlocks(now, 48);
    now += 300;
    midi(0x80, 60, 0);
    midi(0x80, 62, 0);
    runtime.apply(now);
    assert(runtime.snapshot().active_notes == 0);
    runtime.end();
    return pcm;
}

static void testLfoControls(unsigned patch_id) {
    fake_nvs::blobs.clear();
    fake_nvs::free_entries = 700;
    const auto pcm_orig = renderLfoScenario(patch_id, LfoMode::Original, 1.0f, 80.0f, 8.0f, 0.0f);
    const auto pcm_pitch_sine = renderLfoScenario(patch_id, LfoMode::Edited, 1.0f, 80.0f, 8.0f, 0.0f);
    assert(pcm_pitch_sine != pcm_orig);
    assert(renderLfoScenario(patch_id, LfoMode::Compared, 1.0f, 80.0f, 8.0f, 0.0f) == pcm_orig);
    assert(renderLfoScenario(patch_id, LfoMode::Reverted, 1.0f, 80.0f, 8.0f, 0.0f) == pcm_orig);
    assert(renderLfoScenario(patch_id, LfoMode::Recalled, 1.0f, 80.0f, 8.0f, 0.0f) == pcm_pitch_sine);

    // Wave PULSE (4) and Rate 14 Hz -> must differ from SINE (0) at 8 Hz and from Original
    const auto pcm_pitch_pulse = renderLfoScenario(patch_id, LfoMode::Edited, 1.0f, 80.0f, 14.0f, 4.0f);
    assert(pcm_pitch_pulse != pcm_pitch_sine && pcm_pitch_pulse != pcm_orig);

    // LFO -> Amp (dest=3) -> must differ from Original and Pitch
    const auto pcm_amp = renderLfoScenario(patch_id, LfoMode::Edited, 3.0f, 80.0f, 8.0f, 0.0f);
    assert(pcm_amp != pcm_orig && pcm_amp != pcm_pitch_sine);

    // LFO -> Filter Cutoff (dest=2): valid on Juno (< 128), rejected on DX7 (>= 128)
    if (patch_id < 128) {
        const auto pcm_flt = renderLfoScenario(patch_id, LfoMode::Edited, 2.0f, 80.0f, 8.0f, 0.0f);
        assert(pcm_flt != pcm_orig && pcm_flt != pcm_amp);
    } else {
        assert(runtime.begin());
        assert(runtime.commands().push({amy::CommandType::Patch, uint16_t(patch_id)}));
        runtime.apply(0);
        runtime.loadPendingPatch();
        const uint32_t errs = runtime.snapshot().edit.errors;
        edit::Request bad{};
        bad.patch = uint16_t(patch_id);
        bad.generation = runtime.snapshot().generation;
        bad.request = runtime.snapshot().edit.request + 1;
        bad.mask = edit::bit(edit::LfoDest);
        bad.values[edit::LfoDest] = 2.0f; // Filter Cutoff on FM -> must reject
        assert(runtime.commands().edits.push(bad));
        runtime.apply(20);
        assert(runtime.snapshot().edit.errors == errs + 1);
        runtime.end();
    }
    std::printf("PASS Gate E LFO on patch=%u (rate, depth, wave, dest, 8 voices, Harmony, Compare/Revert/Save/Recall)\n", patch_id);
}

static std::vector<int16_t> renderCc1PitchScenario(
    unsigned patch_id,
    uint8_t cc1_val,
    float enable_val,
    float amount_val,
    float polarity_val,
    bool revert_after = false
) {
    assert(runtime.begin());
    assert(runtime.commands().push({amy::CommandType::Patch, uint16_t(patch_id)}));
    runtime.apply(0);
    runtime.loadPendingPatch();
    LocalAmyPerformanceBackend backend(runtime.commands());
    unsigned now = 20;
    auto publish = [&] { backend.publish(runtime.snapshot(), false, 0, 0); };
    auto flush = [&] {
        backend.service(now);
        runtime.apply(now);
        publish();
        now += 20;
        assert(!backend.editError());
    };
    publish();

    assert(backend.editParameter(edit::ModEnable, enable_val));
    assert(backend.editParameter(edit::ModAmount, amount_val));
    assert(backend.editParameter(edit::ModPolarity, polarity_val));
    flush();
    if (revert_after) {
        assert(backend.editAction(1));
        flush();
    }

    my_srand48(555);
    midi(0xb0, 1, cc1_val);
    midi(0x90, 60, 80);
    runtime.apply(now);
    auto pcm = renderBlocks(now, 32);
    now += 200;
    midi(0x80, 60, 0);
    midi(0xb0, 1, 0);
    runtime.apply(now);
    runtime.end();
    return pcm;
}

static void testModMatrixAndCc1Composition() {
    // 1. Mod Wheel -> Pitch on Juno (patch 4) and DX7 (patch 135) across CC1 = 0, 64, 127, Amount, Polarity, Enable.
    for (unsigned patch_id : {4u, 135u}) {
        const auto cc0 = renderCc1PitchScenario(patch_id, 0, 1.0f, 50.0f, 1.0f);
        const auto cc64 = renderCc1PitchScenario(patch_id, 64, 1.0f, 50.0f, 1.0f);
        const auto cc127 = renderCc1PitchScenario(patch_id, 127, 1.0f, 50.0f, 1.0f);
        assert(cc0 != cc64 && cc64 != cc127 && cc0 != cc127);

        // Increase ModAmount to 100% -> must differ from default 50% at CC1=127
        const auto cc127_amt100 = renderCc1PitchScenario(patch_id, 127, 1.0f, 100.0f, 1.0f);
        assert(cc127_amt100 != cc127);

        // Switch ModPolarity to Unipolar (0) -> must differ from Bipolar (1) at CC1=127
        const auto cc127_uni = renderCc1PitchScenario(patch_id, 127, 1.0f, 100.0f, 0.0f);
        assert(cc127_uni != cc127_amt100);

        // Disable Slot 1 (ModEnable = 0) -> CC1=127 must produce identical PCM to CC1=0!
        const auto cc127_disabled = renderCc1PitchScenario(patch_id, 127, 0.0f, 100.0f, 0.0f);
        assert(cc127_disabled == cc0);

        // Revert -> restores default Mod Wheel -> Pitch behavior
        assert(renderCc1PitchScenario(patch_id, 127, 0.0f, 100.0f, 0.0f, true) == cc127);
    }

    // 2. Mod Wheel -> Filter Cutoff (ModDest = 3) on Juno (patch 0 and patch 4), composition with Cutoff edit.
    {
        fake_nvs::blobs.clear();
        fake_nvs::free_entries = 700;
        assert(runtime.begin());
        assert(runtime.commands().push({amy::CommandType::Patch, 0}));
        runtime.apply(0);
        runtime.loadPendingPatch();
        NvsUserPresets store;
        assert(store.begin());
        LocalAmyPerformanceBackend backend(runtime.commands());
        backend.presetStore(&store);
        unsigned now = 20;
        auto publish = [&] { backend.publish(runtime.snapshot(), false, 0, 0); };
        auto flush = [&] {
            backend.service(now);
            runtime.apply(now);
            publish();
            now += 20;
            assert(!backend.editError());
        };
        publish();

        const float base_cutoff_log = synth[0]->filter_logfreq_coefs[COEF_CONST];
        assert(backend.editParameter(edit::ModDest, 3.0f)); // FLT CUT
        assert(backend.editParameter(edit::ModAmount, 80.0f));
        flush();

        // CC1 = 0 -> filter cutoff equals base
        midi(0xb0, 1, 0);
        runtime.apply(now);
        assert(std::fabs(synth[0]->filter_logfreq_coefs[COEF_CONST] - base_cutoff_log) < 1e-5f);

        // CC1 = 64 -> filter cutoff shifts up by ~1.21 octaves across all 8 voices
        midi(0xb0, 1, 64);
        runtime.apply(now);
        const float log_cc64 = synth[0]->filter_logfreq_coefs[COEF_CONST];
        assert(log_cc64 > base_cutoff_log + 1.0f);
        for (unsigned v = 0; v < 8; ++v) {
            uint16_t voice = 0;
            assert(instrument_get_num_voices(v + 1, &voice) == 1);
            assert(std::fabs(synth[voice_to_base_osc[voice]]->filter_logfreq_coefs[COEF_CONST] - log_cc64) < 1e-5f);
        }

        // CC1 = 127 -> filter cutoff shifts up by 2.4 octaves (0.8 * 3.0)
        midi(0xb0, 1, 127);
        runtime.apply(now);
        const float log_cc127 = synth[0]->filter_logfreq_coefs[COEF_CONST];
        assert(std::fabs(log_cc127 - (base_cutoff_log + 2.4f)) < 1e-4f);

        // Compose with manual Cutoff edit (500 Hz) while CC1 = 127
        assert(backend.editParameter(edit::Cutoff, 500.0f));
        flush();
        const float log_500_cc127 = synth[0]->filter_logfreq_coefs[COEF_CONST];
        assert(std::fabs(log_500_cc127 - (logfreq_of_freq(500.0f) + 2.4f)) < 1e-4f);

        // Compare restores exact factory cutoff even with CC1 = 127
        assert(backend.editAction(0));
        flush();
        assert(synth[0]->filter_logfreq_coefs[COEF_CONST] == base_cutoff_log);
        assert(backend.editAction(0));
        flush();
        assert(std::fabs(synth[0]->filter_logfreq_coefs[COEF_CONST] - log_500_cc127) < 1e-4f);

        // Disconnect resets CC1 modulation and restores edited base cutoff (500 Hz)
        runtime.usbInput().disconnect();
        runtime.apply(now);
        assert(std::fabs(freq_of_logfreq(synth[0]->filter_logfreq_coefs[COEF_CONST]) - 500.0f) < 0.5f);

        // Unsupported ModSource (2 = AMP ENV) and unsupported ModDest (2 = SUB LVL) must be rejected
        const uint32_t errs = runtime.snapshot().edit.errors;
        edit::Request bad{};
        bad.patch = 0;
        bad.generation = runtime.snapshot().generation;
        bad.request = runtime.snapshot().edit.request + 1;
        bad.mask = edit::bit(edit::ModSource);
        bad.values[edit::ModSource] = 2.0f;
        assert(runtime.commands().edits.push(bad));
        runtime.apply(now);
        assert(runtime.snapshot().edit.errors == errs + 1);

        bad.request = runtime.snapshot().edit.request + 1;
        bad.mask = edit::bit(edit::ModDest);
        bad.values[edit::ModDest] = 2.0f;
        assert(runtime.commands().edits.push(bad));
        runtime.apply(now);
        assert(runtime.snapshot().edit.errors == errs + 2);

        runtime.end();
    }
    std::puts("PASS Gate E Mod Matrix & CC1: Mod Wheel -> Pitch (Juno/DX7), Mod Wheel -> Filter Cutoff (Juno), CC1=0/64/127, disconnect reset, invalid route rejection");
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    testCodecExtendedCapacity();
    testLfoControls(0);   // Juno Brass Set 1
    testLfoControls(4);   // Juno Moving Strings
    testLfoControls(128); // DX7 Brass 1
    testLfoControls(135); // DX7 Piano 1
    testModMatrixAndCc1Composition();
    return 0;
}
