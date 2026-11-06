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
extern int next_delta_block;
void probe_fail_nth(size_t,unsigned);
bool probe_failure_consumed();
}
using namespace smk::p4::amy;
static uint32_t now_ms=0;
static void midi(Runtime& r,uint8_t status,uint8_t a,uint8_t b) {
    uint8_t p[]={uint8_t(status>>4),status,a,b};
    assert(r.usbInput().transfer(p,4,now_ms));
}
static void service(Runtime& r,unsigned blocks=1,const char* scene="events") {
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
    if(blocks>=64) std::printf("HOST voices=%u patch=%u scene=%s gates=%u blocks=%u service_avg_us=%llu service_max_us=%u render_avg_us=%llu render_max_us=%u\n",kVoices,r.snapshot().patch_id,scene,r.snapshot().active_notes,blocks,static_cast<unsigned long long>(total/blocks),maximum,static_cast<unsigned long long>(render_total/blocks),render_max);
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
    // Aggregate demand must not be mistaken for a single allocation. This
    // historical heap shape has more free total than its largest region.
    assert(Runtime::hasBootMemory(411199,245760));
    assert(!Runtime::hasBootMemory(Runtime::bootFreeInternalBytes()-1,196608));
    assert(!Runtime::hasBootMemory(411199,196607));
    Runtime r;
    const auto live=probe_live(); const auto bytes=probe_bytes();
    if(kVoices>4) {
        // First matching allocation is the initial pool; fail its boot-only
        // expansion, then verify fail-closed teardown and a clean restart.
        probe_fail_nth(2048*sizeof(struct delta),2);
        assert(!r.begin() && probe_failure_consumed());
        assert(!r.snapshot().ready && r.snapshot().oom_count && !r.render());
        assert(probe_live()==live && probe_bytes()==bytes);
        probe_fail_nth(0,0);
        std::puts("PASS injected boot delta-reserve OOM: no ready engine, no leak, retry follows");
    }
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
    assert(delta_num_free()>=static_cast<int>(kDeltaReserve));
    std::printf("MEM voices=%u oscs=%u runtime_size=%zu live_bytes=%zu peak_bytes=%zu live_allocs=%u delta_blocks=%d delta_size=%zu\n",kVoices,kOscillators,sizeof(r),probe_bytes()-bytes,probe_peak_bytes(),probe_live()-live,next_delta_block,sizeof(struct delta));
    if(kPatchCount>2) assert(probe_peak_bytes()-bytes<=kLibraryEngineHeapBudgetBytes);
    probe_realtime(true);
#ifdef SMKP4_SOUND_LIBRARY
    // Captured workload needed at least57 slots. Preserve every command, but
    // never expand the per-block apply budget or allocate during service.
    for(unsigned n=0;n<57;++n) midi(r,0xb0,1,n);
    assert(r.usbInput().commands.depth()==57 && !r.usbInput().dropped);
    service(r);assert(r.usbInput().commands.depth()==25);
    service(r);assert(!r.usbInput().commands.depth() && !r.usbInput().dropped);
    // Full stale generations are purged in one bounded scan, not replayed.
    for(unsigned n=0;n<kUsbCommandCapacity;++n) midi(r,0x90,48+n%12,100);
    r.usbInput().disconnect();service(r);
    assert(!r.usbInput().commands.depth() && !r.snapshot().active_notes);
#endif
    service(r,128,"idle"); // idle
    for(unsigned bank=0;bank<kPatchCount;++bank) {
        assert(r.commands().push({CommandType::Patch,kPatches[bank].id,0,0,0,now_ms})); service(r);
        midi(r,0x90,60,100); service(r,128,"one"); assert(r.snapshot().active_notes==1);
        panic(r);
        for(unsigned n=0;n<kVoices/2;++n) midi(r,0x90,60+n,80);
        service(r,128,"half"); assert(r.snapshot().active_notes==kVoices/2);
        panic(r);
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
        service(r,128,"full");
        const auto rejected=r.snapshot().polyphony_rejected;
        midi(r,0x90,90,100); service(r);
        assert(r.snapshot().active_notes==kVoices && r.snapshot().polyphony_rejected==rejected+1);
        midi(r,0x80,90,0); service(r); assert(r.snapshot().active_notes==kVoices);
        // Reverse Off order; tails must remain, then full immediate re-admission.
        for(unsigned n=kVoices;n>0;--n) midi(r,0x80|((n-1)%2),59+n,0);
        service(r); assert(!r.snapshot().active_notes);
        service(r,64,"release"); chord(r); service(r,64,"new_chord");
        // Sustain occupies capacity on each channel; channel 1 cannot clear 0.
        midi(r,0xb0,64,127); midi(r,0xb1,64,127); service(r);
        for(unsigned n=0;n<kVoices;++n) midi(r,0x80|(n%2),60+n,0);
        service(r); assert(r.snapshot().active_notes==kVoices);
        service(r,128,"full_sustain");
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
        midi(r,0xe0,127,127); midi(r,0xb1,1,127); service(r,128,"mixed_bend_cc1");
        r.usbInput().disconnect(); service(r); assert(r.snapshot().active_notes==1);
        midi(r,0x90,60,100); service(r); assert(r.snapshot().active_notes==2);
        r.commands().release(60,token); service(r); assert(r.snapshot().active_notes==1);
        panic(r);
        // Both producer queues full: one Touch slot retriggered with fresh
        // tokens, all remaining USB voices on the same modulated channel.
        token=r.commands().nextToken();
        assert(r.commands().push({CommandType::NoteOn,60,127,0,token,now_ms}));
        for(unsigned n=1;n<kVoices;++n) midi(r,0x90,60+n,127);
        service(r); assert(r.snapshot().active_notes==kVoices);
        for(unsigned n=0;n<kCommandCapacity;++n) {
            token=r.commands().nextToken();
            assert(r.commands().push({CommandType::NoteOn,60,127,0,token,now_ms}));
            midi(r,0xb0,1,127);
        }
        service(r,128,"dual_queue_full"); assert(r.snapshot().active_notes==kVoices);
        panic(r);
        // Repeated full chords while release tails still have amplitude.
        for(unsigned repeat=0;repeat<16;++repeat) {
            chord(r);
            for(unsigned n=0;n<kVoices;++n) midi(r,0x80|(n%2),60+n,0);
            service(r); assert(!r.snapshot().active_notes);
        }
        service(r,64,"rapid_release"); panic(r);
        // Touch lease and overflow cancellation remain bounded at full capacity.
        token=r.commands().nextToken();
        assert(r.commands().push({CommandType::NoteOn,60,100,0,token,now_ms})); service(r);
        now_ms+=kLeaseMs; service(r); assert(!r.snapshot().active_notes);
        assert(r.commands().push({CommandType::Master,0,127,0,0,now_ms}));
        for(unsigned n=0;n<kVoices;++n) midi(r,0x90,60+n,127);
        service(r); assert(r.snapshot().active_notes==kVoices && r.snapshot().master==127);
        for(unsigned n=0;n<kCommandCapacity;++n) midi(r,0xb0,1,n);
        service(r,128,"cc1_full_channel"); assert(r.snapshot().active_notes==kVoices);
        for(unsigned n=0;n<kUsbCommandCapacity;++n) midi(r,0xb0,1,n%128);
        midi(r,0x80,60,0); service(r); assert(!r.snapshot().active_notes);
        chord(r);
        assert(r.commands().push({CommandType::Patch,kPatches[(bank+1)%kPatchCount].id,0,0,0,now_ms})); service(r);
        assert(!r.snapshot().active_notes); panic(r);
        assert(r.commands().push({CommandType::Master,0,64,0,0,now_ms})); service(r);
    }
    probe_realtime(false); r.end();
    assert(probe_live()==live && probe_bytes()==bytes);
    std::printf("PASS capacity=%u rejection/release/sustain/ownership/disconnect/reconnect/lease/overflow/patch/Panic; no realtime heap\n",kVoices);
}
