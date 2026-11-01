#include "arpeggiator.h"
#include "step_sequencer.h"
#include "diagnostics.h"
#include <cassert>
#include <vector>
namespace smk {
Diagnostics& Diagnostics::instance() { static Diagnostics d; return d; }
DiagnosticCounters& Diagnostics::counters() { return counters_; }
}
using namespace smk;
static std::vector<SynthEvent> drain(EventBus& b) {
    std::vector<SynthEvent> events; SynthEvent e{};
    while (b.receive(e,0)) events.push_back(e);
    return events;
}
int main() {
    for (float bpm : {30.f,60.f,120.f,180.f,300.f}) {
        const uint64_t period = static_cast<uint64_t>(60000000.0 / bpm / 24);
        EventBus bus(64); Arpeggiator arp; StepSequencer seq;
        for (int t=0;t<4;++t) { seq.clearTrack(t); seq.setStep(t,0,60+t,100,true,false); }
        MusicalTiming timely{1000000,1000000,period};
        arp.setEnabled(true); arp.noteOn(72,100); seq.play();
        arp.processTick(0,bus,timely); seq.processTick(0,bus,timely);
        auto events = drain(bus); assert(events.size()==5);
        for (auto e:events) { assert(e.type==EventType::NoteOn); assert(e.deadline_us==1000000+3*period); }
        MusicalTiming late{2000000,2000000+period,period};
        arp.processTick(6,bus,late); seq.processTick(6,bus,late);
        events=drain(bus); assert(events.size()==5);
        for (auto e:events) assert(e.type==EventType::NoteOff);
        assert(seq.currentStep()==2);
        seq.processTick(30,bus,timely); // Skip 12/18/24, no replay.
        assert(seq.currentStep()==6); assert(drain(bus).empty());
        seq.stop(bus); arp.reset(bus); assert(drain(bus).empty());
        arp.processTick(36,bus,timely); seq.processTick(36,bus,timely);
        assert(drain(bus).empty());
        SynthEvent e{EventType::NoteOn,EventSource::Sequencer,0,60,100,1000};
        e.deadline_us=2000;
        assert(!expiredTrigger(e,1999) && expiredTrigger(e,2000));
        e.type=EventType::NoteOff; assert(!expiredTrigger(e,3000));
        bus.send(e); e.type=EventType::NoteOn; bus.send(e);
        e.type=EventType::Panic; bus.send(e);
        SynthEvent out{}; assert(bus.receive(out,0) && out.type==EventType::Panic);
        bus.acknowledgePanic(); assert(drain(bus).empty());
        for (float swing : {50.f,67.f,75.f}) {
            StepSequencer s; s.setSwing(swing); s.play();
            s.processTick(0,bus,timely); drain(bus);
            s.processTick(48,bus,late); // 8 crossed boundaries, latest suppressed.
            assert(s.currentStep()==9);
            for (auto release:drain(bus)) assert(release.type==EventType::NoteOff);
        }
    }
    assert(Diagnostics::instance().counters().musical_releases_late.load()>0);
}
