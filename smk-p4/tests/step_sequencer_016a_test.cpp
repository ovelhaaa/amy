#include "amy_runtime.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <thread>

extern "C" void probe_realtime(bool);
extern "C" void my_srand48(uint32_t);

using namespace smk::p4;
using seq::Action;

namespace {
constexpr auto kBlock=amy::kBlockFrames;
static amy::Runtime runtime;
static uint32_t now=0;

void testClock() {
    constexpr tempo::Division divisions[]{
        tempo::Division::Quarter,tempo::Division::Eighth,tempo::Division::Sixteenth,
        tempo::Division::ThirtySecond,tempo::Division::EighthTriplet,tempo::Division::SixteenthTriplet
    };
    constexpr unsigned ticks[]{24,12,6,3,8,4};
    uint64_t max_onset_frames=0, max_gate_frames=0;
    for(unsigned bpm:{40,120,240}) for(unsigned div=0;div<6;++div) for(unsigned swing:{50,75}) {
        tempo::Clock clock;
        seq::Engine sequencer;
        assert(clock.setBpm(bpm));
        assert(sequencer.setDivision(divisions[div]) && sequencer.setSwing(swing));
        seq::Pattern pattern;
        for(unsigned i=0;i<seq::kSteps;++i) pattern.steps[i]={uint8_t(60+i),uint8_t(80+i),90,true};
        assert(sequencer.setPattern(pattern));
        assert(sequencer.play(clock.position()));
        const uint64_t nominal=uint64_t(ticks[div])*tempo::kUnitsPerTick;
        uint64_t expected_on=0,expected_off=0;
        const uint64_t speed=clock.position().unitsPerFrame();
        unsigned onsets=0,offs=0;
        for(uint64_t frames=0;onsets<256;frames+=kBlock) {
            const auto events=sequencer.update(clock.position());
            if(events.release) {
                assert(frames*speed>=expected_off && frames*speed-expected_off<kBlock*speed);
                max_gate_frames=std::max(max_gate_frames,frames-(expected_off+speed-1)/speed);
                ++offs;
            }
            if(events.velocity) {
                assert(events.note==60+onsets%seq::kSteps && events.velocity==80+onsets%seq::kSteps);
                assert(sequencer.status().current_step==onsets%seq::kSteps);
                assert(frames*speed>=expected_on && frames*speed-expected_on<kBlock*speed);
                max_onset_frames=std::max(max_onset_frames,frames-(expected_on+speed-1)/speed);
                const unsigned weight=div>=4?100:2*(onsets%2?100-swing:swing);
                const auto interval=nominal*weight/100;
                expected_off=expected_on+interval*9/10;
                expected_on+=interval;
                ++onsets;
            }
            clock.advance(kBlock);
        }
        assert(offs==255);
    }
    std::printf("PASS 36 clock cases, 9216 onsets, max onset lateness=%llu frames, max gate lateness=%llu frames\n",
                static_cast<unsigned long long>(max_onset_frames),static_cast<unsigned long long>(max_gate_frames));
}

void testEdits() {
    tempo::Clock clock;
    seq::Engine engine;
    seq::Pattern pattern;
    assert(engine.play(clock.position()));
    assert(!engine.update(clock.position()).velocity); // Empty pattern.
    assert(!engine.stop());
    pattern.length=3;
    pattern.steps[0]={60,80,25,true};
    pattern.steps[2]={127,127,100,true};
    assert(engine.setPattern(pattern));
    pattern.steps[2].velocity=0;
    assert(!engine.setPattern(pattern)); // Failed replacement is atomic.
    assert(!engine.setLength(0) && !engine.setLength(17));
    assert(!engine.setStep(16,{}) && !engine.setStep(0,{128,127,50,true}));
    assert(!engine.setSwing(49) && !engine.setTranspose(25));
    assert(!engine.setDivision(tempo::Division::Count));
    assert(engine.play(clock.position()));
    assert(engine.update(clock.position()).velocity==80);
    const auto gate_phase=uint64_t(6)*tempo::kUnitsPerTick/4;
    for(unsigned i=0;i<4;++i) {clock.advance(kBlock);assert(!engine.update(clock.position()).release);}
    assert(clock.setBpm(240));
    do {
        clock.advance(kBlock);
        const auto events=engine.update(clock.position());
        assert(events.release==(clock.position().phase>=gate_phase));
    } while(clock.position().phase<gate_phase);
    assert(engine.status().bpm==240);
    assert(engine.setDivision(tempo::Division::ThirtySecond));
    for(unsigned i=0;engine.status().current_step!=1 && i<32;++i) {
        clock.advance(kBlock);
        assert(!engine.update(clock.position()).velocity); // Rest.
    }
    assert(engine.status().current_step==1);
    for(unsigned i=0;engine.status().current_step!=2 && i<32;++i) {
        clock.advance(kBlock);
        const auto events=engine.update(clock.position());
        if(engine.status().current_step==2) assert(events.velocity==127 && events.note==127);
    }
    assert(engine.status().current_step==2);
    assert(engine.setEnabled(false));
    assert(!engine.play(clock.position()));
    assert(!engine.update(clock.position()).velocity && !engine.status().playing);
    std::puts("PASS empty pattern, rests, gate, rate/BPM edits in flight, pattern validation and disable");
}

void midi(uint8_t status,uint8_t id,uint8_t value) {
    const uint8_t packet[]{uint8_t(status>>4),status,id,value};
    assert(runtime.usbInput().packet(packet,now));
}
uint32_t request(Action action,int16_t value=0) {
    auto& commands=runtime.commands();
    seq::Request req;
    req.action=action;req.value=value;req.token=commands.nextToken();
    assert(commands.pushSequencer(req));
    return req.token;
}
const int16_t* block() {
    probe_realtime(true);
    runtime.apply(now);
    const int16_t* pcm=runtime.render();
    probe_realtime(false);
    assert(pcm);
    now+=5;
    return pcm;
}
void loadPattern(uint8_t note,uint8_t velocity,uint8_t gate=50) {
    auto& commands=runtime.commands();
    seq::Request pattern;
    pattern.action=Action::SetPattern;
    pattern.token=commands.nextToken();
    pattern.pattern.length=2;
    pattern.pattern.steps[0]={note,velocity,gate,true};
    assert(commands.pushSequencer(pattern));
}
void tempoCommand(arp::Action action,uint8_t value) {
    auto& commands=runtime.commands();
    amy::Command c{amy::CommandType::Arp,uint16_t(action),value,0,commands.nextToken(),now};
    c.version=arp::kProtocolVersion;
    assert(commands.push(c));
}
void harmonyCommand(const harmony::Config& config) {
    assert(runtime.commands().push({amy::CommandType::Harmony,0,0,0,harmony::pack(config),now}));
}
void awaitStep() {
    for(unsigned i=0;runtime.snapshot().sequencer.current_step==seq::kNoStep && i<60;++i) block();
    assert(runtime.snapshot().sequencer.current_step==0);
}

void testPcmAgainstMidi() {
    static std::array<std::array<int16_t,kBlock*2>,56> reference;
    for(bool enabled:{false,true}) {
        assert(runtime.begin());
        my_srand48(123);
        if(enabled) {loadPattern(62,83);request(Action::Play);}
        for(unsigned b=0;b<reference.size();++b) {
            if(!enabled && (b==0 || b==47)) midi(0x90,62,83);
            if(!enabled && b==12) midi(0x80,62,0);
            const auto* pcm=block();
            if(enabled) assert(std::memcmp(reference[b].data(),pcm,sizeof(reference[b]))==0);
            else std::memcpy(reference[b].data(),pcm,sizeof(reference[b]));
        }
        assert(runtime.snapshot().groups_admitted==2);
        runtime.end();
    }
    std::puts("PASS real AMY sequencer PCM equals MIDI score for pitch, velocity, rests, gate and loop");
}

void testMailbox() {
    seq::Mailbox mailbox;
    constexpr unsigned count=10000;
    std::thread producer([&] {
        for(uint32_t token=1;token<=count;++token) {
            seq::Request request;
            request.action=Action::SetPattern;
            request.token=token;
            for(unsigned i=0;i<seq::kSteps;++i)
                request.pattern.steps[i]={uint8_t((token+i)%128),90,50,true};
            while(!mailbox.push(request,42)) std::this_thread::yield();
        }
    });
    for(uint32_t token=1;token<=count;++token) {
        const seq::Request* request=nullptr;
        while(!(request=mailbox.front())) std::this_thread::yield();
        assert(request->token==token && request->generation==42);
        for(unsigned i=0;i<seq::kSteps;++i)
            assert(request->pattern.steps[i].note==(token+i)%128);
        mailbox.consume();
    }
    producer.join();
    assert(mailbox.front()==nullptr);
    std::puts("PASS concurrent mailbox sequence and payload integrity over 10000 requests");
}

void testOwner() {
    assert(runtime.begin());
    auto& commands=runtime.commands();
    const auto& snap=runtime.snapshot();
    loadPattern(60,100,100);
    midi(0x90,60,100);
    request(Action::Play);
    block();
    assert(snap.active_notes==2 && snap.sequencer.current_step==0);
    midi(0x80,60,0);
    block();
    assert(snap.active_notes==1 && snap.sequencer.playing);
    midi(0x90,60,100);
    block();
    assert(snap.active_notes==2);
    const auto stop=request(Action::Stop);
    block();
    assert(snap.active_notes==1 && !snap.sequencer.playing && snap.sequencer.token==stop);
    midi(0x80,60,0);
    block();
    assert(!snap.active_notes);
    for(unsigned i=0;i<24;++i) {
        request(Action::Play);
        awaitStep();
        assert(snap.active_notes==1);
        request(Action::Stop);
        block();
        assert(!snap.active_notes && !snap.sequencer.playing);
    }

    midi(0x90,60,100);
    request(Action::Play);
    awaitStep();
    assert(snap.active_notes==2);
    commands.panic();
    block();
    assert(!snap.active_notes && !snap.sequencer.playing);

    for(uint8_t i=0;i<amy::kVoices;++i) midi(0x90,48+i,100);
    block();
    const auto rejected=snap.polyphony_rejected;
    request(Action::Play);
    awaitStep();
    assert(snap.active_notes==amy::kVoices && snap.polyphony_rejected==rejected+1);
    request(Action::Stop);
    block();
    assert(snap.active_notes==amy::kVoices);
    commands.panic();
    block();
    assert(!snap.active_notes && !snap.sequencer.playing);

    harmony::Config config;
    config.scale_on=true;config.chord_on=true;config.quant=uint8_t(harmony::Quant::Up);
    harmonyCommand(config);
    loadPattern(60,100,100);
    request(Action::SetTranspose,1);
    request(Action::SetQuantize,1);
    request(Action::Play);
    awaitStep();
    assert(snap.active_notes==1 && snap.last_notes[0]==62);
    config.root=1;
    harmonyCommand(config);
    block();
    request(Action::Stop);
    block();
    assert(!snap.active_notes);

    config.chord_on=false;
    harmonyCommand(config);
    request(Action::SetQuantize,0);
    tempoCommand(arp::Action::Bpm,240);
    tempoCommand(arp::Action::Enable,1);
    tempoCommand(arp::Action::Gate,100);
    tempoCommand(arp::Action::Swing,50);
    request(Action::Play);
    midi(0x90,65,100);
    awaitStep();
    assert(snap.active_notes==2 && snap.last_notes[0]==61);
    assert(snap.arp.tempo.bpm==240 && snap.sequencer.bpm==240);
    request(Action::Restart);
    block();
    awaitStep();
    for(unsigned i=0;i<seq::kQueueCapacity;++i) request(Action::Play);
    seq::Request overflow;
    assert(!commands.pushSequencer(overflow));
    request(Action::Stop);
    block();
    assert(!snap.sequencer.playing && snap.active_notes<=1 && snap.arp.config.enabled);

    request(Action::Play);
    awaitStep();
    assert(commands.push({amy::CommandType::Patch,128,0,0,0,now}));
    runtime.apply(now);
    runtime.loadPendingPatch();
    block();
    assert(snap.patch_id==128 && !snap.active_notes && !snap.sequencer.playing);
    const auto bad=request(Action::SetLength,-1);
    block();
    assert(snap.sequencer.token==bad && snap.sequencer.rejected && snap.sequencer.errors==1);
    request(Action::Play);
    commands.panic();
    block();
    assert(snap.sequencer.errors==2 && !snap.sequencer.playing && !snap.active_notes);
    std::printf("MEASURE runtime=%zu snapshot=%zu engine=%zu mailbox=%zu bytes\n",
                sizeof(runtime),sizeof(amy::Snapshot),sizeof(seq::Engine),sizeof(seq::Mailbox));
    runtime.end();
    std::puts("PASS owner commands, shared Arp BPM, note ownership, capacity, scale change, overflow Stop, patch and Panic under heap probe");
}
}

int main() {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    testClock();
    testEdits();
    testPcmAgainstMidi();
    testMailbox();
    testOwner();
}
