// Isolated real-AMY experiment. Host timings/heap are not ESP32-P4 evidence.
#include "amy_runtime.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
extern "C" {
#include "amy.h"
size_t probe_bytes();
uint32_t probe_calls();
uint32_t probe_live();
void probe_realtime(bool);
}
using Clock=std::chrono::steady_clock;
static long long micros(Clock::time_point t) {
    return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-t).count();
}
static void start(unsigned instruments) {
    auto c=amy_default_config();
    c.features.default_synths=c.features.startup_bleep=0;
    c.features.reverb=c.features.chorus=c.features.echo=0;
    c.features.partials=c.features.custom=0;
    c.platform.multicore=c.platform.multithread=0;
    c.ks_oscs=0; c.max_oscs=64; c.max_voices=8;
    c.max_synths=instruments+1; c.max_memory_patches=c.max_sequencer_tags=1;
    amy_start(c);
}
static amy_patch_result load(unsigned id,unsigned patch,unsigned voices) {
    auto e=amy_default_event();e.synth=id;e.patch_number=patch;
    e.num_voices=voices;e.bus=0;e.synth_delay_ms=0;e.time=0;
    return patches_load_patch_checked(&e);
}
static void verify(unsigned instruments,unsigned patch,unsigned cost) {
    bool seen[64]{}; unsigned total=0;
    for(unsigned id=1;id<=instruments;++id) {
        uint16_t voices[MAX_VOICES_PER_INSTRUMENT];
        const int count=instrument_get_num_voices(id,voices);
        assert(count==int(8/instruments));
        assert(instrument_get_patch_number(id)==int(patch));
        assert(instrument_get_oscs_per_voice(id)==int(cost));
        for(int v=0;v<count;++v) {
            unsigned owned=0;const unsigned base=voice_to_base_osc[voices[v]];
            assert(base+cost<=64);
            for(unsigned o=0;o<64;++o) if(osc_to_voice[o]==voices[v]) {
                assert(o>=base && o<base+cost && !seen[o] && synth[o]);
                seen[o]=true;++owned;++total;
            }
            assert(owned==cost);
        }
    }
    assert(total==8*cost && !amy_get_oom_count());
}
int main() {
    setvbuf(stdout,nullptr,_IONBF,0);
    const size_t baseline=probe_bytes();const auto live=probe_live();
    // Compare one eight-voice instrument and eight independent one-voice slots.
    for(unsigned instruments:{1u,8u}) {
        start(instruments);
        size_t stable[5]{};
        constexpr unsigned patches[]={128,135,138,142,4};
        long long max_load=0;
        for(unsigned cycle=0;cycle<2005;++cycle) {
            const unsigned p=cycle%5,patch=patches[p],cost=p==4?6:8;
            all_notes_off();amy_deltas_reset();
            // Whole-set quiescence avoids six/eight-osc fragmentation. No notes
            // admitted until every instrument has materialized and verified.
            for(unsigned id=1;id<=instruments;++id)
                assert(load(id,128,0)==AMY_PATCH_RESOURCES_ASSIGNED);
            amy_execute_deltas();
            const auto calls=probe_calls();const auto t=Clock::now();
            for(unsigned id=1;id<=instruments;++id)
                assert(load(id,patch,8/instruments)==AMY_PATCH_RESOURCES_ASSIGNED);
            amy_execute_deltas();
            const auto us=micros(t);if(us>max_load)max_load=us;
            verify(instruments,patch,cost);
            // Warm lazy DSP, then prohibit allocator calls in steady rendering.
            amy_simple_fill_buffer();
            for(unsigned n=0;n<8;++n) {
                auto e=amy_default_event();e.synth=instruments==1?1:n+1;
                e.midi_note=48+3*n;e.velocity=100/127.0f;amy_add_event(&e);
            }
            unsigned peak=0,clipped=0;
            probe_realtime(true);
            for(unsigned block=0;block<16;++block) {
                const auto pcm=amy_simple_fill_buffer();assert(pcm);
                for(unsigned s=0;s<512;++s) {
                    const unsigned a=std::abs(int(pcm[s]));if(a>peak)peak=a;
                    if(a>=32767)++clipped;
                }
            }
            probe_realtime(false);assert(peak);
            for(unsigned n=0;n<8;++n) {
                auto e=amy_default_event();e.synth=instruments==1?1:n+1;
                e.midi_note=48+3*n;e.velocity=0;amy_add_event(&e);
            }
            all_notes_off();amy_deltas_reset();
            const auto pcm=amy_simple_fill_buffer();
            for(unsigned s=0;s<512;++s)assert(!pcm[s]);
            if(cycle>=5 && cycle<10) stable[p]=probe_bytes();
            if(cycle>=10)assert(probe_bytes()==stable[p]);
            if(cycle<10 || cycle>=2000)
                std::printf("SWAP instruments=%u cycle=%u patch=%u heap=%zu live=%u allocations=%u load_us=%lld peak=%u clipped=%u\n",
                    instruments,cycle,patch,probe_bytes()-baseline,probe_live()-live,probe_calls()-calls,us,peak,clipped);
        }
        std::printf("STRESS instruments=%u swaps=2005 max_load_us=%lld no_cumulative_growth=PASS\n",instruments,max_load);
        amy_stop();assert(probe_bytes()==baseline && probe_live()==live);
    }
    std::puts("PASS isolated reusable instruments, FM/FM and FM/Juno, maps, PCM, Panic and balanced teardown; P4 continuity NOT RUN");
}
