#include "local_performance_backend.h"
#include <cassert>
#include <cstdio>
#include <chrono>
#include <cstring>
#include <initializer_list>
extern "C" {
#include "amy.h"
void probe_realtime(bool);size_t probe_bytes();uint32_t probe_calls();
}
using namespace smk::p4;
static void usb(amy::Runtime& r,uint8_t status,uint8_t a,uint8_t b=0){uint8_t p[]={uint8_t(status>>4),status,a,b};assert(r.usbInput().transfer(p,4,0));}
static void config(amy::Runtime& r,harmony::Config c){assert(r.commands().push({amy::CommandType::Harmony,0,0,0,harmony::pack(c)}));r.apply(0);assert(harmony::pack(r.snapshot().harmony)==harmony::pack(c));}
static void panic(amy::Runtime& r){r.commands().panic();r.apply(0);assert(!r.snapshot().active_notes);}
int main(){
    amy::Runtime r;assert(r.begin());LocalAmyPerformanceBackend b(r.commands());
    auto publish=[&]{b.publish(r.snapshot(),true,0,0);};publish();
    auto c=harmony::preset(1);config(r,c);
    usb(r,0x90,60,100);r.apply(0);assert(r.snapshot().active_notes==3);
    assert(r.snapshot().last_notes[0]==60 && r.snapshot().last_notes[1]==64 && r.snapshot().last_notes[2]==67);
    r.render();for(unsigned i=0;i<3;++i){uint16_t v;instrument_get_num_voices(i+1,&v);assert(synth[voice_to_base_osc[v]]->midi_note==r.snapshot().last_notes[i]);}
    // Configuration change freezes an existing group and Off still uses input.
    config(r,harmony::preset(2));usb(r,0x80,60);r.apply(0);assert(!r.snapshot().active_notes);
    probe_realtime(true);for(unsigned i=0;i<15000;++i)r.render();const auto tail=r.render();
    for(unsigned i=0;i<512;++i)assert(tail[i]>=-2 && tail[i]<=2);
    probe_realtime(false);
    config(r,c);usb(r,0x90,60,100);usb(r,0x90,64,90);r.apply(0);assert(r.snapshot().active_notes==6);
    auto rejected=r.snapshot().groups_rejected;usb(r,0x90,67,100);r.apply(0);assert(r.snapshot().active_notes==6 && r.snapshot().groups_rejected==rejected+1);
    usb(r,0x80,60);r.apply(0);assert(r.snapshot().active_notes==3);usb(r,0x80,64);r.apply(0);assert(!r.snapshot().active_notes);
    c.chord=13;config(r,c);usb(r,0x90,60,100);usb(r,0x90,62,100);r.apply(0);assert(r.snapshot().active_notes==8);
    usb(r,0x90,64,100);r.apply(0);assert(r.snapshot().active_notes==8);panic(r);
    config(r,harmony::preset(1));usb(r,0xb0,64,127);usb(r,0x90,60,100);usb(r,0x80,60);r.apply(0);assert(r.snapshot().active_notes==3);
    usb(r,0x91,60,100);r.apply(0);assert(r.snapshot().active_notes==4);usb(r,0xb1,64,0);r.apply(0);assert(r.snapshot().active_notes==4);
    usb(r,0xb0,64,0);r.apply(0);assert(r.snapshot().active_notes==1);usb(r,0x81,60);r.apply(0);assert(!r.snapshot().active_notes);
    publish();assert(b.noteOn(60,100,0));usb(r,0x90,60,100);r.apply(0);r.render();assert(r.snapshot().active_notes==6);
    float before[6];for(unsigned i=0;i<6;++i){uint16_t v;instrument_get_num_voices(i+1,&v);before[i]=synth[voice_to_base_osc[v]]->midi_note;}
    usb(r,0xe0,127,127);usb(r,0xb0,1,127);r.apply(0);r.render();unsigned changes=0;
    for(unsigned i=0;i<6;++i){uint16_t v;instrument_get_num_voices(i+1,&v);if(i<3)assert(before[i]==synth[voice_to_base_osc[v]]->midi_note);changes+=before[i]!=synth[voice_to_base_osc[v]]->midi_note;}assert(changes==3);
    r.usbInput().disconnect();r.apply(0);assert(r.snapshot().active_notes==3);b.noteOff(60);r.apply(0);assert(!r.snapshot().active_notes);
    usb(r,0x90,60,100);r.apply(0);assert(r.snapshot().active_notes==3);usb(r,0x90,60,0);r.apply(0);assert(!r.snapshot().active_notes);
    usb(r,0x90,60,100);r.apply(0);assert(r.snapshot().active_notes==3);panic(r);
    publish();assert(b.noteOn(60,100,0));r.apply(0);assert(r.snapshot().active_notes==3);r.apply(10000);assert(!r.snapshot().active_notes);b.noteOff(60);
    c=harmony::preset(1);c.touch=false;c.channel=15;config(r,c);publish();assert(b.noteOn(60,100,0));usb(r,0x90,60,100);usb(r,0x9f,60,100);r.apply(0);assert(r.snapshot().active_notes==5);panic(r);b.noteOff(60);
    // Warm then assert no AMY malloc/free in event and render service under chords.
    config(r,harmony::preset(1));usb(r,0x90,60,100);r.apply(0);r.render();panic(r);r.render();
    const auto heap=probe_bytes();const auto calls=probe_calls();uint64_t max_us=0,render_max=0;
    probe_realtime(true);
    for(unsigned i=0;i<1000;++i){
        usb(r,0x90,60,100);usb(r,0x90,64,100);auto t=std::chrono::steady_clock::now();r.apply(0);
        auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-t).count();if(uint64_t(us)>max_us)max_us=us;
        t=std::chrono::steady_clock::now();assert(r.render());us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-t).count();if(uint64_t(us)>render_max)render_max=us;
        usb(r,0x80,60);usb(r,0x80,64);r.apply(0);r.render();assert(!r.snapshot().active_notes);
    }
    probe_realtime(false);assert(probe_bytes()==heap && probe_calls()==calls);
    printf("PASS harmony runtime ownership/sustain/channel/source/lease/atomic capacity; heap=%zu calls_delta=0 apply_2triads_max_us=%llu render_max_us=%llu sizeof_runtime=%zu\n",heap,(unsigned long long)max_us,(unsigned long long)render_max,sizeof(r));
    for(uint16_t patch:{128,135,138,142,4,8,31}){
        assert(r.commands().push({amy::CommandType::Patch,patch}));r.apply(0);r.loadPendingPatch();assert(r.snapshot().patch_state==amy::PatchState::Ready && !r.snapshot().active_notes);
        config(r,harmony::preset(1));usb(r,0x90,48,100);usb(r,0x90,52,100);r.apply(0);unsigned peak=0;
        for(unsigned i=0;i<128;++i){auto pcm=r.render();for(unsigned n=0;n<512;++n){unsigned mag=pcm[n]<0?-int(pcm[n]):pcm[n];if(mag>peak)peak=mag;}}
        assert(peak && !r.snapshot().oom_count);usb(r,0x80,48);usb(r,0x80,52);r.apply(0);assert(!r.snapshot().active_notes);
        printf("PASS harmony AMY patch=%u peak=%u clipped_cumulative=%u\n",patch,peak,r.snapshot().clipped);
    }
    panic(r);c=harmony::preset(3);config(r,c);
    // A full 32-command Touch burst repeatedly replaces four-note groups.
    for(unsigned i=0;i<32;++i)assert(r.commands().push({amy::CommandType::NoteOn,60,100,0,r.commands().nextToken(),0}));
    for(unsigned i=0;i<32;++i)usb(r,0xb1,1,100);
    probe_realtime(true);r.apply(0);assert(r.snapshot().active_notes==4);r.render();probe_realtime(false);
    assert(!r.snapshot().oom_count);panic(r);
    // Stale Touch Off cannot stop a fresh token during the old release tail.
    const auto old=r.commands().nextToken();assert(r.commands().push({amy::CommandType::NoteOn,60,100,0,old}));r.apply(0);r.render();
    r.commands().release(60,old);r.apply(0);r.render();const auto fresh=r.commands().nextToken();
    assert(r.commands().push({amy::CommandType::NoteOn,60,100,0,fresh}));r.apply(0);r.render();
    r.commands().release(60,old);r.apply(0);assert(r.snapshot().active_notes==4);
    r.commands().release(60,fresh);r.apply(0);assert(!r.snapshot().active_notes);
    printf("HARMONY_TIMING transform_max_us=%u admission_max_us=%u delta_reserve=%u boot_guard=%zu\n",r.snapshot().transform_max_us,r.snapshot().admission_max_us,amy::kDeltaReserve,amy::Runtime::bootFreeInternalBytes());
    puts("PASS full Touch burst: no service allocation; stale Touch release isolation");
    r.end();
}
