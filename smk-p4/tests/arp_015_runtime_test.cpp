#include "amy_runtime.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <array>
extern "C" void probe_realtime(bool);
extern "C" void my_srand48(uint32_t);

using namespace smk::p4;
static amy::Runtime runtime;
static uint32_t now=0;

static void midi(uint8_t status,uint8_t id,uint8_t value) {
    const uint8_t packet[]{uint8_t(status>>4),status,id,value};
    assert(runtime.usbInput().packet(packet,now));
}
static void request(arp::Action action,uint8_t value) {
    auto& commands=runtime.commands();
    amy::Command c{amy::CommandType::Arp,uint16_t(action),value,0,commands.nextToken(),now};
    c.version=arp::kProtocolVersion;
    assert(commands.push(c));
}
static const int16_t* block() {
    probe_realtime(true);
    runtime.apply(now);
    const auto* pcm=runtime.render();
    assert(pcm);
    probe_realtime(false);
    now+=5;
    return pcm;
}
static void awaitStep(uint8_t pitch,uint32_t previous) {
    for(unsigned i=0;i<30 && runtime.snapshot().groups_admitted==previous;++i) block();
    const auto& state=runtime.snapshot();
    assert(state.groups_admitted==previous+1 && state.last_input==pitch);
}

static void compareArpPcm() {
    static std::array<std::array<int16_t,amy::kBlockFrames*2>,70> expected;
    constexpr unsigned ons[]{0,24,47},offs[]{16,39,63};
    constexpr uint8_t pitches[]{60,64,67},velocities[]{101,82,93};
    for(bool enabled:{false,true}) {
        assert(runtime.begin());
        my_srand48(123);
        if(enabled) {
            request(arp::Action::Enable,1);
            request(arp::Action::Octaves,1);
            request(arp::Action::Swing,50);
            for(unsigned i=0;i<3;++i) midi(0x90,pitches[i],velocities[i]);
        }
        int peak=0;
        for(unsigned i=0;i<expected.size();++i) {
            if(!enabled) for(unsigned j=0;j<3;++j) {
                if(i==ons[j]) midi(0x90,pitches[j],velocities[j]);
                if(i==offs[j]) midi(0x80,pitches[j],0);
            }
            const auto* pcm=block();
            for(unsigned j=0;j<expected[i].size();++j) if(pcm[j]>peak) peak=pcm[j];
            if(enabled) assert(std::memcmp(expected[i].data(),pcm,sizeof(expected[i]))==0);
            else std::memcpy(expected[i].data(),pcm,sizeof(expected[i]));
        }
        assert(peak && runtime.snapshot().groups_admitted==3);
        runtime.end();
    }
    std::puts("PASS ARP PCM vs direct score: exact pitch, velocity, onset and gate");
}

int main() {
    compareArpPcm();
    assert(runtime.begin());
    auto& usb=runtime.usbInput();
    usb.enableControls();usb.identifyDevice(0x4353,0x4b4d);usb.setLegacyProfile(true);
    midi(0x99,47,100);midi(0x99,47,100);
    request(arp::Action::Octaves,1);
    request(arp::Action::Swing,50);
    block();
    assert(runtime.snapshot().arp.config.enabled && runtime.snapshot().arp.usb.token);
    midi(0x89,47,0);
    midi(0x90,60,101);midi(0x90,64,82);midi(0x90,67,93);
    block();
    const auto& state=runtime.snapshot();
    assert(state.arp.sounding && state.active_notes==1 && state.last_input==60);
    for(unsigned i=1;i<16;++i) {block();assert(state.active_notes==1);}
    block();assert(!state.active_notes);
    awaitStep(64,state.groups_admitted);
    awaitStep(67,state.groups_admitted);
    midi(0x80,64,0);block();assert(state.arp.length==2);
    request(arp::Action::Latch,1);block();
    midi(0x80,60,0);midi(0x80,67,0);
    midi(0xb0,64,127);block();
    assert(!state.arp.held_count && state.arp.length==2);
    awaitStep(60,state.groups_admitted);
    auto& commands=runtime.commands();
    assert(commands.push({amy::CommandType::NoteOn,64,100,0,commands.nextToken(),now}));
    block();assert(state.active_notes==2);
    // Fill the USB command queue before Stop. The independent barrier wins
    // even when the Stop acknowledgement itself cannot be queued.
    for(unsigned i=0;i<amy::kUsbCommandCapacity;++i) midi(0x90,71,100);
    midi(0xb0,115,127);block();
    assert(state.active_notes==1 && !state.arp.length && !state.arp.armed && !state.sustain_active);
    for(unsigned i=0;i<30;++i) block();
    assert(state.active_notes==1);
    midi(0xb0,114,127);block();assert(state.arp.armed && !state.arp.length);
    midi(0x90,72,100);block();assert(state.arp.sounding && state.active_notes==2);
    usb.disconnect();block();assert(state.active_notes==1 && !state.arp.length);
    commands.panic();block();assert(!state.active_notes && !state.arp.armed);

    harmony::Config chord;
    chord.chord_on=true;chord.chord=7;chord.touch=false;
    assert(commands.push({amy::CommandType::Harmony,0,0,0,harmony::pack(chord),now}));
    for(unsigned i=0;i<5;++i)
        assert(commands.push({amy::CommandType::NoteOn,uint16_t(50+i),100,0,commands.nextToken(),now}));
    midi(0x90,60,100);block();
    assert(state.last_result==3 && state.groups_rejected==1 && state.active_notes==5);
    commands.panic();block();
    midi(0x90,60,100);block();
    assert(state.last_count==4 && state.active_notes==4);
    const auto expected=harmony::transform(60,chord);
    assert(std::memcmp(expected.notes,state.last_notes,expected.count)==0);
    assert(commands.push({amy::CommandType::Patch,135,0,0,0,now}));
    runtime.apply(now);runtime.loadPendingPatch();block();
    assert(!state.active_notes && !state.arp.length);
    std::printf("MEASURE arp_service_max_us=%u runtime_bytes=%zu\n",
                unsigned(state.arp_service_max_us),sizeof(runtime));
    runtime.end();
    std::puts("PASS real AMY arp gates, latch, USB/Touch isolation, Stop, Panic, Harmony admission and Patch");
}
