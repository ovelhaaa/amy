#include "local_performance_backend.h"
#include "user_preset_nvs.h"
#include "user_preset_fx_nvs.h"
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

static uint64_t pcmEnergy(const std::vector<int16_t>& pcm) {
    uint64_t sum = 0;
    for (int16_t s : pcm) {
        const int64_t v = s;
        sum += uint64_t(v * v);
    }
    return sum;
}

enum class EvalMode { Original, MinVal, MaxVal, Edited, Compared, Reverted, Recalled };

static float targetValueFor(unsigned param, float orig, EvalMode mode) {
    if (mode == EvalMode::MinVal) return 0.0f;
    if (mode == EvalMode::MaxVal) {
        if (param == edit::FmFeedback || (param >= edit::OpVel0 && param <= edit::OpVel5)) return 100.0f;
        if (param >= edit::OpAttack0 && param <= edit::OpAttack5) return 2000.0f;
        return 4000.0f;
    }
    if (param == edit::FmFeedback) return orig > 50.0f ? 0.0f : 100.0f;
    if (param >= edit::OpAttack0 && param <= edit::OpAttack5) return orig > 200.0f ? 5.0f : 450.0f;
    if (param >= edit::OpDecay0 && param <= edit::OpDecay5) return orig > 500.0f ? 40.0f : 1200.0f;
    return 85.0f; // OpVel0..5
}

static std::vector<int16_t> renderDx7Scenario(
    unsigned patch_id,
    unsigned param,
    EvalMode mode,
    bool eight_voices,
    bool harmony_on,
    uint8_t velocity = 80
) {
    assert(runtime.begin());
    assert(runtime.commands().push({amy::CommandType::Patch, uint16_t(patch_id)}));
    runtime.apply(0);
    runtime.loadPendingPatch();
    assert(runtime.snapshot().patch_state == amy::PatchState::Ready);
    assert(runtime.snapshot().edit.family == 0);

    NvsUserPresets store;
    assert(store.begin());
    fx::NvsFxPresets fx_store;
    assert(fx_store.begin());
    LocalAmyPerformanceBackend backend(runtime.commands());
    backend.presetStore(&store);
    backend.fxPresetStore(&fx_store);
    auto publish = [&] { backend.publish(runtime.snapshot(), false, 0, 0); };
    publish();
    unsigned now = 20;
    auto flush = [&] {
        backend.service(now);
        runtime.apply(now);
        publish();
        now += 20;
        assert(!backend.editError());
    };

    const auto avail = runtime.snapshot().edit.available;
    if (!(avail & edit::bit(param))) {
        runtime.end();
        return {};
    }

    const float orig = runtime.snapshot().edit.original[param];
    const float val = targetValueFor(param, orig, mode);

    // Capture native operator topology and breakpoints before editing.
    uint16_t orig_algo_src[8][6]{};
    uint32_t orig_bp_times[64][5]{};
    float orig_bp_vals[64][5]{};
    float orig_fb[8]{};
    float orig_vel_coef[64]{};
    for (unsigned v = 0; v < 8; ++v) {
        uint16_t voice = 0;
        assert(instrument_get_num_voices(v + 1, &voice) == 1);
        const unsigned base = voice_to_base_osc[voice];
        assert(synth[base]->wave == ALGO);
        orig_fb[v] = synth[base]->feedback;
        for (unsigned op = 0; op < 6; ++op) {
            orig_algo_src[v][op] = synth[base]->algo_source[5 - op];
        }
    }
    for (unsigned o = 0; o < 64; ++o) if (synth[o]) {
        orig_vel_coef[o] = synth[o]->amp_coefs[COEF_VEL];
        if (synth[o]->breakpoint_times[0] && synth[o]->breakpoint_values[0]) {
            for (unsigned j = 0; j < 5; ++j) {
                orig_bp_times[o][j] = synth[o]->breakpoint_times[0][j];
                orig_bp_vals[o][j] = synth[o]->breakpoint_values[0][j];
            }
        }
    }

    if (mode == EvalMode::Recalled) {
        assert(backend.editAction(3, 1));
        runtime.apply(now);
        runtime.loadPendingPatch();
        publish();
        flush();
        assert(std::fabs(runtime.snapshot().edit.applied[param] - val) < 0.1f);
    } else if (mode != EvalMode::Original) {
        assert(backend.editParameter(param, val));
        flush();
        if (mode == EvalMode::Compared) {
            assert(backend.editAction(0));
            flush();
        } else if (mode == EvalMode::Reverted) {
            assert(backend.editAction(1));
            flush();
        } else if (mode == EvalMode::Edited) {
            assert(backend.editAction(2, 1, "GATE D DX7 EDIT"));
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

    const bool active = (mode == EvalMode::MinVal || mode == EvalMode::MaxVal || mode == EvalMode::Edited || mode == EvalMode::Recalled);
    // Verify 8-voice native application and strict operator isolation.
    for (unsigned v = 0; v < 8; ++v) {
        uint16_t voice = 0;
        assert(instrument_get_num_voices(v + 1, &voice) == 1);
        const unsigned base = voice_to_base_osc[voice];
        assert(synth[base]->wave == ALGO);
        if (param == edit::FmFeedback && active) {
            assert(std::fabs(synth[base]->feedback - (val / 625.0f)) < 1e-6f);
        } else {
            assert(synth[base]->feedback == orig_fb[v]);
        }
        for (unsigned op = 0; op < 6; ++op) {
            const uint16_t op_osc = synth[base]->algo_source[5 - op];
            assert(op_osc == orig_algo_src[v][op]);
            assert(synth[op_osc]->eg_type[0] == ENVELOPE_DX7);
            // All 5 breakpoint values must remain bit-identical (curve and levels preserved).
            assert(std::memcmp(synth[op_osc]->breakpoint_values[0], orig_bp_vals[op_osc], 5 * sizeof(float)) == 0);
            if (param == edit::OpAttack0 + op && active) {
                const uint32_t expected_samples = uint32_t(std::lround(val * amy::kSampleRateHz / 1000.0f));
                assert(synth[op_osc]->breakpoint_times[0][1] == expected_samples);
                assert(synth[op_osc]->breakpoint_times[0][0] == orig_bp_times[op_osc][0]);
                assert(synth[op_osc]->breakpoint_times[0][2] == orig_bp_times[op_osc][2]);
                assert(synth[op_osc]->breakpoint_times[0][3] == orig_bp_times[op_osc][3]);
                assert(synth[op_osc]->breakpoint_times[0][4] == orig_bp_times[op_osc][4]);
            } else if (param == edit::OpDecay0 + op && active) {
                const uint32_t expected_samples = uint32_t(std::lround(val * amy::kSampleRateHz / 1000.0f));
                const unsigned seg = std::fabs(orig_bp_vals[op_osc][2] - orig_bp_vals[op_osc][1]) > 1e-4f ? 2u : 3u;
                assert(synth[op_osc]->breakpoint_times[0][seg] == expected_samples);
                for (unsigned j = 0; j < 5; ++j) if (j != seg) {
                    assert(synth[op_osc]->breakpoint_times[0][j] == orig_bp_times[op_osc][j]);
                }
            } else {
                assert(std::memcmp(synth[op_osc]->breakpoint_times[0], orig_bp_times[op_osc], 5 * sizeof(uint32_t)) == 0);
            }
            if (param == edit::OpVel0 + op && active) {
                assert(std::fabs(synth[op_osc]->amp_coefs[COEF_VEL] - (val / 50.0f)) < 1e-5f);
            } else {
                assert(synth[op_osc]->amp_coefs[COEF_VEL] == orig_vel_coef[op_osc]);
            }
        }
    }

    if (harmony_on) {
        auto c = harmony::preset(1);
        c.chord = 13; // 4-note chord
        assert(runtime.commands().push({amy::CommandType::Harmony, 0, 0, 0, harmony::pack(c)}));
        runtime.apply(now);
    }

    my_srand48(777);
    if (harmony_on) {
        midi(0x90, 60, velocity);
        if (eight_voices) midi(0x90, 62, velocity);
    } else if (eight_voices) {
        for (unsigned i = 0; i < 8; ++i) midi(0x90, uint8_t(52 + i * 2), velocity);
    } else {
        midi(0x90, 60, velocity);
    }
    runtime.apply(now);
    const unsigned expected_voices = harmony_on ? (eight_voices ? 8u : 4u) : (eight_voices ? 8u : 1u);
    assert(runtime.snapshot().active_notes == expected_voices);

    std::vector<int16_t> pcm;
    pcm.reserve(72 * 512);
    const auto alloc_before = probe_calls();
    const auto bytes_before = probe_bytes();

    for (unsigned b = 0; b < 72; ++b) {
        if (b == 30) midi(0xb0, 64, 127); // Sustain pedal on
        if (b == 34) {
            if (harmony_on) {
                midi(0x80, 60, 0);
                if (eight_voices) midi(0x80, 62, 0);
            } else if (eight_voices) {
                for (unsigned i = 0; i < 8; ++i) midi(0x80, uint8_t(52 + i * 2), 0);
            } else {
                midi(0x80, 60, 0);
            }
        }
        if (b == 44) midi(0xb0, 64, 0); // Sustain pedal off -> release
        if (b == 52) { // Retrigger
            if (harmony_on) {
                midi(0x90, 60, velocity);
                if (eight_voices) midi(0x90, 62, velocity);
            } else if (eight_voices) {
                for (unsigned i = 0; i < 8; ++i) midi(0x90, uint8_t(52 + i * 2), velocity);
            } else {
                midi(0x90, 60, velocity);
            }
        }
        if (b == 64) {
            if (harmony_on) {
                midi(0x80, 60, 0);
                if (eight_voices) midi(0x80, 62, 0);
            } else if (eight_voices) {
                for (unsigned i = 0; i < 8; ++i) midi(0x80, uint8_t(52 + i * 2), 0);
            } else {
                midi(0x80, 60, 0);
            }
        }
        probe_realtime(true);
        runtime.apply(now + b * 5);
        const int16_t* out = runtime.render();
        probe_realtime(false);
        assert(out);
        if (b == 36) assert(runtime.snapshot().active_notes == expected_voices); // Held by sustain
        if (b == 46) assert(runtime.snapshot().active_notes == 0);               // Released after CC64=0
        if (b == 54) assert(runtime.snapshot().active_notes == expected_voices); // Retriggered
        pcm.insert(pcm.end(), out, out + 512);
    }

    assert(probe_calls() == alloc_before && probe_bytes() == bytes_before);
    assert(runtime.snapshot().active_notes == 0);
    assert(runtime.snapshot().clipped == 0);
    assert(runtime.snapshot().oom_count == 0);
    assert(runtime.snapshot().edit.errors == 0);
    runtime.end();
    return pcm;
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const unsigned dx7_patches[] = {128u, 135u, 138u, 142u}; // Brass 1, Piano 1, E.Piano 1, Bass 1

    // 1. Verify availability, contextual N/A, invalid values, and 1/8-voice/Harmony/Compare/Revert/Save/Recall across DX7 patches.
    for (unsigned patch_id : dx7_patches) {
        fake_nvs::blobs.clear();
        fake_nvs::free_entries = 700;

        // Test FmFeedback (ID 29)
        const auto fb_orig = renderDx7Scenario(patch_id, edit::FmFeedback, EvalMode::Original, false, false);
        assert(!fb_orig.empty());
        const auto fb_min = renderDx7Scenario(patch_id, edit::FmFeedback, EvalMode::MinVal, false, false);
        const auto fb_max = renderDx7Scenario(patch_id, edit::FmFeedback, EvalMode::MaxVal, false, false);
        const auto fb_edit = renderDx7Scenario(patch_id, edit::FmFeedback, EvalMode::Edited, false, false);
        assert(fb_min != fb_max);
        assert(fb_edit != fb_orig);
        assert(renderDx7Scenario(patch_id, edit::FmFeedback, EvalMode::Compared, false, false) == fb_orig);
        assert(renderDx7Scenario(patch_id, edit::FmFeedback, EvalMode::Reverted, false, false) == fb_orig);
        assert(renderDx7Scenario(patch_id, edit::FmFeedback, EvalMode::Recalled, false, false) == fb_edit);

        // 8-voice Harmony test on FmFeedback
        const auto fb_h_orig = renderDx7Scenario(patch_id, edit::FmFeedback, EvalMode::Original, true, true);
        const auto fb_h_edit = renderDx7Scenario(patch_id, edit::FmFeedback, EvalMode::Edited, true, true);
        assert(fb_h_orig != fb_h_edit);

        // Test per-operator Attack, Decay, Velocity Sensitivity
        unsigned att_count = 0, dec_count = 0, vel_count = 0;
        for (unsigned op = 0; op < 6; ++op) {
            for (unsigned base_param : {unsigned(edit::OpAttack0), unsigned(edit::OpDecay0), unsigned(edit::OpVel0)}) {
                const unsigned param = base_param + op;
                fake_nvs::blobs.clear();
                fake_nvs::free_entries = 700;
                const auto orig = renderDx7Scenario(patch_id, param, EvalMode::Original, false, false, 60);
                if (orig.empty()) {
                    std::printf("GATE_D patch=%u op=%u param=%u contextual N/A\n", patch_id, op + 1, param);
                    continue;
                }
                const auto edited = renderDx7Scenario(patch_id, param, EvalMode::Edited, false, false, 60);
                const auto compared = renderDx7Scenario(patch_id, param, EvalMode::Compared, false, false, 60);
                const auto reverted = renderDx7Scenario(patch_id, param, EvalMode::Reverted, false, false, 60);
                const auto recalled = renderDx7Scenario(patch_id, param, EvalMode::Recalled, false, false, 60);
                assert(compared == orig);
                assert(reverted == orig);
                assert(recalled == edited);
                if (edited != orig) {
                    if (base_param == edit::OpAttack0) ++att_count;
                    else if (base_param == edit::OpDecay0) ++dec_count;
                    else ++vel_count;
                }
            }
        }
        std::printf("GATE_D patch=%u audible_ops: attack=%u decay=%u vel=%u\n", patch_id, att_count, dec_count, vel_count);
        assert(att_count > 0 && dec_count > 0 && vel_count > 0);
    }

    // 2. Verify musical velocity curve on OpVel0..5 across velocities 40, 80, 127 without altering incoming MIDI velocity.
    {
        const auto v40 = renderDx7Scenario(135, edit::OpVel0, EvalMode::Edited, false, false, 40);
        const auto v80 = renderDx7Scenario(135, edit::OpVel0, EvalMode::Edited, false, false, 80);
        const auto v127 = renderDx7Scenario(135, edit::OpVel0, EvalMode::Edited, false, false, 127);
        const uint64_t e40 = pcmEnergy(v40), e80 = pcmEnergy(v80), e127 = pcmEnergy(v127);
        std::printf("GATE_D patch=135 OpVel0=85%% energy: vel40=%llu vel80=%llu vel127=%llu\n",
                    static_cast<unsigned long long>(e40),
                    static_cast<unsigned long long>(e80),
                    static_cast<unsigned long long>(e127));
        assert(e40 < e80 && e80 < e127);
        // At velocity=127 (1.0f), COEF_VEL log attenuation is 0, so Original and Edited at vel=127 match.
        const auto v127_orig = renderDx7Scenario(135, edit::OpVel0, EvalMode::Original, false, false, 127);
        assert(v127 == v127_orig);
    }

    // 3. Verify rejection of invalid values and cross-family / unavailable operator parameters.
    {
        assert(runtime.begin());
        assert(runtime.commands().push({amy::CommandType::Patch, 135}));
        runtime.apply(0);
        runtime.loadPendingPatch();
        const auto& s = runtime.snapshot();
        // On DX7 PIANO 1 (135), OP3 (v5) has flat sustain L1=L2=L3=1.0, so OpDecay2 must be unavailable.
        assert(!(s.edit.available & edit::bit(edit::OpDecay0 + 2)));
        uint32_t errs = s.edit.errors;
        auto submitBad = [&](unsigned p, float val) {
            edit::Request r{};
            r.patch = s.patch_id;
            r.generation = s.generation;
            r.request = runtime.snapshot().edit.request + 1;
            r.mask = edit::bit(p);
            r.values[p] = val;
            assert(runtime.commands().edits.push(r));
            runtime.apply(10);
            assert(runtime.snapshot().edit.errors == ++errs);
        };
        submitBad(edit::FmFeedback, -1.0f);
        submitBad(edit::FmFeedback, 101.0f);
        submitBad(edit::FmFeedback, NAN);
        submitBad(edit::OpAttack0, -5.0f);
        submitBad(edit::OpAttack0, 2001.0f);
        submitBad(edit::OpDecay0 + 1, 4001.0f);
        submitBad(edit::OpDecay0 + 2, 100.0f); // Unavailable on patch 135 OP3
        submitBad(edit::OpVel0, 105.0f);
        submitBad(edit::Cutoff, 1000.0f);      // Subtractive-only parameter on FM patch
        runtime.end();
    }

    std::puts("PASS Gate D: DX7 FmFeedback, OpAttack1..6, OpDecay1..6, OpVel1..6 across 128/135/138/142, 1/8 voices, Harmony, sustain, retrigger, Compare/Revert/Save/Recall");
    return 0;
}
