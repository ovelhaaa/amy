// Run with tests/run_smk_amy_boot.ps1. Uses the real adapter and AMY engine.
#include "amy_adapter.h"
#include "synth_config.h"
#include "diagnostics.h"
#include <cassert>
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
    std::puts("PASS: boot, complete voice allocation, 256 patch changes, MIDI CC, note release, drums and Panic");
}
