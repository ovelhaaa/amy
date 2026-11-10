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
}
using namespace smk::p4;
static amy::Runtime runtime;
static unsigned qualified_decay = 0;
static unsigned qualified_sustain = 0;
static unsigned contextual_na_cases = 0;
enum Mode { Original, Edited, Compared, Reverted, Recalled };

static void midi(uint8_t status, uint8_t a, uint8_t b = 0) {
    const uint8_t packet[] = {uint8_t(status >> 4), status, a, b};
    assert(runtime.usbInput().packet(packet, 0));
}

static bool eligibleAdsrOsc(const synthinfo* osc) {
    if (!osc || osc->role == SYNTH_IS_MOD_SOURCE || osc->eg_type[0] == ENVELOPE_DX7 ||
        osc->max_num_breakpoints[0] < 3 || !osc->breakpoint_times[0] || !osc->breakpoint_values[0]) {
        return false;
    }
    unsigned count = 0;
    for (unsigned j = 0; j < osc->max_num_breakpoints[0]; ++j) {
        if (AMY_IS_SET(osc->breakpoint_times[0][j]) || AMY_IS_SET(osc->breakpoint_values[0][j])) count = j + 1;
    }
    return count >= 3 && AMY_IS_SET(osc->breakpoint_times[0][1]) && AMY_IS_SET(osc->breakpoint_values[0][1]);
}

static std::vector<int16_t> render(unsigned param, unsigned note, bool harmony_on, Mode mode, unsigned patch) {
    assert(runtime.begin());
    assert(runtime.commands().push({amy::CommandType::Patch, uint16_t(patch)}));
    runtime.apply(0);
    runtime.loadPendingPatch();
    if (!(runtime.snapshot().edit.available & (uint64_t(1) << param))) {
        ++contextual_na_cases;
        runtime.end();
        return {};
    }
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
    uint32_t orig_bp_times[64][MAX_BREAKPOINTS]{};
    float orig_bp_vals[64][MAX_BREAKPOINTS]{};
    for (unsigned i = 0; i < 64; ++i) {
        if (synth[i] && synth[i]->breakpoint_times[0] && synth[i]->breakpoint_values[0]) {
            const unsigned n = synth[i]->max_num_breakpoints[0] <= MAX_BREAKPOINTS
                                   ? synth[i]->max_num_breakpoints[0]
                                   : MAX_BREAKPOINTS;
            for (unsigned j = 0; j < n; ++j) {
                orig_bp_times[i][j] = synth[i]->breakpoint_times[0][j];
                orig_bp_vals[i][j] = synth[i]->breakpoint_values[0][j];
            }
        }
    }
    const float native = runtime.snapshot().edit.original[param];
    const float value = (param == edit::Decay)
                            ? (std::fabs(native - 1500.0f) > 200.0f ? 1500.0f : 350.0f)
                            : (std::fabs(native - 15.0f) > 15.0f ? 15.0f : 80.0f);
    if (mode == Recalled) {
        assert(backend.editAction(3, 0));
        runtime.apply(now);
        runtime.loadPendingPatch();
        publish();
        flush();
        assert(runtime.snapshot().edit.applied[param] == value);
    } else if (mode != Original) {
        assert(backend.editParameter(param, value));
        flush();
        if (mode == Compared) {
            assert(backend.editAction(0));
            flush();
        }
        if (mode == Reverted) {
            assert(backend.editAction(1));
            flush();
        }
        if (mode == Edited) {
            assert(backend.editAction(2, 0, "GATE A ADSR"));
            runtime.apply(now);
            publish();
            assert(runtime.snapshot().storage_quiet);
            backend.service(now);
            runtime.apply(now);
            publish();
            now += 20;
            assert(!backend.editError());
            edit::Preset saved;
            assert(store.load(0, saved));
            assert(saved.values[param] == value);
        }
    }
    const bool active = (mode == Edited || mode == Recalled);
    const uint32_t expected_decay_samples = uint32_t(std::lround(value * amy::kSampleRateHz / 1000.0f));
    const float expected_sustain_val = value / 100.0f;
    unsigned eligible_osc_count = 0;
    for (unsigned i = 0; i < 64; ++i) {
        if (!synth[i] || !synth[i]->breakpoint_times[0] || !synth[i]->breakpoint_values[0]) continue;
        const bool eligible = eligibleAdsrOsc(synth[i]);
        if (eligible) ++eligible_osc_count;
        const unsigned n = synth[i]->max_num_breakpoints[0] <= MAX_BREAKPOINTS
                               ? synth[i]->max_num_breakpoints[0]
                               : MAX_BREAKPOINTS;
        for (unsigned j = 0; j < n; ++j) {
            if (eligible && active && j == 1 && param == edit::Decay) {
                assert(synth[i]->breakpoint_times[0][j] == expected_decay_samples);
            } else {
                assert(synth[i]->breakpoint_times[0][j] == orig_bp_times[i][j]);
            }
            if (eligible && active && j == 1 && param == edit::Sustain) {
                assert(std::fabs(synth[i]->breakpoint_values[0][j] - expected_sustain_val) < 1e-6f);
            } else {
                assert(std::memcmp(&synth[i]->breakpoint_values[0][j], &orig_bp_vals[i][j], sizeof(float)) == 0);
            }
        }
    }
    assert(eligible_osc_count >= 8 && (eligible_osc_count % 8) == 0);

    if (harmony_on) {
        auto c = harmony::preset(1);
        c.chord = 13;
        assert(runtime.commands().push({amy::CommandType::Harmony, 0, 0, 0, harmony::pack(c)}));
        runtime.apply(now);
    }
    my_srand48(123);
    midi(0x90, uint8_t(note), 100);
    if (harmony_on) midi(0x90, uint8_t(note + 2), 100);
    runtime.apply(now);
    assert(runtime.snapshot().active_notes == (harmony_on ? 8u : 1u));
    std::vector<int16_t> pcm;
    pcm.reserve(128 * 512);
    const auto allocations = probe_calls();
    for (unsigned block = 0; block < 128; ++block) {
        if (block == 60) midi(0xb0, 64, 127);
        if (block == 64) {
            midi(0x80, uint8_t(note));
            if (harmony_on) midi(0x80, uint8_t(note + 2));
        }
        if (block == 80) midi(0xb0, 64, 0);
        if (block == 96) {
            midi(0x90, uint8_t(note), 100);
            if (harmony_on) midi(0x90, uint8_t(note + 2), 100);
        }
        if (block == 120) {
            midi(0x80, uint8_t(note));
            if (harmony_on) midi(0x80, uint8_t(note + 2));
        }
        uint32_t clocks[64]{};
        if (block == 32 && mode == Edited) {
            for (unsigned i = 0; i < 64; ++i)
                if (synth[i]) clocks[i] = synth[i]->note_on_clock;
            const auto& state = runtime.snapshot();
            edit::Request req;
            req.patch = state.patch_id;
            req.generation = state.generation;
            req.mask = state.edit.mask;
            req.request = state.edit.request + 1;
            for (unsigned i = 0; i < edit::Count; ++i) req.values[i] = state.edit.manual[i];
            assert(runtime.commands().edits.push(req));
        }
        probe_realtime(true);
        runtime.apply(now + block * 5);
        if (block == 32 && mode == Edited) {
            assert(runtime.snapshot().active_notes == (harmony_on ? 8u : 1u));
            for (unsigned i = 0; i < 64; ++i)
                if (synth[i]) assert(synth[i]->note_on_clock == clocks[i]);
        }
        auto out = runtime.render();
        probe_realtime(false);
        assert(out);
        if (block == 64) assert(runtime.snapshot().active_notes == (harmony_on ? 8u : 1u));
        pcm.insert(pcm.end(), out, out + 512);
    }
    assert(probe_calls() == allocations && !runtime.snapshot().active_notes &&
           !runtime.snapshot().clipped && !runtime.snapshot().edit.errors);
    std::printf("GATE_A patch=%u param=%u note=%u voices=%u mode=%u PCM=%zu clipping=0 render_allocations=0\n",
                patch, param, note, harmony_on ? 8 : 1, unsigned(mode), pcm.size());
    runtime.end();
    return pcm;
}

static void v1CompatibilityWithGateATest() {
    fake_nvs::blobs.clear();
    fake_nvs::free_entries = 700;
    edit::Preset v1;
    v1.sequence = 1;
    v1.patch = 4;
    v1.family = 1;
    v1.mask = (1u << edit::Attack) | (1u << edit::Release);
    v1.values[edit::Attack] = 240.0f;
    v1.values[edit::Release] = 680.0f;
    std::strcpy(v1.name, "LEGACY V1 ADSR");
    uint8_t raw_v1[edit::kRecordBytes]{};
    edit::encode(v1, raw_v1);
    fake_nvs::blobs[{1u, "s02a"}] = std::vector<uint8_t>(raw_v1, raw_v1 + sizeof(raw_v1));

    assert(runtime.begin());
    assert(runtime.commands().push({amy::CommandType::Patch, 4}));
    runtime.apply(0);
    runtime.loadPendingPatch();
    NvsUserPresets store;
    assert(store.begin());
    fx::NvsFxPresets fx_store;
    assert(fx_store.begin());
    LocalAmyPerformanceBackend backend(runtime.commands());
    backend.presetStore(&store);
    backend.fxPresetStore(&fx_store);
    auto publish = [&] { backend.publish(runtime.snapshot(), false, 0, 0); };
    publish();
    assert(backend.editAction(3, 2));
    runtime.apply(20);
    runtime.loadPendingPatch();
    publish();
    backend.service(40);
    runtime.apply(40);
    publish();
    assert(!backend.editError());
    assert(runtime.snapshot().edit.applied[edit::Attack] == 240.0f);
    assert(runtime.snapshot().edit.applied[edit::Release] == 680.0f);
    assert(!(runtime.snapshot().edit.mask & ((1u << edit::Decay) | (1u << edit::Sustain))));
    runtime.end();
    std::puts("PASS Gate A V1 preset read compatibility preserved alongside V2 ADSR");
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    for (unsigned patch : {0u, 1u, 2u, 3u, 4u, 8u, 31u}) {
        for (unsigned param : {unsigned(edit::Decay), unsigned(edit::Sustain)}) {
            for (bool harmony_on : {false, true}) {
                fake_nvs::blobs.clear();
                fake_nvs::free_entries = 700;
                const auto original = render(param, 60, harmony_on, Original, patch);
                if (original.empty()) {
                    std::printf("GATE_A patch=%u param=%u contextual N/A\n", patch, param);
                    continue;
                }
                const auto edited = render(param, 60, harmony_on, Edited, patch);
                assert(edited != original);
                if (param == edit::Decay) ++qualified_decay;
                if (param == edit::Sustain) ++qualified_sustain;
                assert(render(param, 60, harmony_on, Compared, patch) == original);
                assert(render(param, 60, harmony_on, Reverted, patch) == original);
                assert(render(param, 60, harmony_on, Recalled, patch) == edited);
            }
        }
    }
    assert(qualified_decay > 0 && qualified_sustain > 0 && contextual_na_cases > 0);
    v1CompatibilityWithGateATest();
    std::puts("PASS Gate A real AMY: Juno Decay/Sustain, 8-voice breakpoint verification, 1/8 voices+Harmony, PCM/Compare/Revert/Save/Recall v2, V1 read compatibility, sustain/retrigger/release, no render allocation or clipping");
    return 0;
}
