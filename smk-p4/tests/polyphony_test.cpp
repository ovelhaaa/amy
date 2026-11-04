// Real engine admission/ownership and worst-event load; host timing is not P4 timing.
#include "amy_runtime.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cmath>
extern "C" {
#include "amy.h"
void probe_realtime(bool);
uint32_t probe_live();
size_t probe_bytes();
size_t probe_peak_bytes();
}
using namespace smk::p4::amy;
static uint32_t now_ms=0;
static void midi(Runtime& r,uint8_t status,uint8_t a,uint8_t b) {
    uint8_t p[]={uint8_t(status>>4),status,a,b};
    assert(r.usbInput().transfer(p,4,now_ms));
}
static void service(Runtime& r,unsigned blocks=1) {
    uint64_t total=0,render_total=0; unsigned maximum=0,render_max=0;
    for(unsigned b=0;b<blocks;++b) {
        const auto start=std::chrono::steady_clock::now();
        r.apply(now_ms);
        const auto render_start=std::chrono::steady_clock::now();
        assert(r.render());
        const auto render_us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-render_start).count();
        render_total+=render_us; if(render_us>render_max) render_max=static_cast<unsigned>(render_us);
        const auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
        total+=us; if(us>maximum) maximum=static_cast<unsigned>(us);
        now_ms+=5;
    }
    assert(!r.snapshot().oom_count && !r.snapshot().clipped);
    if(blocks>=64) std::printf("HOST voices=%u gates=%u blocks=%u service_avg_us=%llu service_max_us=%u render_avg_us=%llu render_max_us=%u\n",kVoices,r.snapshot().active_notes,blocks,static_cast<unsigned long long>(total/blocks),maximum,static_cast<unsigned long long>(render_total/blocks),render_max);
}
static void panic(Runtime& r) {
    r.commands().panic(); service(r);
    assert(!r.snapshot().active_notes);
    const auto* pcm=r.render(); assert(pcm);
    for(unsigned i=0;i<kBlockFrames*2;++i) assert(pcm[i]==0);
}
static void chord(Runtime& r) {
    for(unsigned n=0;n<kVoices;++n) midi(r,0x90|(n%2),60+n,40+n*10);
    service(r); assert(r.snapshot().active_notes==kVoices);
}
int main() {
    Runtime r;
    const auto live=probe_live(); const auto bytes=probe_bytes();
    assert(r.begin());
    bool boot_voices[kAmyVoices]{};
    unsigned boot_oscs=0;
    for(unsigned i=1;i<kSynthCount;++i) {
        uint16_t voice; assert(instrument_get_num_voices(i,&voice)==1);
        assert(voice<kAmyVoices && !boot_voices[voice]); boot_voices[voice]=true;
        unsigned count=0;
        for(unsigned o=0;o<kOscillators;++o) if(osc_to_voice[o]==voice) {
            assert(synth[o] && msynth[o]); ++count;
        }
        assert(count==kOscillatorsPerVoice); boot_oscs+=count;
    }
    assert(boot_oscs==kOscillators);
    std::printf("MEM voices=%u oscs=%u runtime_size=%zu live_bytes=%zu peak_bytes=%zu live_allocs=%u\n",kVoices,kOscillators,sizeof(r),probe_bytes()-bytes,probe_peak_bytes(),probe_live()-live);
    probe_realtime(true);
    service(r,128); // idle
    for(unsigned bank=0;bank<kPatchCount;++bank) {
        assert(r.commands().push({CommandType::Patch,kPatches[bank].id,0,0,0,now_ms})); service(r);
        // Full admission must produce independent real AMY instruments/notes.
        chord(r);
        bool seen[kOscillators]{};
        for(unsigned n=0;n<kVoices;++n) {
            uint16_t v; assert(instrument_get_num_voices(bank*kVoices+n+1,&v)==1);
            const unsigned base=voice_to_base_osc[v];
            assert(base<kOscillators && !seen[base]); seen[base]=true;
            assert(std::fabs(synth[base]->midi_note-(60.0f+n))<0.001f);
            assert(synth[base]->velocity>0);
        }
        service(r,128);
        const auto rejected=r.snapshot().polyphony_rejected;
        midi(r,0x90,90,100); service(r);
        assert(r.snapshot().active_notes==kVoices && r.snapshot().polyphony_rejected==rejected+1);
        midi(r,0x80,90,0); service(r); assert(r.snapshot().active_notes==kVoices);
        // Reverse Off order; tails must remain, then full immediate re-admission.
        for(unsigned n=kVoices;n>0;--n) midi(r,0x80|((n-1)%2),59+n,0);
        service(r); assert(!r.snapshot().active_notes);
        service(r,64); chord(r); service(r,64);
        // Sustain occupies capacity on each channel; channel 1 cannot clear 0.
        midi(r,0xb0,64,127); midi(r,0xb1,64,127); service(r);
        for(unsigned n=0;n<kVoices;++n) midi(r,0x80|(n%2),60+n,0);
        service(r); assert(r.snapshot().active_notes==kVoices);
        midi(r,0x90,60,100); service(r); assert(r.snapshot().active_notes==kVoices);
        midi(r,0x90,60,0); service(r); // velocity zero release, still sustained
        midi(r,0xb1,64,0); service(r); assert(r.snapshot().active_notes==(kVoices+1)/2);
        midi(r,0xb0,64,0); service(r); assert(!r.snapshot().active_notes);
        panic(r);
        // Mixed source, identical pitch; USB cancellation leaves Touch alive.
        auto token=r.commands().nextToken();
        assert(r.commands().push({CommandType::NoteOn,60,100,0,token,now_ms})); service(r);
        for(unsigned n=0;n<kVoices-1;++n) midi(r,0x90|(n%2),60+n,80);
        service(r); assert(r.snapshot().active_notes==kVoices);
        midi(r,0xe0,127,127); midi(r,0xb1,1,127); service(r,128);
        r.usbInput().disconnect(); service(r); assert(r.snapshot().active_notes==1);
        midi(r,0x90,60,100); service(r); assert(r.snapshot().active_notes==2);
        r.commands().release(60,token); service(r); assert(r.snapshot().active_notes==1);
        panic(r);
        // Touch lease and overflow cancellation remain bounded at full capacity.
        token=r.commands().nextToken();
        assert(r.commands().push({CommandType::NoteOn,60,100,0,token,now_ms})); service(r);
        now_ms+=kLeaseMs; service(r); assert(!r.snapshot().active_notes);
        chord(r);
        for(unsigned n=0;n<kCommandCapacity;++n) midi(r,0xb0,1,n);
        midi(r,0x80,60,0); service(r); assert(!r.snapshot().active_notes);
        chord(r);
        assert(r.commands().push({CommandType::Patch,kPatches[1-bank].id,0,0,0,now_ms})); service(r);
        assert(!r.snapshot().active_notes); panic(r);
    }
    probe_realtime(false); r.end();
    assert(probe_live()==live && probe_bytes()==bytes);
    std::printf("PASS capacity=%u rejection/release/sustain/ownership/disconnect/reconnect/lease/overflow/patch/Panic; no realtime heap\n",kVoices);
}
