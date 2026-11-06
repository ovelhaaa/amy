#include "local_performance_backend.h"
#include "pcm_handoff.h"
#include <cassert>
#include <cstdio>
#include <thread>
#include <initializer_list>
#include <chrono>
#include <cstring>
extern "C" {
#include "amy.h"
size_t probe_bytes();uint32_t probe_live();void probe_realtime(bool);
void probe_fail_next(unsigned);void probe_on_allocation(unsigned,void (*)());
}
using namespace smk::p4;
static amy::Runtime* active=nullptr;
static int16_t captured[256*512]{};
static void saveWav(uint16_t patch) {
    char path[128];std::snprintf(path,sizeof(path),"build/unified08/patch_%u_8.wav",patch);
    auto* f=std::fopen(path,"wb");assert(f);
    auto u16=[&](uint16_t x) { std::fputc(x&255,f);std::fputc(x>>8,f); };
    auto u32=[&](uint32_t x) { u16(x&65535);u16(x>>16); };
    std::fwrite("RIFF",1,4,f);u32(36+sizeof(captured));std::fwrite("WAVEfmt ",1,8,f);
    u32(16);u16(1);u16(2);u32(48000);u32(48000*4);u16(4);u16(16);
    std::fwrite("data",1,4,f);u32(sizeof(captured));
    for(auto sample:captured)u16(static_cast<uint16_t>(sample));
    assert(!std::ferror(f));assert(std::fclose(f)==0);
}
static void cancelLoad() { active->commands().panic(); }
static void disconnectLoad() { active->usbInput().disconnect(); }
static void partialLoad() { instrument_release(4); }
static void zero(const int16_t* pcm) { assert(pcm);for(unsigned i=0;i<512;++i)assert(!pcm[i]); }
static void usb(amy::Runtime& r,uint8_t status,uint8_t a,uint8_t b=0) {
    const uint8_t p[]={static_cast<uint8_t>(status>>4),status,a,b};assert(r.usbInput().transfer(p,4,0));
}
static void select(amy::Runtime& r,uint16_t id) {
    assert(r.commands().push({amy::CommandType::Patch,id}));r.apply(0);
    assert(r.snapshot().patch_state==amy::PatchState::Requested);zero(r.render());
    r.loadPendingPatch();assert(r.snapshot().patch_state==amy::PatchState::Ready);
    assert(r.snapshot().patch_id==id && !r.snapshot().active_notes);
}
int main() {
    setvbuf(stdout,nullptr,_IONBF,0);
    static_assert(amy::kAmyVoices==8 && amy::kOscillators==64);
    const auto base=probe_bytes();const auto live=probe_live();
    amy::Runtime r;active=&r;assert(r.begin());
    std::printf("RUNTIME_HEAP_BOOT bytes=%zu live=%u sizeof_runtime=%zu delta_reserve=%u\n",probe_bytes()-base,probe_live()-live,sizeof(r),amy::kDeltaReserve);
    LocalAmyPerformanceBackend b(r.commands());
    auto publish=[&] { b.publish(r.snapshot(),false,0,0); };publish();
    assert(b.setPatch(135,0) && b.busy());assert(!b.noteOn(60,100,0));
    r.apply(0);publish();assert(b.busy() && b.state().patch_id==128);
    // Events accepted during transition cannot reappear in the new generation.
    usb(r,0x90,60,100);r.loadPendingPatch();publish();r.apply(0);
    assert(!b.busy() && b.state().patch_id==135 && !r.snapshot().active_notes);
    assert(b.setPatch(128,0));b.panic();r.apply(0);publish();assert(!b.busy());
    // Touch and USB same pitch, both release orders, sustain and channel isolation.
    for(unsigned order=0;order<2;++order) {
        assert(b.noteOn(60,100,0));usb(r,0x90,60,100);r.apply(0);r.render();
        assert(r.snapshot().active_notes==2);
        if(order==0)b.noteOff(60);else usb(r,0x80,60);
        r.apply(0);r.render();assert(r.snapshot().active_notes==1);
        if(order==0)usb(r,0x80,60);else b.noteOff(60);
        r.apply(0);r.render();assert(!r.snapshot().active_notes);
    }
    usb(r,0xb1,64,127);usb(r,0x91,60,100);usb(r,0x92,64,100);
    assert(b.noteOn(67,100,0));r.apply(0);r.render();
    uint16_t voice1;assert(instrument_get_num_voices(1,&voice1)==1);
    float before[3];for(unsigned i=0;i<3;++i) {
        uint16_t v;instrument_get_num_voices(i+1,&v);before[i]=synth[voice_to_base_osc[v]]->midi_note;
    }
    usb(r,0xe1,127,127);usb(r,0xb1,1,127);r.apply(0);r.render();
    unsigned changed=0;for(unsigned i=0;i<3;++i) {
        uint16_t v;instrument_get_num_voices(i+1,&v);
        if(before[i]!=synth[voice_to_base_osc[v]]->midi_note)++changed;
    }
    assert(changed==1);
    usb(r,0x81,60);r.apply(0);r.render();assert(r.snapshot().active_notes==3);
    usb(r,0xb1,64,0);r.apply(0);r.render();assert(r.snapshot().active_notes==2);
    select(r,4);r.apply(0);assert(!r.snapshot().active_notes);
    // Qualify the selected software subset through the actual reusable path,
    // at1/4/8 voices, dry (ROM EQ retained), before exposing it to the UI.
    for(const auto& patch:amy::kPatches) {
        select(r,patch.id);
        for(unsigned count:{1u,4u,8u}) {
            r.commands().panic();r.apply(0);r.render();
            for(unsigned n=0;n<count;++n)usb(r,0x90,48+3*n,100);
            r.apply(0);unsigned peak=0;uint64_t total_us=0,max_us=0;
            const auto clipped_before=r.snapshot().clipped;
            probe_realtime(true);
            for(unsigned block=0;block<256;++block) {
                const auto t=std::chrono::steady_clock::now();const auto pcm=r.render();
                const uint64_t us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-t).count();
                total_us+=us;if(us>max_us)max_us=us;
                if(count==8)std::memcpy(captured+block*512,pcm,512*sizeof(int16_t));
                for(unsigned s=0;s<512;++s) { const unsigned mag=pcm[s]<0?-int(pcm[s]):pcm[s];if(mag>peak)peak=mag; }
            }
            probe_realtime(false);
            if(count==8)saveWav(patch.id);
            assert(peak && r.snapshot().clipped==clipped_before);
            std::printf("REUSABLE_HOST patch=%u notes=%u peak=%u render_avg_us=%llu render_max_us=%llu clipped=0\n",patch.id,count,peak,(unsigned long long)(total_us/256),(unsigned long long)max_us);
            for(unsigned n=0;n<count;++n)usb(r,0x80,48+3*n);
            r.apply(0);assert(!r.snapshot().active_notes);
            probe_realtime(true);for(unsigned block=0;block<15000;++block)assert(r.render());
            const auto tail=r.render();unsigned tail_peak=0;
            for(unsigned s=0;s<512;++s) { const unsigned mag=tail[s]<0?-int(tail[s]):tail[s];if(mag>tail_peak)tail_peak=mag; }
            probe_realtime(false);
            std::printf("RELEASE_HOST patch=%u notes=%u tail_peak=%u\n",patch.id,count,tail_peak);
            assert(tail_peak<=2); // Quantized filter/EQ residue, at most-84dBFS.
        }
    }
    // Exact1000 FM/FM and1000 alternating FM/Juno selections, eight active
    // notes before each barrier, no old queued USB/Touch gate survives.
    size_t stable=0;
    for(unsigned i=0;i<2000;++i) {
        const uint16_t patch=i<1000?(i%2?128:135):(i%2?128:4);
        for(unsigned n=0;n<8;++n)usb(r,0x90,48+n,100);
        r.apply(0);assert(r.snapshot().active_notes==8);r.render();
        select(r,patch);
        probe_realtime(true);r.apply(0);zero(r.render());probe_realtime(false);
        if(i==10)stable=probe_bytes();if(i>10)assert(probe_bytes()==stable);
    }
    const auto confirmed=r.snapshot().patch_id;
    for(unsigned fail:{1u,2u,3u,13u}) {
        assert(r.commands().push({amy::CommandType::Patch,135}));r.apply(0);
        probe_fail_next(fail);r.loadPendingPatch();
        assert(r.snapshot().patch_state==amy::PatchState::Failed && r.snapshot().patch_id==confirmed);
        usb(r,0x90,60,100);r.apply(0);zero(r.render());assert(!r.snapshot().active_notes);
        r.commands().panic();r.apply(0);zero(r.render());
        select(r,confirmed); // Explicit full reconstruction, never a rollback claim.
    }
    for(auto callback:{cancelLoad,disconnectLoad,partialLoad}) {
        assert(r.commands().push({amy::CommandType::Patch,135}));r.apply(0);
        probe_on_allocation(callback==partialLoad?24:4,callback);r.loadPendingPatch();
        assert(r.snapshot().patch_state==amy::PatchState::Failed);zero(r.render());
        if(callback==partialLoad) { r.end();assert(r.begin()); }
        else select(r,confirmed);
    }
    assert(r.commands().push({amy::CommandType::Patch,65535}));r.apply(0);
    assert(r.snapshot().patch_id==confirmed && r.snapshot().patch_state==amy::PatchState::Ready);
    amy_oom("injected service resource failure\n");probe_realtime(true);
    zero(r.render());probe_realtime(false);
    assert(r.snapshot().patch_state==amy::PatchState::Failed && !r.snapshot().active_notes);
    select(r,confirmed);
    std::printf("RUNTIME_HEAP_END bytes=%zu live=%u\n",probe_bytes()-base,probe_live()-live);
    r.end();assert(probe_bytes()==base && probe_live()==live);
    // Exercise real SPSC handoff concurrently, including generation discard.
    amy::PcmHandoff<> q;int16_t output[512];zero((q.take(output,1),output));
    std::thread producer([&] { int16_t pcm[512];for(unsigned n=1;n<10001;++n) {
        for(auto& x:pcm)x=static_cast<int16_t>(n);
        while(!q.push(pcm,n))std::this_thread::yield();
    }});
    for(unsigned n=1;n<10001;++n) {
        while(!q.take(output,n))std::this_thread::yield();
        for(auto x:output)assert(x==int(n));
    }
    producer.join();
    int16_t old[512];for(auto& x:old)x=321;
    assert(q.push(old,1));assert(!q.take(output,2));zero(output);
    std::puts("PASS runtime reuse:2000 swaps,8 voices,ownership,sustain,channel bend/CC1,load failure,Panic,disconnect,partial instrument,retry,no growth; concurrent PCM handoff10000 blocks");
}
