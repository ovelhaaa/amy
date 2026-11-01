// Real AMY allocator tests. Faults target malloc_caps call positions, not host
// heap exhaustion. This tests admission, NOT materialization/Ready or rollback.
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
extern "C" {
#include "amy.h"
void delay_ms(uint32_t) {}
void amy_update_tasks() {}
void amy_platform_init() {}
void amy_platform_deinit() {}
int16_t* amy_render_audio() { return amy_simple_fill_buffer(); }
size_t amy_i2s_write(const uint8_t*, size_t n) { return n; }
void __real_free(void*);
}

static int fail_at, calls;
static bool armed;
static std::array<void*, 8> tracked{};
// Compile amy.c with MALLOC_CAPS_DEFINED for this executable only. Otherwise
// the host compiler can inline its malloc_caps into alloc_osc and bypass a
// linker --wrap hook in the same translation unit.
extern "C" void* malloc_caps(uint32_t size, uint32_t) {
    if (armed && ++calls == fail_at) return nullptr;
    void* p = std::malloc(size);
    if (armed && p) {
        bool stored = false;
        for (auto& slot : tracked) if (!slot) { slot = p; stored = true; break; }
        assert(stored);
    }
    return p;
}
extern "C" void* malloc_caps_block(uint32_t size, uint32_t caps) {
    return malloc_caps(size, caps);
}
extern "C" void __wrap_free(void* p) {
    if (armed && p) for (auto& slot : tracked) if (slot == p) slot = nullptr;
    __real_free(p);
}

static amy_event request(uint8_t id, uint16_t patch, uint8_t voices) {
    amy_event e = amy_default_event();
    e.synth = id; e.patch_number = patch; e.num_voices = voices;
    e.time = amy_sysclock();
    return e;
}
static void load(uint8_t id, uint16_t patch, uint8_t voices) {
    auto e = request(id, patch, voices);
    assert(patches_load_patch_checked(&e) == AMY_PATCH_RESOURCES_ASSIGNED);
    amy_execute_deltas();
}
static void complete(uint8_t id, int count, int cost) {
    uint16_t voices[MAX_VOICES_PER_INSTRUMENT];
    assert(instrument_get_num_voices(id, voices) == count);
    assert(instrument_get_oscs_per_voice(id) == cost);
    for (int v = 0; v < count; ++v) {
        const auto base = voice_to_base_osc[voices[v]];
        assert(AMY_IS_SET(base) && base + cost <= AMY_OSCS);
        int assigned = 0;
        for (int o = 0; o < AMY_OSCS; ++o) {
            if (osc_to_voice[o] == voices[v]) {
                ++assigned;
                assert(o >= base && o < base + cost);
            }
        }
        assert(assigned == cost);
    }
}
struct Snapshot {
    std::vector<uint16_t> osc_map, voice_map;
    std::vector<synthinfo> oscs;
    std::vector<synthinfo*> pointers;
    std::vector<float> voice_levels;
    std::array<int, 3> patches, counts;
    std::array<float, 3> levels;
    delta* queue;
    uint16_t qsize;
    int free_deltas;
    Snapshot() : osc_map(osc_to_voice, osc_to_voice + AMY_OSCS),
                 voice_map(voice_to_base_osc, voice_to_base_osc + amy_global.config.max_voices),
                 oscs(AMY_OSCS), pointers(synth, synth + AMY_OSCS),
                 voice_levels(amy_global.config.max_voices),
                 queue(amy_global.delta_queue), qsize(amy_global.delta_qsize), free_deltas(delta_num_free()) {
        for (int o = 0; o < AMY_OSCS; ++o)
            if (synth[o]) std::memcpy(&oscs[o], synth[o], sizeof(synthinfo));
        for (size_t v = 0; v < voice_levels.size(); ++v)
            voice_levels[v] = instrument_level_for_voice(static_cast<uint16_t>(v));
        for (int i = 0; i < 3; ++i) {
            const int id = i == 2 ? 10 : i + 1;
            patches[i] = instrument_number_exists(id, nullptr) ? instrument_get_patch_number(id) : -1;
            counts[i] = instrument_get_num_voices(id, nullptr);
            levels[i] = instrument_get_level(id);
        }
    }
    void unchanged() const {
        assert(std::memcmp(osc_map.data(), osc_to_voice, osc_map.size()*sizeof(uint16_t)) == 0);
        assert(std::memcmp(voice_map.data(), voice_to_base_osc, voice_map.size()*sizeof(uint16_t)) == 0);
        for (size_t v = 0; v < voice_levels.size(); ++v)
            assert(voice_levels[v] == instrument_level_for_voice(static_cast<uint16_t>(v)));
        for (int o = 0; o < AMY_OSCS; ++o) {
            assert(pointers[o] == synth[o]);
            if (synth[o]) assert(std::memcmp(&oscs[o], synth[o], sizeof(synthinfo)) == 0);
        }
        for (int i = 0; i < 3; ++i) {
            const int id = i == 2 ? 10 : i + 1;
            assert(patches[i] == (instrument_number_exists(id, nullptr) ? instrument_get_patch_number(id) : -1));
            assert(counts[i] == instrument_get_num_voices(id, nullptr));
            assert(levels[i] == instrument_get_level(id));
        }
        assert(queue == amy_global.delta_queue && qsize == amy_global.delta_qsize);
        assert(free_deltas == delta_num_free());
    }
};

static void reject(amy_event e, amy_patch_result expected) {
    const Snapshot before;
    assert(patches_load_patch_checked(&e) == expected);
    before.unchanged();
}
static void allocation_failures(uint8_t id) {
    // New B and replacement of either A or B. Both FIFO positions exercised.
    for (int position = 1; position <= 3; ++position) {
        const Snapshot before;
        auto e = request(id, 135, 4);
        calls = 0; fail_at = position; armed = true;
        assert(patches_load_patch_checked(&e) == AMY_PATCH_NO_INSTRUMENT_MEMORY);
        for (void* p : tracked) assert(p == nullptr);
        armed = false;
        assert(calls >= position);
        before.unchanged();
    }
}
static void note(uint8_t id, float velocity, uint32_t time = 0) {
    auto e = amy_default_event(); e.synth = id; e.midi_note = 60; e.velocity = velocity;
    e.time = time ? time : amy_sysclock(); amy_add_event(&e);
}
int main() {
    auto config = amy_default_config();
    config.max_oscs = 120; config.features.default_synths = 0;
    config.max_voices = 120; // Test fixture: one-osc voices for fragmentation.
    config.features.startup_bleep = 0; config.platform.multicore = 0; config.platform.multithread = 0;
    amy_start(config);
    load(10, 258, 1); load(1, 128, 4);
    complete(1, 4, 8); complete(10, 1, 32);
    allocation_failures(2); // B absent must stay absent.
    load(2, 135, 4); complete(2, 4, 8);
    instrument_set_level(1, .37f); instrument_set_level(2, .61f);
    note(1, .7f); note(2, .8f); amy_execute_deltas();
    note(1, .6f, amy_sysclock() + 10000); // Future A delta must survive failed B.
    allocation_failures(2); allocation_failures(1);
    assert(patches_load_patch_checked(nullptr) == AMY_PATCH_INVALID_REQUEST);
    for (uint8_t id : {1, 2}) {
        reject(request(id, 300, 8), AMY_PATCH_UNKNOWN_PATCH);
        reject(request(id, 65534, 4), AMY_PATCH_UNKNOWN_PATCH);
        reject(request(id, 128, MAX_VOICES_PER_INSTRUMENT + 1), AMY_PATCH_INVALID_REQUEST);
        reject(request(id, 135, 8), AMY_PATCH_NO_OSCS);
        auto e = request(id, 0, 4); e.oscs_per_voice = 0;
        reject(e, AMY_PATCH_INVALID_COST);
        e.oscs_per_voice = 121; reject(e, AMY_PATCH_INVALID_COST);
        e.oscs_per_voice = 8; e.bus = AMY_NUM_BUSES; reject(e, AMY_PATCH_INVALID_REQUEST);
    }
    {
        // Combined load + Note On must not leak its remainder into old B.
        auto e = request(2, 300, 8); e.velocity = .99f; e.midi_note = 72;
        e.bus = 1; e.synth_level = .02f;
        const Snapshot before;
        amy_add_event(&e); before.unchanged();
    }
    // Detached preparation/cancellation must not change active per-voice levels.
    uint16_t voices[MAX_VOICES_PER_INSTRUMENT];
    assert(instrument_get_num_voices(1, voices) == 4);
    {
        const Snapshot before;
        auto* prepared = instrument_prepare(1, 4, voices, 128, 8, 0, 0);
        assert(prepared); before.unchanged();
        instrument_discard(prepared); before.unchanged();
    }
    // Sustain/FIFO state still belongs to the old container after heap failure.
    uint16_t released[MAX_VOICES_PER_INSTRUMENT];
    assert(instrument_sustain(1, true, released) == 0);
    allocation_failures(1);
    bool stolen = false;
    assert(instrument_voice_for_note_event(1, 60, true, &stolen) == _INSTRUMENT_NO_VOICE);
    assert(instrument_sustain(1, false, released) == 1);
    amy_deltas_reset(); // Only global emergency semantics are supported here.
    note(2, 0); amy_execute_deltas();
    // Alternate resources at fixed budget and exercise explicit Single/Dual teardown.
    for (int i = 0; i < 100; ++i) {
        const uint8_t id = i % 2 + 1;
        load(id, i % 4 < 2 ? 128 : 135, 4);
        complete(1, 4, 8); complete(2, 4, 8); complete(10, 1, 32);
        if (i % 10 == 0) {
            auto remove = request(2, 0, 0);
            assert(patches_load_patch_checked(&remove) == AMY_PATCH_RESOURCES_ASSIGNED);
            amy_execute_deltas(); assert(!instrument_number_exists(2, nullptr));
            load(2, 135, 4);
        }
        amy_simple_fill_buffer();
    }
    // Induce real map fragmentation without editing maps: consume every osc as
    // one-osc voices, then release even positions and regroup each container.
    // Each survivor is a valid one-osc voice.
    instruments_reset(); patches_reset(); amy_deltas_reset();
    std::array<uint16_t, 120> all{};
    int used = 0;
    for (uint8_t id = 1; id <= 4; ++id) {
        auto e = request(id, 0, id == 4 ? 24 : 32); e.oscs_per_voice = 1;
        assert(patches_load_patch_checked(&e) == AMY_PATCH_RESOURCES_ASSIGNED);
        used += instrument_get_num_voices(id, all.data() + used);
    }
    assert(used == 120); amy_execute_deltas();
    reject(request(5, 128, 1), AMY_PATCH_NO_VOICES);
    // Release via the real helper and rebuild the survivor containers.
    extern void release_for_test(); // Defined below with C linkage helper.
    release_for_test();
    int free_count = 0, run = 0, largest = 0;
    for (int o = 0; o < AMY_OSCS; ++o) {
        if (AMY_IS_UNSET(osc_to_voice[o])) { ++free_count; if (++run > largest) largest = run; }
        else run = 0;
    }
    assert(free_count == 60 && largest == 1);
    reject(request(5, 128, 1), AMY_PATCH_NO_OSCS);
    // Explicitly reproduce why RESOURCES_ASSIGNED must never be a Ready ACK:
    // osc-state allocation can fail later, when a configuration delta runs.
    auto raw = request(5, 0, 1); raw.oscs_per_voice = 1;
    assert(patches_load_patch_checked(&raw) == AMY_PATCH_RESOURCES_ASSIGNED);
    uint16_t raw_voice[MAX_VOICES_PER_INSTRUMENT];
    assert(instrument_get_num_voices(5, raw_voice) == 1);
    const auto base = voice_to_base_osc[raw_voice[0]];
    free_osc(base); // Discard cached, unowned DSP from the fragmentation fixture.
    delta wave{}; wave.time = amy_sysclock(); wave.osc = base;
    wave.param = WAVE; wave.data.i = SINE;
    add_delta_to_queue(&wave, &amy_global.delta_queue);
    calls = 0; fail_at = 1; armed = true;
    amy_execute_deltas();
    armed = false;
    assert(calls == 1 && synth[base] == nullptr && msynth[base] == nullptr);
    complete(5, 1, 1); // Complete maps do not imply a materialized oscillator.
    std::puts("PASS: deterministic post-admission oscillator OOM reproduced; resource result cannot certify Ready (no rendering of incomplete DSP)");
    amy_stop();
    std::puts("PASS: real AMY resource admission; instrument/FIFO faults, cancellation, active DSP/maps/levels/sustain/future deltas preserved; 100 switches; 96-oscs budget; fragmentation 60 free/largest 1");
}

extern "C" void release_voice_oscs(int32_t, uint32_t);
void release_for_test() {
    // Rebuild survivor containers using the public instrument API, keeping
    // only odd physical osc positions; release every even physical osc.
    for (int id = 1; id <= 4; ++id) {
        uint16_t original[MAX_VOICES_PER_INSTRUMENT], kept[MAX_VOICES_PER_INSTRUMENT];
        const int n = instrument_get_num_voices(id, original);
        int k = 0;
        for (int i = 0; i < n; ++i) {
            if (voice_to_base_osc[original[i]] % 2 == 0) release_voice_oscs(original[i], amy_sysclock());
            else kept[k++] = original[i];
        }
        assert(k > 0);
        instrument_add_new(id, k, kept, UINT16_MAX, 1, 0, 0);
        complete(id, k, 1);
    }
    amy_execute_deltas();
}
