#include "amy_runtime.h"
#include <cassert>
#include <cstdio>

extern "C" void probe_realtime(bool);

using namespace smk::p4;
using seq::Action;
using seq::InputMode;

namespace {
amy::Runtime runtime;
uint32_t now=1;

void send(uint8_t status,uint8_t id,uint8_t value) {
    const uint8_t packet[]{uint8_t(status>>4),status,id,value};
    assert(runtime.usbInput().packet(packet,now));
}

void block() {
    probe_realtime(true);
    runtime.apply(now);
    probe_realtime(false);
    runtime.loadPendingPatch();
    probe_realtime(true);
    assert(runtime.render());
    probe_realtime(false);
    now+=5;
}

uint32_t queue(Action action,int value=0,unsigned index=0) {
    auto& commands=runtime.commands();
    seq::Request request;
    request.action=action;
    request.index=uint8_t(index);
    request.value=int16_t(value);
    request.token=commands.nextToken();
    assert(commands.pushSequencer(request));
    return request.token;
}

void set(Action action,int value=0,unsigned index=0) {
    const auto token=queue(action,value,index);
    block();
    const auto& status=runtime.snapshot().sequencer;
    const auto& ack=action==Action::Stop?status.stop:status.command;
    assert(ack.token==token && !ack.rejected);
}

const auto& state() { return runtime.snapshot(); }
const auto& midi() { return state().sequencer.midi; }
const auto& step(unsigned index) { return state().sequencer_pattern.steps[index]; }

void input() {
    runtime.usbInput().identifyDevice(0x4353,0x4b4d);
    send(0x90,64,80);
    block();
    assert(state().active_notes==1 && midi().smk_connected);
    set(Action::SetInputChannel,0);
    set(Action::SetInputMode,int(InputMode::StepInput),3);
    assert(!step(3).enabled);
    send(0x90,64,96); // Previously sounding key retains ownership.
    send(0x90,65,114);
    block();
    assert(state().active_notes==1 && step(3).note==65 && step(3).velocity==114 && step(3).enabled);
    send(0xb0,64,127);
    send(0x80,64,40);
    block();
    assert(state().active_notes==1 && state().sustain_active);
    set(Action::SelectStep,5);
    send(0x90,65,32); // Repeat before release is ignored even after changing cursor.
    send(0x91,67,77); // Other channel remains musical.
    block();
    assert(!step(5).enabled && state().active_notes==2);
    send(0x90,65,0);
    send(0x90,65,32);
    block();
    assert(step(5).enabled && step(5).velocity==32 && midi().selected==5);
    send(0x80,65,0);
    send(0x90,55,52);
    block();
    assert(step(5).note==55 && step(5).velocity==52);
    set(Action::SetInputMode,int(InputMode::Normal));
    send(0x80,55,0);
    send(0x81,67,0);
    send(0xb0,64,0);
    block();
    assert(!state().active_notes && !state().sustain_active);
    send(0x90,55,70);
    block();
    assert(state().active_notes==1);
    send(0x80,55,0);
    block();
}

void record() {
    set(Action::SetLength,3);
    set(Action::SetRecordGate,74);
    set(Action::SetInputMode,int(InputMode::StepRecord),2);
    send(0x90,60,64);
    send(0x90,60,66);
    send(0x90,62,90);
    block();
    assert(step(2).note==60 && step(2).velocity==64 && step(2).gate==74);
    assert(step(0).note==62 && midi().selected==1 && !state().active_notes);
    set(Action::Rest);
    assert(!step(1).enabled && midi().selected==2);
    send(0x80,60,0);
    send(0x90,60,88);
    block();
    assert(step(2).velocity==88 && midi().selected==0);
    queue(Action::SetStepGate,97,0);
    send(0x80,62,0);
    send(0x90,69,83);
    block();
    assert(step(0).note==69 && step(0).velocity==83 && step(0).gate==74);
    set(Action::SetStepGate,97,0);
    assert(step(0).note==69 && step(0).velocity==83 && step(0).gate==97);
    const auto invalid=queue(Action::SetRecordGate,101);
    block();
    assert(state().sequencer.command.token==invalid && state().sequencer.command.rejected && midi().gate==74);
    set(Action::SetSmkOnly,1);
    runtime.usbInput().disconnect();
    block();
    assert(midi().mode==InputMode::Normal && !midi().smk_connected && !state().active_notes);
    set(Action::SetInputMode,int(InputMode::StepInput),1);
    send(0x90,74,90);
    block();
    assert(!step(1).enabled && state().active_notes==1);
    send(0x80,74,0);
    runtime.usbInput().identifyDevice(0x4353,0x4b4d);
    send(0x90,74,60);
    block();
    assert(step(1).note==74 && step(1).enabled && !state().active_notes);
    runtime.commands().panic();
    block();
    assert(midi().mode==InputMode::Normal && !state().active_notes);
}

void transpose() {
    set(Action::SetLiveChannel,2);
    set(Action::SetTranspose,2);
    set(Action::SetLive,1);
    send(0x92,60,100);
    block();
    assert(midi().live_offset==0 && midi().effective==2 && !state().active_notes);
    send(0x92,62,100);
    block();
    assert(midi().live_offset==2 && midi().effective==4);
    send(0x92,57,100);
    block();
    assert(midi().live_offset==-3 && midi().effective==-1);
    harmony::Config harmony;
    harmony.scale_on=true;
    harmony.chord_on=true;
    harmony.quant=uint8_t(harmony::Quant::NearDown);
    assert(runtime.commands().push({amy::CommandType::Harmony,0,0,0,harmony::pack(harmony),now}));
    set(Action::SetQuantize,1);
    set(Action::Play);
    for(unsigned i=0;i<60 && state().sequencer.current_step==seq::kNoStep;++i) block();
    assert(state().sequencer.current_step==0 && state().last_notes[0]==67 && state().last_count==1);
    assert(step(0).note==69 && state().active_notes==1);
    send(0x92,127,100);
    block();
    assert(midi().effective==69 && state().sequencer.playing);
    for(unsigned i=0;i<60 && state().sequencer.current_step==0;++i) block();
    assert(state().sequencer.current_step==1 && !state().active_notes);
    set(Action::SetReference,0);
    assert(midi().live_offset==127 && midi().effective==127);
    set(Action::SetLive,0);
    assert(midi().effective==2 && state().sequencer.playing);
    send(0x82,57,0);
    send(0x92,57,100);
    set(Action::Stop);
    assert(!state().sequencer.playing && state().active_notes==1);
    send(0x82,57,0);
    block();
    assert(!state().active_notes);
}
void barriers() {
    runtime.usbInput().identifyDevice(0x4353,0x4b4d);
    set(Action::SetInputMode,int(InputMode::StepRecord),1);
    set(Action::SetThru,1);
    send(0x90,79,91);
    block();
    assert(step(1).note==79 && state().active_notes>0);
    set(Action::SetInputMode,int(InputMode::Normal));
    send(0x80,79,0);
    block();
    assert(!state().active_notes);

    set(Action::SetInputMode,int(InputMode::StepRecord),2);
    const auto stale=queue(Action::SetRecordGate,88);
    send(0x90,83,99);
    auto& commands=runtime.commands();
    assert(commands.push({amy::CommandType::Patch,129,0,0,commands.nextToken(),now}));
    for(unsigned i=0;i<20 && state().patch_id!=129;++i) block();
    block();
    assert(state().patch_id==129 && midi().mode==InputMode::Normal);
    assert(step(2).note==60 && state().sequencer.command.token==stale && state().sequencer.command.rejected);
    assert(!state().active_notes);

    seq::Request expired;
    expired.action=Action::SetInputMode;
    expired.value=int16_t(InputMode::StepRecord);
    expired.timed=true;
    expired.submitted_ms=now;
    expired.token=commands.nextToken();
    assert(commands.pushSequencer(expired));
    now+=seq::kConfirmationTimeoutMs;
    block();
    assert(state().sequencer.command.token==expired.token && state().sequencer.command.rejected);
    assert(midi().mode==InputMode::Normal);

    queue(Action::SetInputMode,int(InputMode::StepRecord),2);
    queue(Action::Play);
    queue(Action::SetLive,1);
    queue(Action::SetReference,60);
    const auto stop=queue(Action::Stop);
    block();
    assert(!state().sequencer.playing && state().sequencer.stop.token==stop);
    send(0x90,91,123);
    commands.panic();
    block();
    assert(midi().mode==InputMode::Normal && !midi().live && step(2).note==60 && !state().active_notes);

    // The live transpose channel is exclusive, while keys on another channel feed Arp.
    amy::Command arp{amy::CommandType::Arp,uint16_t(arp::Action::Enable),1,0,commands.nextToken(),now};
    arp.version=smk::p4::arp::kProtocolVersion;
    assert(commands.push(arp));
    set(Action::SetLive,1);
    send(0x92,62,100);
    send(0x91,55,87);
    set(Action::Play);
    for(unsigned i=0;i<70 && !(state().arp.sounding && state().sequencer.playing);++i) block();
    assert(midi().live_offset==2 && state().arp.held_count==1 && state().arp.sounding && state().sequencer.playing);
    runtime.usbInput().disconnect();
    block();
    assert(!state().active_notes && !state().arp.sounding && !state().sequencer.playing);
}

}

int main() {
    assert(runtime.begin());
    input();
    record();
    transpose();
    barriers();
    std::printf("PASS real AMY MIDI ownership, recording, Harmony and live transpose; runtime=%zu snapshot=%zu\n",
                sizeof(runtime),sizeof(amy::Snapshot));
}
