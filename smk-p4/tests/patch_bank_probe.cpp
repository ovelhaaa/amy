// Actual AMY bank cost and dry-render probe. Host metrics do not qualify P4.
#include "amy_runtime.h"
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
extern "C" {
#include "amy.h"
uint32_t probe_calls();
size_t probe_bytes();
uint32_t probe_live();
void probe_realtime(bool);
}
using Clock=std::chrono::steady_clock;
static const unsigned candidates[]={128,135,138,142,4,31,32,8,0,1,12,14};
static long long elapsed(Clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start).count();
}
static void start(unsigned banks,unsigned oscs) {
    auto c=amy_default_config();
    c.features.default_synths=c.features.startup_bleep=0;
    c.features.reverb=c.features.chorus=c.features.echo=0;
    c.features.partials=c.features.custom=0;
    c.platform.multicore=c.platform.multithread=0;
    c.ks_oscs=0; c.max_oscs=oscs; c.max_voices=banks*8;
    c.max_synths=banks*8+1; c.max_memory_patches=c.max_sequencer_tags=1;
    amy_start(c);
}
static void load(unsigned instrument,unsigned patch) {
    auto e=amy_default_event(); e.synth=instrument; e.patch_number=patch;
    e.num_voices=1; e.bus=0; e.synth_delay_ms=0;
    amy_add_event(&e);
    assert(instrument_get_patch_number(instrument)==int(patch));
}
int main() {
    const auto base=probe_bytes(); const auto live=probe_live();
    for(unsigned banks: {2u,4u,6u}) {
        start(banks,banks==6?352:banks*64);
        for(unsigned b=0;b<banks;++b) {
            const auto bytes=probe_bytes(); const auto calls=probe_calls(); const auto t=Clock::now();
            for(unsigned v=0;v<8;++v) load(b*8+v+1,candidates[b==5?7:b]);
            amy_simple_fill_buffer(); amy_simple_fill_buffer();
            std::printf("BANK architecture=A banks=%u patch=%u load_us=%lld heap_delta=%zu alloc_calls=%u total_bytes=%zu\n",banks,candidates[b==5?7:b],elapsed(t),probe_bytes()-bytes,probe_calls()-calls,probe_bytes()-base);
        }
        const auto calls=probe_calls();const auto t=Clock::now();
        for(unsigned v=0;v<8;++v) load(v+1,142);
        amy_simple_fill_buffer();
        std::printf("REPLACE architecture=B banks=%u load_us=%lld alloc_calls=%u\n",banks,elapsed(t),probe_calls()-calls);
        assert(probe_calls()>calls); // Not allocation-free, even with equal oscillator cost.
        amy_stop(); assert(probe_bytes()==base && probe_live()==live);
    }
    for(unsigned patch:candidates) {
        start(1,64);
        for(unsigned v=0;v<8;++v) load(v+1,patch);
        amy_simple_fill_buffer();amy_simple_fill_buffer();
        const auto bytes=probe_bytes();
        unsigned actual=instrument_get_oscs_per_voice(1);
        std::printf("CANDIDATE patch=%u oscs=%u heap=%zu\n",patch,actual,bytes-base);
        for(unsigned notes: {1u,4u,6u,8u}) {
            all_notes_off();
            for(unsigned v=0;v<notes;++v) {
                auto e=amy_default_event();e.synth=v+1;e.midi_note=48+3*v;e.velocity=100/127.0f;amy_add_event(&e);
            }
            unsigned peak=0,clipped=0;double energy=0;long long total=0,maximum=0;
            // Any lazy DSP allocations are also forbidden during rendering.
            probe_realtime(true);
            for(unsigned block=0;block<256;++block) {
                const auto t=Clock::now();const auto pcm=amy_simple_fill_buffer();const auto us=elapsed(t);
                total+=us;if(us>maximum)maximum=us;
                for(unsigned i=0;i<512;++i) {
                    unsigned a=std::abs(int(pcm[i]));if(a>peak)peak=a;
                    if(a>=32767)++clipped;energy+=double(pcm[i])*pcm[i];
                }
            }
            probe_realtime(false);
            assert(peak && !amy_get_oom_count() && probe_bytes()==bytes);
            std::printf("DRY_HOST patch=%u notes=%u peak=%u rms=%.3f clipped=%u avg_us=%lld max_us=%lld quality=%s\n",patch,notes,peak,std::sqrt(energy/(256*512)),clipped,total/256,maximum,clipped?"FAIL_CLIPPING":"NOT_QUALIFIED");
        }
        amy_stop();assert(probe_bytes()==base && probe_live()==live);
    }
    std::puts("PASS bank admission, render allocation guards and balanced teardown; audition/P4 deadlines MANUAL");
}
