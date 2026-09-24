// Run with tests/run_smk_amy_boot.ps1. Uses the real adapter and AMY engine.
#include "amy_adapter.h"
#include "synth_config.h"
#include "diagnostics.h"
#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

namespace smk {
Diagnostics& Diagnostics::instance() { static Diagnostics instance; return instance; }
DiagnosticCounters& Diagnostics::counters() { return counters_; }
struct AmyAdapterTestAccess {
    static bool service(AmyAdapter& adapter) { return adapter.serviceBlock(); }
};
}

extern "C" {
#include "amy.h"
void delay_ms(uint32_t) {}
void amy_update_tasks() {}
int16_t* amy_render_audio() { return amy_simple_fill_buffer(); }
size_t amy_i2s_write(const uint8_t*, size_t nbytes) { return nbytes; }
struct FmAlgorithm { uint8_t ops[MAX_ALGO_OPS]; };
extern const struct FmAlgorithm algorithms[33];
}

static int allocated_oscs(int instrument, bool require_complete) {
    uint16_t voices[MAX_VOICES_PER_INSTRUMENT];
    int count = instrument_get_num_voices(instrument, voices);
    int total = 0;
    for (int v = 0; v < count; ++v) {
        int assigned = 0;
        for (int osc = 0; osc < AMY_OSCS; ++osc) {
            if (osc_to_voice[osc] == voices[v]) ++assigned;
        }
        if (require_complete) assert(assigned == instrument_get_oscs_per_voice(instrument));
        total += assigned;
    }
    return total;
}

static bool render_signal(smk::AmyAdapter& adapter, int blocks) {
    bool signal = false;
    for (int block = 0; block < blocks; ++block) {
        assert(smk::AmyAdapterTestAccess::service(adapter));
        const int16_t* samples = adapter.render();
        assert(samples != nullptr);
        for (int i = 0; i < AMY_BLOCK_SIZE * AMY_NCHANS; ++i) {
            if (samples[i] != 0) signal = true;
        }
    }
    return signal;
}

// ─────────────────────────────────────────────────────────────
// Real-AMY FM integration checks. These read live synth[] operator state, so
// they validate the whole chain: AMY preset -> algo_source -> snapshot ->
// relative FM control (not just PatchManager -> factor with a mock adapter).
// ─────────────────────────────────────────────────────────────
static constexpr uint8_t FM_OUT_BUS_ONE = 1 << 0;
static constexpr uint8_t FM_OUT_BUS_TWO = 1 << 1;

static void fm_service_render(smk::AmyAdapter& adapter) {
    for (int i = 0; i < 2; ++i) {
        assert(smk::AmyAdapterTestAccess::service(adapter));
        adapter.render();
    }
}

static uint16_t fm_synth1_base() {
    uint16_t voices[MAX_VOICES_PER_INSTRUMENT];
    const int nv = instrument_get_num_voices(1, voices);
    if (nv <= 0) return UINT16_MAX;
    return voice_to_base_osc[voices[0]];
}

static bool fm_mod_for_algo(uint8_t algo_id, uint8_t op) {
    if (algo_id < 1 || algo_id > 32) algo_id = 1;
    return (algorithms[algo_id].ops[op] & (FM_OUT_BUS_ONE | FM_OUT_BUS_TWO)) != 0;
}

// Returns the number of set operators, or -1 when Synth 1 is not an ALGO voice.
static int fm_read_ops(int16_t ops_out[MAX_ALGO_OPS], bool mods_out[MAX_ALGO_OPS],
                       float amps_out[MAX_ALGO_OPS], float logratio_out[MAX_ALGO_OPS]) {
    const uint16_t base = fm_synth1_base();
    if (!AMY_IS_SET(base) || synth[base]->wave != ALGO) return -1;
    uint8_t algo_id = synth[base]->algorithm;
    if (algo_id < 1 || algo_id > 32) algo_id = 1;
    int count = 0;
    for (int op = 0; op < MAX_ALGO_OPS; ++op) {
        const int16_t o = synth[base]->algo_source[op];
        ops_out[op] = o;
        mods_out[op] = fm_mod_for_algo(algo_id, static_cast<uint8_t>(op));
        amps_out[op] = AMY_IS_SET(o) ? synth[o]->amp_coefs[COEF_CONST] : 0.0f;
        logratio_out[op] = AMY_IS_SET(o) ? synth[o]->logratio : 0.0f;
        if (AMY_IS_SET(o)) ++count;
    }
    return count;
}

static void run_real_amy_fm_relative(smk::AmyAdapter& adapter) {
    int chosen = -1;
    for (int patch = 128; patch < 200 && chosen < 0; ++patch) {
        adapter.loadPreset(1, static_cast<uint16_t>(patch), 8);
        fm_service_render(adapter);
        int16_t ops[MAX_ALGO_OPS]; bool mods[MAX_ALGO_OPS];
        float amps[MAX_ALGO_OPS]; float lrs[MAX_ALGO_OPS];
        if (fm_read_ops(ops, mods, amps, lrs) < 0) continue;

        int mod_count = 0, distinct = 0;
        float first = -1.0f;
        for (int op = 0; op < MAX_ALGO_OPS; ++op) {
            if (!mods[op] || !AMY_IS_SET(ops[op])) continue;
            ++mod_count;
            if (first < 0.0f) first = amps[op];
            else if (std::fabs(amps[op] - first) > 1e-3f) ++distinct;
        }
        if (mod_count < 2 || distinct < 1) continue;
        chosen = patch;

        // Relative mod index 2x scales every modulator's captured baseline,
        // including modulators that start at different levels, and leaves
        // carriers untouched.
        adapter.setFmModIndex(1, 2.0f);
        fm_service_render(adapter);
        for (int op = 0; op < MAX_ALGO_OPS; ++op) {
            if (!AMY_IS_SET(ops[op])) continue;
            const float got = synth[ops[op]]->amp_coefs[COEF_CONST];
            const float expected = mods[op] ? std::min(amps[op] * 2.0f, 16.0f) : amps[op];
            assert(std::fabs(got - expected) < 1e-3f);
        }
        // Center (1x) restores the exact baseline.
        adapter.setFmModIndex(1, 1.0f);
        fm_service_render(adapter);
        for (int op = 0; op < MAX_ALGO_OPS; ++op) {
            if (!AMY_IS_SET(ops[op])) continue;
            assert(std::fabs(synth[ops[op]]->amp_coefs[COEF_CONST] - amps[op]) < 1e-4f);
        }
        // Relative ratio 2x adds exactly one octave to modulators.
        adapter.setFmRatio(1, 2.0f);
        fm_service_render(adapter);
        for (int op = 0; op < MAX_ALGO_OPS; ++op) {
            if (!AMY_IS_SET(ops[op])) continue;
            const float expected = mods[op] ? (lrs[op] + 1.0f) : lrs[op];
            assert(std::fabs(synth[ops[op]]->logratio - expected) < 1e-3f);
        }
        adapter.setFmRatio(1, 1.0f);
        fm_service_render(adapter);
        for (int op = 0; op < MAX_ALGO_OPS; ++op) {
            if (!AMY_IS_SET(ops[op])) continue;
            assert(std::fabs(synth[ops[op]]->logratio - lrs[op]) < 1e-4f);
        }
    }
    assert(chosen >= 0);
    std::printf("PASS: real AMY FM relative (patch %d): modulators scaled 2x, carriers intact, center exact\n", chosen);
}

static void run_real_amy_fm_algorithm_change(smk::AmyAdapter& adapter) {
    // Patch 128 uses algorithm 22: modulators {0,4}, carriers {1,2,3,5}.
    // Algorithm 18 reclassifies {1,2,3} as modulators. A stale snapshot would
    // leave them untouched; dynamic classification must scale them.
    adapter.loadPreset(1, 128, 8);
    fm_service_render(adapter);
    int16_t ops[MAX_ALGO_OPS]; bool mods_a[MAX_ALGO_OPS];
    float amps[MAX_ALGO_OPS]; float lrs[MAX_ALGO_OPS];
    assert(fm_read_ops(ops, mods_a, amps, lrs) >= 0);
    assert(synth[fm_synth1_base()]->algorithm == 22);
    assert(mods_a[0] && !mods_a[1] && !mods_a[2] && !mods_a[3] && mods_a[4] && !mods_a[5]);

    adapter.setFmAlgorithm(1, 18);
    fm_service_render(adapter);
    assert(synth[fm_synth1_base()]->algorithm == 18);

    adapter.setFmModIndex(1, 2.0f);
    fm_service_render(adapter);
    for (int op = 0; op < MAX_ALGO_OPS; ++op) {
        if (!AMY_IS_SET(ops[op])) continue;
        const bool mod_b = fm_mod_for_algo(18, static_cast<uint8_t>(op));
        const float expected = mod_b ? std::min(amps[op] * 2.0f, 16.0f) : amps[op];
        assert(std::fabs(synth[ops[op]]->amp_coefs[COEF_CONST] - expected) < 1e-3f);
    }
    adapter.setFmModIndex(1, 1.0f);
    fm_service_render(adapter);
    std::printf("PASS: real AMY FM algorithm change (22 -> 18): carrier/modulator classification is dynamic\n");
}

static void run_real_amy_fm_patch_switch(smk::AmyAdapter& adapter) {
    int first = -1, second = -1;
    int16_t first_ops[MAX_ALGO_OPS]; float first_amps[MAX_ALGO_OPS];
    int16_t b_ops[MAX_ALGO_OPS]; bool b_mods[MAX_ALGO_OPS]; float b_amps[MAX_ALGO_OPS];
    for (int patch = 128; patch < 200 && second < 0; ++patch) {
        adapter.loadPreset(1, static_cast<uint16_t>(patch), 8);
        fm_service_render(adapter);
        int16_t ops[MAX_ALGO_OPS]; bool mods[MAX_ALGO_OPS];
        float amps[MAX_ALGO_OPS]; float lrs[MAX_ALGO_OPS];
        if (fm_read_ops(ops, mods, amps, lrs) < 0) continue;
        if (first < 0) {
            first = patch;
            for (int op = 0; op < MAX_ALGO_OPS; ++op) { first_ops[op] = ops[op]; first_amps[op] = amps[op]; }
            continue;
        }
        bool differs = false;
        for (int op = 0; op < MAX_ALGO_OPS; ++op) {
            if (mods[op] && AMY_IS_SET(ops[op]) && AMY_IS_SET(first_ops[op]) &&
                std::fabs(amps[op] - first_amps[op]) > 1e-3f) {
                differs = true;
            }
        }
        if (!differs) continue;
        second = patch;
        for (int op = 0; op < MAX_ALGO_OPS; ++op) { b_ops[op] = ops[op]; b_mods[op] = mods[op]; b_amps[op] = amps[op]; }
    }
    assert(first >= 0 && second >= 0);

    // Load A then B: the FM baseline must be B's, with no reference to A left.
    adapter.loadPreset(1, static_cast<uint16_t>(first), 8);
    fm_service_render(adapter);
    adapter.loadPreset(1, static_cast<uint16_t>(second), 8);
    fm_service_render(adapter);
    adapter.setFmModIndex(1, 2.0f);
    fm_service_render(adapter);
    for (int op = 0; op < MAX_ALGO_OPS; ++op) {
        if (!AMY_IS_SET(b_ops[op])) continue;
        const float expected = b_mods[op] ? std::min(b_amps[op] * 2.0f, 16.0f) : b_amps[op];
        assert(std::fabs(synth[b_ops[op]]->amp_coefs[COEF_CONST] - expected) < 1e-3f);
    }
    adapter.setFmModIndex(1, 1.0f);
    fm_service_render(adapter);
    std::printf("PASS: real AMY FM patch switch A(%d) -> B(%d): snapshot tracks the loaded patch\n", first, second);
}

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--legacy") == 0) {
        amy_config_t config = amy_default_config();
        config.max_oscs = 120;
        config.features.default_synths = 1;
        config.platform.multicore = 0;
        config.platform.multithread = 0;
        amy_start(config);
        int required = 0;
        int assigned = 0;
        for (int id : {0, 10, 2, 1}) {
            const int requested = instrument_get_num_voices(id, nullptr) * instrument_get_oscs_per_voice(id);
            const int actual = allocated_oscs(id, false);
            std::printf("Legacy synth %d: requested=%d allocated=%d\n", id, requested, actual);
            required += requested;
            assigned += actual;
        }
        std::printf("Legacy total: requested=%d allocated=%d free=%d\n", required, assigned, AMY_OSCS - assigned);
        for (int osc = 0; osc < AMY_OSCS;) {
            if (osc_to_voice[osc] != UINT16_MAX) { ++osc; continue; }
            const int start = osc;
            while (osc < AMY_OSCS && osc_to_voice[osc] == UINT16_MAX) ++osc;
            std::printf("Legacy free block: %d..%d (%d oscs)\n", start, osc - 1, osc - start);
        }
        assert(required == 117);
        assert(assigned == 111);
        amy_stop();
        return 0;
    }

    smk::AmyAdapter adapter;
    assert(adapter.begin(AMY_SAMPLE_RATE));
    assert(amy_global.config.features.default_synths == 0);
    assert(!instrument_number_exists(0, nullptr));
    assert(!instrument_number_exists(2, nullptr));
    assert(instrument_get_patch_number(1) == 0);
    assert(AMY_OSCS == smk::config::kMaxOscillators);
    assert(instrument_get_num_voices(1, nullptr) == smk::config::kDefaultVoiceCount);
    // The drum patch's embedded oscs_per_voice command replaces its patch ID.
    assert(instrument_get_flags(10) == (SYNTH_FLAGS_NOTES_VIA_MIDI | SYNTH_FLAGS_IGNORE_NOTE_OFFS));
    assert(instrument_get_num_voices(10, nullptr) == 1);
    assert(instrument_get_bus(1) == 0);
    assert(instrument_get_bus(10) == 1);
    assert(instrument_noteon_delay_ms(1) == 4);
    assert(allocated_oscs(10, true) == 32);
    char drum_mapping[AMY_WIRE_COMMAND_LEN];
    assert(midi_fetch_mapping_command(10, MIDI_MAP_TYPE_NOTE, 36, drum_mapping, sizeof(drum_mapping)));
    const int main_oscs = allocated_oscs(1, true);
    assert(main_oscs > 0);
    std::printf("Adapter boot: main=%d drums=32 free=%d\n", main_oscs, AMY_OSCS - main_oscs - 32);
    assert(!render_signal(adapter, 16));
    // Panic must cancel a Note On still waiting for the synth's 4 ms delay.
    adapter.noteOn(0, 60, 100);
    adapter.panic();
    assert(!render_signal(adapter, 32));
    adapter.noteOn(0, 60, 100);
    assert(render_signal(adapter, 32));
    adapter.noteOff(0, 60);
    render_signal(adapter, 1000);
    assert(!render_signal(adapter, 16));
    // Exercise every boot voice, including the voice the legacy boot left empty.
    for (int voice = 0; voice < smk::config::kDefaultVoiceCount; ++voice) {
        adapter.noteOn(0, 60 + voice, 100);
        assert(render_signal(adapter, 32));
        adapter.noteOn(0, 60 + voice, 0); // MIDI velocity-zero release semantics.
        render_signal(adapter, 1000);
        assert(!render_signal(adapter, 16));
    }

    // Loading and releasing the main synth must leave the drum mappings intact.
    for (int patch = 0; patch < 256; ++patch) {
        adapter.loadPreset(1, patch, patch < 128 ? 10 : 8);
        render_signal(adapter, 2); // Async command applied by the owner.
        allocated_oscs(1, true);
        assert(allocated_oscs(10, true) == 32);
    }
    adapter.panic();
    render_signal(adapter, 1000);
    assert(!render_signal(adapter, 16));
    adapter.noteOn(9, 36, 100);
    assert(render_signal(adapter, 32));
    adapter.noteOff(9, 36);
    adapter.panic();
    render_signal(adapter, 1000);
    assert(!render_signal(adapter, 16));

    // The legacy default-synth setup also installed this CC hook. Keep it working.
    adapter.loadPreset(1, 0, 8);
    render_signal(adapter, 2);
    adapter.controlChange(0, 71, 64);
    render_signal(adapter, 2);
    uint16_t voices[MAX_VOICES_PER_INSTRUMENT];
    const int count = instrument_get_num_voices(1, voices);
    int checked = 0;
    for (int osc = 0; osc < AMY_OSCS; ++osc) {
        for (int v = 0; v < count; ++v) {
            if (osc_to_voice[osc] == voices[v]) {
                assert(synth[osc] != nullptr);
                assert(std::fabs(synth[osc]->resonance - 2.8f) < 0.001f);
                ++checked;
            }
        }
    }
    assert(checked == 48);
    run_real_amy_fm_relative(adapter);
    run_real_amy_fm_algorithm_change(adapter);
    run_real_amy_fm_patch_switch(adapter);
    std::puts("PASS: boot, complete voice allocation, 256 patch changes, MIDI CC, note release, drums, Panic and FM");
}
