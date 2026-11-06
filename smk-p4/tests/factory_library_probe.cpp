// Full ROM probe, using eight reusable AMY instruments. Host timings only.
#include "amy_runtime.h"
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
extern "C" {
#include "amy.h"
size_t probe_bytes(); uint32_t probe_live(); uint32_t probe_calls();
void probe_realtime(bool);
}
#ifdef FACTORY_AUDIO_OBSERVER
static unsigned pre_peak=0,soft_samples=0,hard_samples=0;
extern "C" void factory_sample_observe(int32_t sample){
    const uint32_t a=sample<0?uint32_t(-int64_t(sample)):uint32_t(sample);
    if(a>pre_peak)pre_peak=a;
    if(a>=29491)++soft_samples;
    if(a>=34405)++hard_samples; // See pinned clipping_lookup_table.h FIRST_HARDCLIP.
}
#endif
using Clock=std::chrono::steady_clock;
static long long us(Clock::time_point t){return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-t).count();}
static void event(unsigned id,unsigned note,unsigned vel){auto e=amy_default_event();e.synth=id;e.midi_note=note;e.velocity=vel/127.f;amy_add_event(&e);}
static void load(unsigned patch,unsigned cost){
    all_notes_off();amy_deltas_reset();
    for(unsigned i=1;i<=8;++i){auto e=amy_default_event();e.synth=i;e.num_voices=0;e.time=0;assert(patches_load_patch_checked(&e)==AMY_PATCH_RESOURCES_ASSIGNED);}
    amy_execute_deltas();
    auto neutral=amy_default_event();neutral.bus=0;neutral.eq_l=neutral.eq_m=neutral.eq_h=0;amy_add_event(&neutral);
    for(unsigned i=1;i<=8;++i){auto e=amy_default_event();e.synth=i;e.num_voices=1;e.patch_number=patch;e.bus=0;e.time=0;e.synth_delay_ms=0;assert(patches_load_patch_checked(&e)==AMY_PATCH_RESOURCES_ASSIGNED);}
    amy_execute_deltas();amy_simple_fill_buffer();
    bool seen[64]{};
    for(unsigned i=1;i<=8;++i){uint16_t v;assert(instrument_get_num_voices(i,&v)==1 && v<8);assert(instrument_get_patch_number(i)==int(patch));assert(instrument_get_oscs_per_voice(i)==int(cost));
        unsigned b=voice_to_base_osc[v],owned=0;assert(b+cost<=64);
        for(unsigned o=0;o<64;++o)if(osc_to_voice[o]==v){assert(!seen[o] && o>=b && o<b+cost && synth[o]);seen[o]=true;++owned;}
        assert(owned==cost);
        if(patch>=128){assert(synth[b]->wave==ALGO);for(unsigned op=0;op<6;++op){auto o=synth[b]->algo_source[op];assert(o>=b && o<b+cost && osc_to_voice[o]==v && synth[o]);}}
    }
    assert(!amy_get_oom_count());
}
int main(int argc,char** argv){
    setvbuf(stdout,nullptr,_IONBF,0);const auto baseline=probe_bytes();const auto live=probe_live();
    auto c=amy_default_config();c.features.default_synths=c.features.startup_bleep=0;
    c.features.reverb=c.features.chorus=c.features.echo=c.features.partials=c.features.custom=0;
    c.platform.multicore=c.platform.multithread=0;c.ks_oscs=0;c.max_oscs=64;c.max_voices=8;c.max_synths=9;c.max_memory_patches=c.max_sequencer_tags=1;
    amy_start(c);load(128,8);size_t stable[256]{};
    for(unsigned cycle=
#ifdef FACTORY_STRESS_ONLY
1
#else
0
#endif
;cycle<
#if defined(FACTORY_AUDIO_OBSERVER) || defined(FACTORY_TIMING_ONLY)
1
#else
3
#endif
;++cycle)for(unsigned index=0;index<256;++index){
        unsigned patch=(index/2)+(index%2?128:0);
#ifdef FACTORY_TIMING_ONLY
        bool selected=false;for(int a=1;a<argc;++a)if(unsigned(std::atoi(argv[a]))==patch)selected=true;
        if(!selected)continue;
#endif
        const auto& meta=smk::p4::library::kFactory[patch];
        auto t=Clock::now();load(patch,meta.oscillators);auto load_us=us(t);const auto bytes=probe_bytes();
        #ifdef FACTORY_STRESS_ONLY
        if(cycle==1)stable[patch]=bytes;else assert(stable[patch]==bytes);
#else
        if(cycle)assert(stable[patch]==bytes);else stable[patch]=bytes;
#endif
        if(cycle){for(unsigned i=0;i<8;++i)event(i+1,48+3*i,100);amy_simple_fill_buffer();all_notes_off();amy_deltas_reset();continue;}
        for(unsigned notes:{1u,4u,8u}){
            #ifdef FACTORY_AUDIO_OBSERVER
            pre_peak=soft_samples=hard_samples=0;
#endif
            unsigned peak=0,clipped=0,release_peak=0;double energy=0;long long total=0,maximum=0;unsigned blocks=0;
            for(unsigned region:{36u,48u,60u,72u})for(unsigned vel:{48u,100u,127u}){
                all_notes_off();amy_deltas_reset();
                for(unsigned i=0;i<notes;++i)event(i+1,region+3*i,vel);
                probe_realtime(true);
                for(unsigned block=0;block<384;++block){auto start=Clock::now();const auto pcm=amy_simple_fill_buffer();const auto elapsed=us(start);total+=elapsed;if(elapsed>maximum)maximum=elapsed;++blocks;
                    for(unsigned j=0;j<512;++j){unsigned a=std::abs(int(pcm[j]));if(a>peak)peak=a;if(a>=32767)++clipped;energy+=double(pcm[j])*pcm[j];}}
                probe_realtime(false);
                for(unsigned i=0;i<notes;++i)event(i+1,region+3*i,0);
                // 80 seconds accommodates long factory releases; no realtime sleeping.
                probe_realtime(true);
                #if defined(FACTORY_AUDIO_OBSERVER) || defined(FACTORY_TIMING_ONLY)
                const unsigned release_blocks=0;
#else
                const unsigned release_blocks=(region==72 && vel==127)?15008:256;
#endif
                for(unsigned block=0;block<release_blocks;++block){const auto pcm=amy_simple_fill_buffer();if(block>=15000)for(unsigned j=0;j<512;++j){unsigned a=std::abs(int(pcm[j]));if(a>release_peak)release_peak=a;}}
                probe_realtime(false);
            }
            all_notes_off();amy_deltas_reset();const auto pcm=amy_simple_fill_buffer();for(unsigned j=0;j<512;++j)assert(!pcm[j]);
            assert(!amy_get_oom_count() && probe_bytes()==bytes);
#ifdef FACTORY_AUDIO_OBSERVER
            std::printf("OBSERVER id=%u notes=%u pre_peak=%u soft=%u hard=%u\n",patch,notes,pre_peak,soft_samples,hard_samples);
#endif
            std::printf("FACTORY id=%u notes=%u peak=%u rms=%.6f clipped=%u release_peak=%u avg_us=%lld max_us=%lld load_us=%lld heap=%zu calls=%u\n",patch,notes,peak,std::sqrt(energy/(blocks*512.0)),clipped,release_peak,total/blocks,maximum,load_us,bytes-baseline,probe_calls());
        }
    }
    amy_stop();assert(probe_bytes()==baseline && probe_live()==live);
    #ifdef FACTORY_TIMING_ONLY
    std::puts("PASS FACTORY timing selected IDs from arguments; balanced_teardown");
#elif defined(FACTORY_AUDIO_OBSERVER)
    std::puts("PASS FACTORY observer structural_maps=256 selections=256 balanced_teardown");
#elif defined(FACTORY_STRESS_ONLY)
    std::puts("PASS FACTORY stress structural_maps=256 swaps=512 no_cumulative_growth balanced_teardown");
#else
    std::puts("PASS FACTORY structural_maps=256 swaps=768 no_cumulative_growth balanced_teardown render_allocation_guard panic");
#endif
}
