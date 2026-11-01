// Real AMY allocator tests. Faults target malloc_caps call positions, not host
// heap exhaustion. Tests admission and detached capacity primitives, NOT full
// patch materialization/Ready or patch rollback.
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
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
void play_delta(struct delta*);
extern struct delta* delta_blocks[];
extern int next_delta_block;
}

static int fail_at, calls;
static bool armed;
static uint32_t bytes_requested;
static std::array<void*, 256> tracked{};
// Compile amy.c with MALLOC_CAPS_DEFINED for this executable only. Otherwise
// the host compiler can inline its malloc_caps into alloc_osc and bypass a
// linker --wrap hook in the same translation unit.
extern "C" void* malloc_caps(uint32_t size, uint32_t) {
    if (armed) bytes_requested += size;
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
    std::vector<mod_synthinfo> mods;
    std::vector<std::vector<uint8_t>> envelope_bytes;
    std::vector<delta> queued;
    global_state_t global;
    std::vector<float> voice_levels;
    std::array<int, 3> patches, counts;
    std::array<float, 3> levels;
    delta* queue;
    uint16_t qsize;
    int free_deltas;
    Snapshot() : osc_map(osc_to_voice, osc_to_voice + AMY_OSCS),
                 voice_map(voice_to_base_osc, voice_to_base_osc + amy_global.config.max_voices),
                 oscs(AMY_OSCS), pointers(synth, synth + AMY_OSCS), mods(AMY_OSCS), envelope_bytes(AMY_OSCS),
                 voice_levels(amy_global.config.max_voices),
                 queue(amy_global.delta_queue), qsize(amy_global.delta_qsize), free_deltas(delta_num_free()) {
        std::memcpy(&global, &amy_global, sizeof(global));
        for (auto* d = queue; d; d = d->next) queued.push_back(*d);
        for (int o = 0; o < AMY_OSCS; ++o) {
            if (!synth[o]) continue;
            std::memcpy(&oscs[o], synth[o], sizeof(synthinfo));
            std::memcpy(&mods[o], msynth[o], sizeof(mod_synthinfo));
            for (int set = 0; set < MAX_BREAKPOINT_SETS; ++set) {
                const size_t n = synth[o]->max_num_breakpoints[set];
                const auto* times = reinterpret_cast<const uint8_t*>(synth[o]->breakpoint_times[set]);
                const auto* values = reinterpret_cast<const uint8_t*>(synth[o]->breakpoint_values[set]);
                envelope_bytes[o].insert(envelope_bytes[o].end(), times, times + n * sizeof(uint32_t));
                envelope_bytes[o].insert(envelope_bytes[o].end(), values, values + n * sizeof(float));
            }
        }
        for (size_t v = 0; v < voice_levels.size(); ++v)
            voice_levels[v] = instrument_level_for_voice(static_cast<uint16_t>(v));
        for (int i = 0; i < 3; ++i) {
            const int id = i == 2 ? 10 : i + 1;
            patches[i] = instrument_number_exists(id, nullptr) ? instrument_get_patch_number(id) : -1;
            counts[i] = instrument_get_num_voices(id, nullptr);
            levels[i] = instrument_get_level(id);
        }
    }
    void unchanged(int except1 = -1, int except2 = -1, bool pool_grew = false) const {
        // Legacy admission reports OOM diagnostically; that counter is allowed
        // to change while the active musical/configuration state is preserved.
        global_state_t current;
        std::memcpy(&current, &amy_global, sizeof(current));
        current.oom_count = global.oom_count;
        assert(std::memcmp(&global, &current, sizeof(global)) == 0);
        assert(std::memcmp(osc_map.data(), osc_to_voice, osc_map.size()*sizeof(uint16_t)) == 0);
        assert(std::memcmp(voice_map.data(), voice_to_base_osc, voice_map.size()*sizeof(uint16_t)) == 0);
        for (size_t v = 0; v < voice_levels.size(); ++v)
            assert(voice_levels[v] == instrument_level_for_voice(static_cast<uint16_t>(v)));
        for (int o = 0; o < AMY_OSCS; ++o) {
            if (o == except1 || o == except2) continue;
            assert(pointers[o] == synth[o]);
            if (synth[o]) {
                assert(std::memcmp(&oscs[o], synth[o], sizeof(synthinfo)) == 0);
                assert(std::memcmp(&mods[o], msynth[o], sizeof(mod_synthinfo)) == 0);
                size_t offset = 0;
                for (int set = 0; set < MAX_BREAKPOINT_SETS; ++set) {
                    const size_t n = synth[o]->max_num_breakpoints[set];
                    assert(std::memcmp(envelope_bytes[o].data() + offset, synth[o]->breakpoint_times[set], n * sizeof(uint32_t)) == 0);
                    offset += n * sizeof(uint32_t);
                    assert(std::memcmp(envelope_bytes[o].data() + offset, synth[o]->breakpoint_values[set], n * sizeof(float)) == 0);
                    offset += n * sizeof(float);
                }
            }
        }
        for (int i = 0; i < 3; ++i) {
            const int id = i == 2 ? 10 : i + 1;
            assert(patches[i] == (instrument_number_exists(id, nullptr) ? instrument_get_patch_number(id) : -1));
            assert(counts[i] == instrument_get_num_voices(id, nullptr));
            assert(levels[i] == instrument_get_level(id));
        }
        assert(queue == amy_global.delta_queue && qsize == amy_global.delta_qsize);
        if (!pool_grew) assert(free_deltas == delta_num_free());
        auto* d = queue;
        for (const auto& expected : queued) {
            assert(d && d->param == expected.param && d->osc == expected.osc &&
                   d->time == expected.time && d->data.i == expected.data.i && d->next == expected.next);
            d = d->next;
        }
        assert(d == nullptr);
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

static void no_temporary_leaks() {
    for (void* p : tracked) assert(p == nullptr);
}

static void dsp_capacity_preparation() {
    uint16_t a[MAX_VOICES_PER_INSTRUMENT], b[MAX_VOICES_PER_INSTRUMENT];
    instrument_get_num_voices(1, a); instrument_get_num_voices(2, b);
    const uint16_t oa = voice_to_base_osc[a[0]], ob = voice_to_base_osc[b[0]];
    amy_osc_capacity_request req[] = {{oa, {MAX_BREAKPOINTS, MAX_BREAKPOINTS}},
                                     {ob, {MAX_BREAKPOINTS, MAX_BREAKPOINTS}}};
    amy_osc_preparation* prepared = nullptr;
    // One token plus one coallocation per oscillator. Each includes synthinfo,
    // mod_synthinfo and BOTH envelopes' time/value vectors, not separate mallocs.
    for (int failure = 1; failure <= 3; ++failure) {
        const Snapshot before;
        calls = 0; fail_at = failure; armed = true;
        assert(amy_osc_bank_prepare(req, 2, &prepared) == AMY_RESOURCE_NO_MEMORY);
        assert(prepared == nullptr && calls == failure); no_temporary_leaks();
        armed = false; before.unchanged();
    }
    {
        const Snapshot before;
        calls = 0; fail_at = 0; bytes_requested = 0; armed = true;
        assert(amy_osc_bank_prepare(req, 2, &prepared) == AMY_RESOURCE_OK);
        std::printf("MEASURE host: detached bank of 2 oscs/24+24 breakpoints requested=%u bytes including token; synthinfo=%zu mod_synthinfo=%zu\n",
                    bytes_requested, sizeof(synthinfo), sizeof(mod_synthinfo));
        before.unchanged();
        auto* same = prepared;
        assert(amy_osc_bank_prepare(req, 2, &prepared) == AMY_RESOURCE_INVALID && prepared == same);
        amy_osc_bank_discard(&prepared); amy_osc_bank_discard(&prepared);
        no_temporary_leaks(); armed = false; before.unchanged();
    }
    // If a later target has changed, preflight must not publish the first target.
    assert(amy_osc_bank_prepare(req, 2, &prepared) == AMY_RESOURCE_OK);
    uint8_t grown[] = {MAX_BREAKPOINTS, MAX_BREAKPOINTS};
    assert(ensure_osc_allocd(ob, grown));
    const Snapshot after_growth;
    assert(amy_osc_bank_commit(&prepared) == AMY_RESOURCE_STALE);
    after_growth.unchanged(); amy_osc_bank_discard(&prepared); after_growth.unchanged();
    // Commit copies current musical state/envelopes, including edits since prepare.
    auto* old = synth[oa];
    assert(amy_osc_bank_prepare(req, 2, &prepared) == AMY_RESOURCE_OK);
    synth[oa]->breakpoint_values[0][0] = .3125f;
    synth[oa]->breakpoint_times[1][0] = 12345;
    msynth[oa]->amp = .375f;
    const auto phase = synth[oa]->phase;
    const Snapshot before_commit;
    calls = 0; fail_at = 1; armed = true;
    assert(amy_osc_bank_commit(&prepared) == AMY_RESOURCE_OK);
    assert(calls == 0 && prepared == nullptr && synth[oa] != old);
    no_temporary_leaks(); armed = false;
    before_commit.unchanged(oa, ob);
    assert(synth[oa]->breakpoint_values[0][0] == .3125f);
    assert(synth[oa]->breakpoint_times[1][0] == 12345);
    assert(msynth[oa]->amp == .375f && synth[oa]->phase == phase);
    assert(synth[oa]->max_num_breakpoints[0] == MAX_BREAKPOINTS);
    for (int j = DEFAULT_NUM_BREAKPOINTS; j < MAX_BREAKPOINTS; ++j)
        assert(AMY_IS_UNSET(synth[oa]->breakpoint_times[0][j]));
    // Checked allocation also supports unallocated slots without touching maps.
    uint16_t unused = 0;
    while (unused < AMY_OSCS && AMY_IS_SET(osc_to_voice[unused])) ++unused;
    assert(unused < AMY_OSCS); free_osc(unused);
    amy_osc_capacity_request fresh{unused, {0, 0}};
    assert(amy_osc_bank_prepare(&fresh, 1, &prepared) == AMY_RESOURCE_OK);
    assert(synth[unused] == nullptr && msynth[unused] == nullptr);
    assert(amy_osc_bank_commit(&prepared) == AMY_RESOURCE_OK);
    assert(synth[unused] && msynth[unused] && synth[unused]->status == SYNTH_OFF);
    assert(synth[unused]->max_num_breakpoints[0] >= DEFAULT_NUM_BREAKPOINTS);
    // Real WAVE/BP deltas cannot allocate within the reserved capacity.
    delta bp{}; bp.osc = oa; bp.param = static_cast<params>(BP_START + 2 * (MAX_BREAKPOINTS - 1) + 1); bp.data.f = .25f;
    calls = 0; fail_at = 1; armed = true;
    play_delta(&bp);
    assert(calls == 0 && synth[oa]->breakpoint_values[0][MAX_BREAKPOINTS - 1] == .25f);
    armed = false;
    auto invalid = fresh; invalid.osc = AMY_OSCS;
    assert(amy_osc_bank_prepare(&invalid, 1, &prepared) == AMY_RESOURCE_INVALID);
    invalid = fresh; invalid.breakpoints[0] = MAX_BREAKPOINTS + 1;
    assert(amy_osc_bank_prepare(&invalid, 1, &prepared) == AMY_RESOURCE_INVALID);
    req[1] = req[0];
    assert(amy_osc_bank_prepare(req, 2, &prepared) == AMY_RESOURCE_INVALID);
    assert(amy_osc_bank_commit(&prepared) == AMY_RESOURCE_INVALID);
    assert(amy_osc_bank_prepare(nullptr, 0, &prepared) == AMY_RESOURCE_INVALID);
    // Bound is the unchanged real oscillator pool. Inject at every allocation
    // position in the largest allowed bank; never render or publish this bank.
    std::vector<amy_osc_capacity_request> whole_pool(AMY_OSCS);
    for (uint16_t o = 0; o < AMY_OSCS; ++o) whole_pool[o] = {o, {MAX_BREAKPOINTS, MAX_BREAKPOINTS}};
    for (int failure = 1; failure <= AMY_OSCS + 1; ++failure) {
        const Snapshot before;
        calls = 0; fail_at = failure; armed = true;
        assert(amy_osc_bank_prepare(whole_pool.data(), AMY_OSCS, &prepared) == AMY_RESOURCE_NO_MEMORY);
        assert(calls == failure && prepared == nullptr); no_temporary_leaks();
        armed = false; before.unchanged();
    }
    {
        const Snapshot before;
        calls = 0; fail_at = 0; bytes_requested = 0; armed = true;
        assert(amy_osc_bank_prepare(whole_pool.data(), AMY_OSCS, &prepared) == AMY_RESOURCE_OK);
        std::printf("MEASURE host: largest detached bank (120 oscs/24+24 breakpoints) requested=%u bytes including token; live engine additional\n", bytes_requested);
        amy_osc_bank_discard(&prepared); no_temporary_leaks(); armed = false; before.unchanged();
    }
    // Repeated cancellation, allocation faults and recovery over active FM/drums.
    const auto started = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        const Snapshot before;
        calls = 0; fail_at = i % 3; armed = true;
        const auto result = amy_osc_bank_prepare(req, 1, &prepared);
        assert(result == (fail_at ? AMY_RESOURCE_NO_MEMORY : AMY_RESOURCE_OK));
        amy_osc_bank_discard(&prepared); no_temporary_leaks();
        armed = false; before.unchanged();
    }
    std::printf("MEASURE host: 100 capacity prepare/fault/discard attempts plus full snapshots=%lld us (not S3 WCET)\n",
        static_cast<long long>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count()));
    std::printf("PASS: detached DSP/envelope capacity OOM, discard, stale preflight, allocation-free capacity commit and 100 recovery attempts; request=%zu bytes (not patch Ready)\n", sizeof(fresh));
}

static void delta_batch_preparation() {
    const Snapshot active;
    // Leave exactly one free node, then fail the growth needed for node two.
    std::vector<delta*> held;
    while (delta_num_free() > 1) { delta* d = nullptr; assert(delta_get_checked(nullptr, &d) == AMY_RESOURCE_OK); held.push_back(d); }
    const Snapshot before;
    delta values[3]{};
    for (int i = 0; i < 3; ++i) { values[i].osc = 0; values[i].param = WAVE; values[i].data.i = SINE; }
    calls = 0; fail_at = 1; armed = true;
    const auto oom_before = amy_get_oom_count();
    assert(add_delta_batch_to_queue_checked(values, 2, &amy_global.delta_queue) == AMY_RESOURCE_NO_MEMORY);
    assert(calls == 1 && amy_get_oom_count() == oom_before + 1); no_temporary_leaks(); armed = false;
    before.unchanged();
    // If a batch grows one pool and the next growth fails, all acquired nodes
    // return to the free-list; the first successful pool stays engine-owned.
    std::vector<delta> large_batch(2050, values[0]);
    const int blocks_before = next_delta_block;
    calls = 0; fail_at = 2; armed = true;
    assert(add_delta_batch_to_queue_checked(large_batch.data(), static_cast<uint16_t>(large_batch.size()),
                                          &amy_global.delta_queue) == AMY_RESOURCE_NO_MEMORY);
    assert(calls == 2 && next_delta_block == blocks_before + 1 && delta_num_free() == 2049);
    int cached = 0;
    for (auto& p : tracked) if (p) {
        assert(p == delta_blocks[next_delta_block - 1]);
        p = nullptr; ++cached; // ownership verified: persistent engine pool, not a temporary
    }
    assert(cached == 1); armed = false; before.unchanged(-1, -1, true);
    for (auto* d : held) delta_release(d);
    active.unchanged(-1, -1, true); // future A delta and qsize/head unchanged; pool cached
    assert(add_delta_batch_to_queue_checked(nullptr, 1, &amy_global.delta_queue) == AMY_RESOURCE_INVALID);
    assert(add_delta_batch_to_queue_checked(values, 1, nullptr) == AMY_RESOURCE_INVALID);
    assert(add_delta_batch_to_queue_checked(nullptr, 0, &amy_global.delta_queue) == AMY_RESOURCE_OK);
    // Compare actual upstream list ordering for equal times and uint32 wrap.
    values[0].time = UINT32_MAX - 3; values[1].time = 2; values[2].time = 2;
    values[1].data.i = SAW_UP; values[2].data.i = SAW_DOWN;
    delta *checked = nullptr, *legacy = nullptr;
    for (auto& d : values) add_delta_to_queue(&d, &legacy);
    assert(add_delta_batch_to_queue_checked(values, 3, &checked) == AMY_RESOURCE_OK);
    auto* left = legacy; auto* right = checked;
    for (int i = 0; i < 3; ++i) {
        assert(left && right && left->time == right->time && left->data.i == right->data.i);
        left = left->next; right = right->next;
    }
    assert(!left && !right); delta_release_list(legacy); delta_release_list(checked);
    active.unchanged(-1, -1, true);
    // Reach the real global ceiling without abort; keep active future deltas.
    held.clear();
    for (;;) {
        delta* d = nullptr;
        const auto r = delta_get_checked(nullptr, &d);
        if (r == AMY_RESOURCE_CAPACITY) { assert(d == nullptr); break; }
        assert(r == AMY_RESOURCE_OK); held.push_back(d);
    }
    const auto head = amy_global.delta_queue;
    const auto size = amy_global.delta_qsize;
    assert(add_delta_batch_to_queue_checked(values, 3, &amy_global.delta_queue) == AMY_RESOURCE_CAPACITY);
    assert(amy_global.delta_queue == head && amy_global.delta_qsize == size);
    for (auto* d : held) delta_release(d);
    assert(delta_num_free() + delta_list_len(amy_global.delta_queue) == 16 * 2048);
    // Successful global insertion accounts exactly once, without executing it.
    values[0].time = amy_sysclock() + 20000;
    assert(add_delta_batch_to_queue_checked(values, 1, &amy_global.delta_queue) == AMY_RESOURCE_OK);
    assert(amy_global.delta_qsize == size + 1);
    std::puts("PASS: checked delta batch OOM/capacity, no partial queue publication, wrap/equal-time legacy ordering, preserved future deltas; cached pool ceiling=32768");
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
    dsp_capacity_preparation();
    delta_batch_preparation();
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
