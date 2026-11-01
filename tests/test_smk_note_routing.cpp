#include "note_routing.h"
#include "arpeggiator.h"
#include "step_sequencer.h"
#include "diagnostics.h"
#include <cassert>
#include <cstdio>
#include <vector>
namespace smk {
Diagnostics& Diagnostics::instance() { static Diagnostics d; return d; }
DiagnosticCounters& Diagnostics::counters() { return counters_; }
}
using namespace smk;
static NoteRoute route(NoteSource source, uint8_t instance = 0, uint8_t channel = 0,
                       uint64_t token = 0, uint8_t input = 60) {
    NoteRoute r{}; r.identity = {source, instance, channel, input, token};
    r.count = 1; r.outputs[0] = {1, 60}; r.velocity = 100; return r;
}
static void identities() {
    uint64_t token = UINT32_MAX;
    assert(allocateNoteToken(token) == uint64_t(UINT32_MAX) + 1);
    token = UINT64_MAX;
    assert(allocateNoteToken(token) == 0 && token == UINT64_MAX);
    assert(allocateNoteToken(token) == 0);
    NoteOwnership owner;
    int refs[128]{};
    auto on = [&](NoteOutput o, uint8_t) { ++refs[o.note]; };
    auto off = [&](NoteOutput o) { assert(refs[o.note] > 0); --refs[o.note]; };
    auto a = route(NoteSource::Keyboard), b = route(NoteSource::Arpeggiator,0,0,1);
    assert(owner.on(a,on) == NoteOwnership::Result::Accepted);
    assert(owner.on(a,on) == NoteOwnership::Result::Duplicate);
    assert(owner.on(b,on) == NoteOwnership::Result::Accepted && refs[60] == 2);
    owner.off(b.identity,off); owner.off(b.identity,off);
    assert(refs[60] == 1 && owner.size() == 1);
    assert(owner.on(b,on) == NoteOwnership::Result::Stale); // replay after Off
    owner.off(route(NoteSource::Pad).identity,off); assert(refs[60] == 1);
    auto c = route(NoteSource::Keyboard,0,1);
    assert(owner.on(c,on) == NoteOwnership::Result::Accepted);
    owner.off(a.identity,off); assert(refs[60] == 1);
    owner.off(c.identity,off); assert(owner.size() == 0);
    // Same physical key pressed again while its previous instance is sustained.
    owner.pedal(0,true,off); owner.on(a,on); owner.off(a.identity,off);
    owner.on(a,on); assert(refs[60] == 2);
    owner.off(a.identity,off); owner.pedal(0,false,off);
    assert(refs[60] == 0 && owner.size() == 0);
    // Fixed fan-out snapshots, including overlapping outputs of another source.
    a.count=3; a.outputs[1]={1,64}; a.outputs[2]={1,67};
    b.identity.token=2; owner.on(a,on); owner.on(b,on);
    owner.off(a.identity,off); assert(refs[60]==1 && refs[64]==0 && refs[67]==0);
    owner.off(b.identity,off); assert(refs[60]==0);
    // Per-track identity and selective Stop, even while sustained.
    owner.pedal(0,true,off);
    auto t0=route(NoteSource::Sequencer,0,0,1), t1=route(NoteSource::Sequencer,1,0,1);
    owner.on(a,on); owner.on(t0,on); owner.on(t1,on);
    owner.off(t0.identity,off); owner.cancel(NoteSource::Sequencer,off);
    assert(refs[60]==1 && owner.size()==1);
    owner.off(a.identity,off); owner.pedal(0,false,off); assert(owner.size()==0);
    // Capacity preserves all accepted releases; rejected key cannot release others.
    for (uint8_t i=0;i<config::kNoteIdentityCapacity;++i)
        assert(owner.on(route(NoteSource::Internal,0,0,0,i),on)==NoteOwnership::Result::Accepted);
    auto full=route(NoteSource::Pad,1);
    assert(owner.on(full,on)==NoteOwnership::Result::Full);
    owner.off(full.identity,off); assert(owner.size()==config::kNoteIdentityCapacity);
    for (uint8_t i=0;i<config::kNoteIdentityCapacity;++i) owner.off(route(NoteSource::Internal,0,0,0,i).identity,off);
    assert(owner.size()==0 && refs[60]==0);
    // 32-bit identifier rollover is represented without aliasing in the 64-bit token.
    auto wrap=route(NoteSource::Sequencer,2,0,UINT32_MAX);
    owner.on(wrap,on); owner.off(wrap.identity,off);
    ++wrap.identity.token; assert(owner.on(wrap,on)==NoteOwnership::Result::Accepted);
    owner.off(wrap.identity,off);
    auto invalid=a; invalid.count=config::kNoteFanOut+1;
    assert(owner.on(invalid,on)==NoteOwnership::Result::Invalid);
    invalid=a; invalid.outputs[2].instrument=2;
    assert(owner.on(invalid,on)==NoteOwnership::Result::Invalid && refs[60]==0);
    owner.pedal(0,true,off); owner.on(a,on); owner.clear(); // Panic/patch
    owner.off(a.identity,off); assert(owner.size()==0);
    assert(owner.on(wrap,on)==NoteOwnership::Result::Stale);
    printf("NoteOwnership=%zu bytes; SynthEvent=%zu bytes; NoteRoute=%zu bytes\n",sizeof(owner),sizeof(SynthEvent),sizeof(NoteRoute));
}
static std::vector<SynthEvent> drain(EventBus& bus) {
    std::vector<SynthEvent> events; SynthEvent e{};
    while(bus.tryReceive(e)) events.push_back(e);
    return events;
}
static void generators() {
    EventBus bus(64); StepSequencer seq; Arpeggiator arp;
    seq.setEventBus(&bus); arp.setEventBus(&bus);
    for(int t=0;t<4;++t) seq.clearTrack(t);
    seq.setTrackMetadata(0,"A",60,0); seq.setTrackMetadata(1,"B",60,0);
    seq.setStep(0,0,60,100,true,false); seq.setStep(1,0,60,100,true,false);
    seq.play(); drain(bus); seq.processTick(0,bus);
    auto events=drain(bus); assert(events.size()==2);
    assert(events[0].source_instance==0 && events[1].source_instance==1);
    assert(events[0].note_token && events[1].note_token);
    seq.setTrackMetadata(0,"A",60,2); // release uses onset channel
    seq.processTick(3,bus); auto offs=drain(bus); assert(offs.size()==2);
    for(size_t i=0;i<2;++i) assert(offs[i].note_token==events[i].note_token && offs[i].channel==0);
    seq.play(); drain(bus); seq.processTick(0,bus);
    SynthEvent key{EventType::NoteOn,EventSource::UsbMidi,0,60,100,0}; bus.send(key);
    seq.stop(); events=drain(bus); assert(events.size()==2);
    assert(events[0].source==EventSource::UsbMidi && events[1].type==EventType::SourceStop);
    arp.setEnabled(true); arp.noteOn(60,100); arp.processTick(0,bus); events=drain(bus);
    assert(events.size()==1 && events[0].note_token);
    const auto old=events[0].note_token;
    arp.processTick(3,bus); events=drain(bus); assert(events.size()==1 && events[0].note_token==old);
    arp.processTick(6,bus); arp.setEnabled(false); events=drain(bus);
    assert(events.size()==1 && events[0].type==EventType::SourceStop);
    arp.setEnabled(true); arp.noteOn(60,100); arp.processTick(0,bus); events=drain(bus);
    assert(events.size()==1 && events[0].note_token>old);
    bus.invalidateMusical(); bus.send(key); bus.cancelNotes(EventSource::Sequencer);
    events=drain(bus); assert(events.size()==2);
    bus.send(key); SynthEvent panic{}; panic.type=EventType::Panic; bus.send(panic);
    events=drain(bus); assert(events.size()==1 && events[0].type==EventType::Panic);
    bus.acknowledgePanic();
}
int main() { identities(); generators(); puts("PASS: identity, bounded fan-out, sustain, capacity, replay, rollover, tracks and source fences"); }
