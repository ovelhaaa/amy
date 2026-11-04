#include "amy_runtime.h"
#include <cassert>
#include <cstdio>
#include <thread>
#include <initializer_list>
#include <cmath>
extern "C" {
#include "amy.h"
}
using namespace smk::p4::amy;
extern "C" void probe_realtime(bool);
extern "C" uint32_t probe_live();
static void midi(Runtime& r,uint8_t status,uint8_t a,uint8_t b) {
    uint8_t p[]={uint8_t(status>>4),status,a,b}; assert(r.usbInput().transfer(p,4,0));
}
static void audio(Runtime& r) { for(unsigned i=0;i<8;++i) assert(r.render()); }
static float voicePitch(unsigned instrument) {
    uint16_t voice; assert(instrument_get_num_voices(instrument,&voice)==1);
    return synth[voice_to_base_osc[voice]]->midi_note;
}
static void queueOrder() {
    Commands touch,usb;
    constexpr unsigned count=50000;
    const auto produce=[](Commands& q,uint8_t channel) {
        for(uint32_t n=1;n<=count;++n) {
            Command c{CommandType::Master,static_cast<uint16_t>(n),100,0,n,n^0x12345678U,channel};
            while(!q.push(c)) std::this_thread::yield();
        }
    };
    std::thread a(produce,std::ref(touch),0),b(produce,std::ref(usb),1);
    uint32_t expected[2]={1,1}; Command c;
    while(expected[0]<=count || expected[1]<=count) {
        for(unsigned source=0;source<2;++source) if((source?usb:touch).pop(c)) {
            assert(c.channel==source && c.token==expected[source]);
            assert(c.id==uint16_t(c.token) && c.timestamp_ms==(c.token^0x12345678U));
            ++expected[source];
        }
    }
    a.join(); b.join();
}
int main() {
    {
        UsbInput diagnostic;
        diagnostic.enableTrace();
        const uint8_t rejected[]={9,0x90,60,100,8,0x90,60,0};
        assert(!diagnostic.transfer(rejected,sizeof(rejected),123));
        assert(!diagnostic.commands.depth() && diagnostic.events==0 && diagnostic.parse_errors==1);
        UsbInput::Trace trace;
        assert(diagnostic.popTrace(trace) && trace.timestamp_ms==123 && trace.packet[0]==9 && trace.valid_bytes==4);
        assert(diagnostic.popTrace(trace) && trace.packet[0]==8 && trace.packet[1]==0x90);
        assert(!diagnostic.popTrace(trace));
        assert(!diagnostic.transfer(rejected,3,124));
        assert(diagnostic.popTrace(trace) && trace.timestamp_ms==124 && trace.valid_bytes==3);
        assert(trace.packet[0]==9 && trace.packet[3]==0);
        assert(!diagnostic.popTrace(trace) && !diagnostic.commands.depth());
    }
    {
        // Real SMK25 stream: a parsed event duplicated on cable 0 and cable 1.
        // The auxiliary copy must not invalidate or duplicate the selected one.
        UsbInput input;
        input.enableTrace();
        const auto generation=input.commands.generation();
        const uint8_t on[]={0x09,0x90,0x3c,0x50,0x19,0x90,0x3c,0x50};
        const uint8_t off[]={0x08,0x80,0x3c,0x40,0x18,0x80,0x3c,0x40};
        assert(input.transfer(on,sizeof(on),1) && input.transfer(off,sizeof(off),2));
        assert(input.events==2 && input.ignored_cable_packets==2 && input.parse_errors==0);
        assert(input.commands.generation()==generation);
        Command c;
        assert(input.commands.pop(c) && c.type==CommandType::NoteOn && c.id==60 && c.value==80);
        assert(input.commands.pop(c) && c.type==CommandType::NoteOff && c.id==60);
        assert(!input.commands.pop(c));
        // A reset or even malformed data on an unselected port cannot affect
        // the selected cable. Malformed selected data still fails closed.
        const uint8_t auxiliary[]={0x1f,0xff,0,0,0x29,0xff,0xff,0xff};
        assert(input.transfer(auxiliary,sizeof(auxiliary),3));
        assert(input.commands.generation()==generation && input.ignored_cable_packets==4);
        const uint8_t malformed[]={0x09,0x90,60,100,0x08,0x90,60,0};
        assert(!input.transfer(malformed,sizeof(malformed),4));
        assert(!input.commands.depth() && input.parse_errors==1 && input.events==2);
        assert(input.commands.generation()!=generation);
    }
    queueOrder();
    Runtime r; const auto baseline=probe_live(); assert(r.begin());
    probe_realtime(true);
    auto& ui=r.commands(); auto& usb=r.usbInput();
    // USB C4 and Touch C4 are distinct voices; either release order works.
    for(bool touch_first:{true,false}) {
        auto t=ui.nextToken(); assert(ui.push({CommandType::NoteOn,60,100,0,t,0}));
        midi(r,0x90,60,90); r.apply(0); assert(r.snapshot().active_notes==2); audio(r);
        midi(r,0xe0,127,127); r.apply(0); audio(r);
        assert(std::fabs(voicePitch(1)-60.0f)<0.001f && voicePitch(2)>61.99f);
        midi(r,0xe0,0,64); r.apply(0); audio(r);
        if(touch_first) ui.release(60,t); else midi(r,0x80,60,0);
        r.apply(1); assert(r.snapshot().active_notes==1); audio(r);
        if(touch_first) midi(r,0x80,60,0); else ui.release(60,t);
        r.apply(2); assert(!r.snapshot().active_notes);
    }
    midi(r,0xb0,64,127); midi(r,0x90,60,100); midi(r,0x91,60,127);
    midi(r,0x90,60,100); // Duplicate held On cannot accrue ownership.
    r.apply(0); assert(r.snapshot().active_notes==2);
    r.apply(20000); assert(r.snapshot().active_notes==2); // USB is not a Touch lease.
    midi(r,0x90,60,0); midi(r,0x81,60,0); r.apply(1);
    assert(r.snapshot().active_notes==1); // Sustain only channel 1.
    midi(r,0xb1,64,0); r.apply(2); assert(r.snapshot().active_notes==1);
    midi(r,0xe0,0,0); r.apply(3); audio(r); assert(std::fabs(voicePitch(1)-58.0f)<0.001f);
    midi(r,0xe0,127,127); r.apply(4); audio(r); assert(voicePitch(1)>61.99f);
    midi(r,0xb0,1,127); r.apply(4); audio(r); assert(voicePitch(1)>62.0f);
    midi(r,0xe0,0,64); midi(r,0xb0,1,0); midi(r,0xb0,64,0); r.apply(5);
    assert(!r.snapshot().active_notes);
    audio(r); // Applied commands become oscillator state at the next render.
    assert(std::fabs(voicePitch(1)-60.0f)<0.001f);
    // Disconnect clears USB sustain and pending events, preserving Touch.
    auto t=ui.nextToken(); assert(ui.push({CommandType::NoteOn,60,100,0,t,0}));
    midi(r,0xb0,64,127); midi(r,0x90,64,100); r.apply(0);
    midi(r,0x90,67,100); usb.disconnect(); r.apply(1);
    assert(r.snapshot().active_notes==1);
    midi(r,0x90,64,100); r.apply(2); assert(r.snapshot().active_notes==2);
    midi(r,0x80,64,0); r.apply(3); assert(r.snapshot().active_notes==1);
    ui.release(60,t); r.apply(4);
    // Exactly the configured total voices. Rejected gates carry no unreleaseable refs.
    for(unsigned n=60;n<60+kVoices+1;++n) midi(r,0x90,n,100);
    r.apply(0); assert(r.snapshot().active_notes==kVoices && r.snapshot().polyphony_rejected==1);
    for(unsigned n=60;n<60+kVoices+1;++n) midi(r,0x80,n,0);
    r.apply(1); assert(!r.snapshot().active_notes);
    for(unsigned cc:{120,123}) {
        midi(r,0x90,60,100); midi(r,0x91,60,100); r.apply(0);
        midi(r,0xb0,cc,0); r.apply(1); assert(r.snapshot().active_notes==1);
        midi(r,0xb1,cc,0); r.apply(2); assert(!r.snapshot().active_notes);
    }
    // Release under saturation triggers USB source cancellation outside queue.
    midi(r,0x90,60,100); r.apply(0);
    for(unsigned i=0;i<kCommandCapacity;++i) midi(r,0xb0,1,i);
    midi(r,0x80,60,0); assert(usb.dropped==1); r.apply(1); assert(!r.snapshot().active_notes);
    // Whole malformed transfer is rejected before publishing a partial chord.
    uint8_t bad[]={9,0x90,60,100,8,0x90,60,0};
    assert(!usb.transfer(bad,sizeof(bad),0)); r.apply(0); assert(!r.snapshot().active_notes);
    assert(!usb.transfer(bad,3,0));
    for(unsigned channel=0;channel<16;++channel) {
        midi(r,0x90|channel,60,100); r.apply(0); assert(r.snapshot().active_notes==1);
        midi(r,0x90|channel,60,0); r.apply(1); assert(!r.snapshot().active_notes);
    }
    midi(r,0x90,60,100); r.apply(0);
    assert(ui.push({CommandType::Patch,135,0,0,0,0}));
    midi(r,0x90,64,100); r.apply(1); audio(r);
    assert(!r.snapshot().active_notes && r.snapshot().patch_id==135);
    midi(r,0x90,60,100); r.apply(2); audio(r); assert(r.snapshot().active_notes==1);
    ui.panic(); r.apply(3); audio(r); assert(!r.snapshot().active_notes);
    // Replay the captured mixed-cable C4 after the phase-sensitive modulation
    // checks above, so this extra render does not alter their initial LFO phase.
    const uint8_t dual_on[]={0x09,0x90,0x3c,0x50,0x19,0x90,0x3c,0x50};
    const uint8_t dual_off[]={0x08,0x80,0x3c,0x40,0x18,0x80,0x3c,0x40};
    const auto parse_errors_before=usb.parse_errors.load();
    assert(usb.transfer(dual_on,sizeof(dual_on),0)); r.apply(0); audio(r);
    assert(r.snapshot().active_notes==1 && usb.parse_errors==parse_errors_before);
    assert(usb.transfer(dual_off,sizeof(dual_off),1)); r.apply(1); audio(r);
    assert(!r.snapshot().active_notes && usb.ignored_cable_packets==2);
    // The producer may enqueue after the owner's once-per-block clock sample.
    // Such a fresh gate must neither expire nor poison maximum latency.
    const uint32_t latency_before=r.snapshot().command_latency_max_ms;
    const uint8_t timed_on[]={9,0x90,60,100}, timed_off[]={8,0x80,60,0};
    assert(usb.transfer(timed_on,sizeof(timed_on),101)); r.apply(100);
    assert(r.snapshot().active_notes==1 && r.snapshot().command_latency_max_ms==latency_before);
    assert(usb.transfer(timed_off,sizeof(timed_off),102)); r.apply(101);
    assert(!r.snapshot().active_notes && r.snapshot().command_latency_max_ms==latency_before);
    const auto future_touch=ui.nextToken();
    assert(ui.push({CommandType::NoteOn,60,100,0,future_touch,201})); r.apply(200);
    assert(r.snapshot().active_notes==1 && r.snapshot().command_latency_max_ms==latency_before);
    ui.release(60,future_touch); r.apply(201); assert(!r.snapshot().active_notes);
    // Truly stale gates remain rejected, while a recent pre-wrap timestamp
    // is accepted after wrap. A just-future timestamp across wrap has age zero.
    assert(usb.transfer(timed_on,sizeof(timed_on),1)); r.apply(1+kLeaseMs);
    assert(!r.snapshot().active_notes);
    assert(eventAgeMs(1,0xfffffffdU)==4 && eventAgeMs(0xffffffffU,0)==0);
    assert(usb.transfer(timed_on,sizeof(timed_on),0xfffffffdU)); r.apply(1);
    assert(r.snapshot().active_notes==1);
    assert(usb.transfer(timed_off,sizeof(timed_off),2)); r.apply(1);
    assert(!r.snapshot().active_notes && r.snapshot().command_latency_max_ms<0x80000000U);
    // Two independent SPSC producers, one real AMY owner, finite stress.
    std::atomic<unsigned> done{0};
    std::thread touch([&] {
        for(unsigned i=0;i<20000;++i) {
            auto token=ui.nextToken(); ui.push({CommandType::NoteOn,60,100,0,token,0});
            ui.release(60,token); if(i%127==0) ui.panic();
        }
        done.fetch_add(1);
    });
    std::thread physical([&] {
        for(unsigned i=0;i<20000;++i) {
            midi(r,0x91,60,100); midi(r,0xb1,64,127); midi(r,0x81,60,0);
            midi(r,0xb1,64,0); if(i%113==0) usb.disconnect();
        }
        done.fetch_add(1);
    });
    while(done.load()!=2 || ui.depth() || usb.commands.depth()) { r.apply(0); assert(r.render()); }
    touch.join(); physical.join(); ui.panic(); r.apply(0); audio(r);
    assert(!r.snapshot().active_notes && !r.snapshot().oom_count);
    probe_realtime(false); r.end(); assert(probe_live()==baseline);
    std::puts("PASS: real AMY two-source/channel identity, sustain, bend/modulation, capacity, saturation, disconnect epochs and two-producer stress; no service heap");
}
