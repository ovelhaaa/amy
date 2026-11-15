#include "arpeggiator.h"
#include "tap_tempo.h"
#include "musical_division.h"
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cmath>

using namespace smk::p4::arp;
namespace tempo = smk::p4::tempo;

int main() {
    tempo::Clock global;
    assert(!global.setBpm(39) && !global.setBpm(241) && global.position().revision==0);
    assert(global.setBpm(120) && global.position().revision==0);
    global.advance(48000);
    assert(global.position().tick()==48);
    assert(global.setBpm(90) && global.position().revision==1 && global.position().tick()==48);
    for(unsigned hour=0;hour<1000;++hour) global.advance(48000*3600);
    assert(global.position().tick()==48+1000*3600*36);
    tempo::Position near_wrap{UINT64_MAX-100000,0,120};
    Clock rollover;
    assert(rollover.step(Rate::Quarter,50,near_wrap,100)==24000);
    near_wrap.phase+=60ull*48000*25*24-1;
    assert(!rollover.due(near_wrap));
    ++near_wrap.phase;
    assert(rollover.due(near_wrap));

    tempo::TapTempo taps;
    uint32_t timestamp=UINT32_MAX-800;
    assert(taps.tap(timestamp)==0);
    assert(taps.tap(timestamp+50)==0);
    assert(taps.tap(timestamp+=500)==0);
    assert(taps.tap(timestamp+=505)==0);
    assert(taps.tap(timestamp+=495)==120);
    assert(taps.tap(timestamp+=300)==0);
    assert(taps.tap(timestamp+=2000)==0);
    for(unsigned i=0;i<2;++i) assert(taps.tap(timestamp+=1000)==0);
    assert(taps.tap(timestamp+=1000)==60);

    constexpr float durations[]{500,375,250,500.0f/3,187.5f,125,250.0f/3,62.5f};
    for(unsigned i=0;i<unsigned(tempo::Division::Count);++i)
        assert(std::abs(tempo::durationMs(tempo::Division(i),120)-durations[i])<0.001f);
    assert(tempo::durationMs(tempo::Division::Quarter,40)==1500);
    assert(tempo::durationMs(tempo::Division::DottedEighth,40)==1125);
    assert(tempo::durationMs(tempo::Division::Quarter,60)==1000);
    assert(tempo::durationMs(tempo::Division::ThirtySecond,240)==31.25f);
    assert(tempo::durationMs(tempo::Division::Count,120)==0);
    std::puts("PASS global tempo phase, wrap, Tap Tempo and rhythmic divisions");
    uint64_t max_lateness_ns=0;
    for(unsigned bpm:{40,60,90,120,180,240}) {
        for(unsigned r=0;r<=unsigned(Rate::ThirtySecond);++r) {
            for(unsigned swing:{50,54,75}) {
                Clock clock;
                tempo::Clock global;
                assert(global.setBpm(bpm));
                const auto rate=Rate(r);
                const bool triplet=rate==Rate::EighthTriplet || rate==Rate::SixteenthTriplet;
                unsigned steps=0;
                for(uint64_t frame=0;steps<1000;frame+=256) {
                    if(clock.due(global.position())) {
                        const uint64_t weight=steps*100ull+(steps%2 && !triplet ? 2*swing-100 : 0);
                        const uint64_t ideal=weight*Clock::ticks(rate)*60*48000;
                        const uint64_t denominator=100*bpm*Clock::kPpqn;
                        assert(frame*denominator>=ideal);
                        const uint64_t lateness=frame*denominator-ideal;
                        assert(lateness<256*denominator);
                        const uint64_t ns=lateness*1000000000/(denominator*48000);
                        if(ns>max_lateness_ns) max_lateness_ns=ns;
                        const unsigned gate=clock.step(rate,swing,global.position(),100);
                        assert(gate>0);
                        ++steps;
                    }
                    global.advance(256);
                }
            }
        }
    }
    std::printf("PASS arp clock maximum onset lateness %.3f us in the host sample trace\n",
                double(max_lateness_ns)/1000);
    Clock fast;
    tempo::Clock fast_global;
    assert(fast_global.setBpm(240));
    assert(fast.step(Rate::ThirtySecond,75,fast_global.position(),10)==225);
    for(unsigned i=0;i<9;++i) fast_global.advance(256);
    assert(fast.due(fast_global.position()) && fast.step(Rate::ThirtySecond,75,fast_global.position(),10)<256);

    Clock live;
    tempo::Clock live_global;
    assert(live_global.setBpm(60));
    assert(live.due(live_global.position()));
    live.step(Rate::Quarter,50,live_global.position(),100);
    for(unsigned i=0;i<100;++i) {
        assert(!live.due(live_global.position()));
        live_global.advance(256);
    }
    assert(live_global.setBpm(240));
    unsigned fast_blocks=0;
    while(!live.due(live_global.position())) { live_global.advance(256);++fast_blocks; }
    assert(fast_blocks==22);
    live.step(Rate::EighthTriplet,75,live_global.position(),10);
    for(unsigned i=0;i<16;++i) {
        assert(!live.due(live_global.position()));
        live_global.advance(256);
    }
    assert(live.due(live_global.position()));

    Engine arp;
    tempo::Clock performance;
    assert(!arp.configure(Action::Bpm,39));
    assert(!arp.configure(Action::Mode,5));
    arp.configure(Action::Enable,1);
    arp.configure(Action::Octaves,1);
    arp.on({67,80,0,2});arp.on({60,90,0,2});arp.on({64,100,0,2});
    const auto next=[&] {
        arp.prepare();
        Input note;
        uint32_t gate=0;
        for(unsigned i=0;i<300;++i) {
            if(arp.next(note,gate,performance.position())) { assert(gate>0); return note; }
            performance.advance(256);
        }
        assert(false);
        return note;
    };
    for(auto pitch:{60,64,67,60}) assert(next().pitch==pitch);
    arp.configure(Action::Mode,uint8_t(Mode::Down));
    for(auto pitch:{67,64,60,67}) assert(next().pitch==pitch);
    arp.configure(Action::Mode,uint8_t(Mode::UpDown));
    for(auto pitch:{60,64,67,64,60}) assert(next().pitch==pitch);
    arp.configure(Action::Mode,uint8_t(Mode::AsPlayed));
    for(auto pitch:{67,60,64,67}) assert(next().pitch==pitch);
    arp.off({64,0,0,1});assert(arp.status().held_count==3);
    arp.off({64,0,0,2});assert(arp.status().held_count==2);
    arp.configure(Action::Latch,1);
    arp.off({60,0,0,2});arp.off({67,0,0,2});
    arp.prepare();
    assert(arp.status().held_count==0 && arp.status().length==2);
    arp.on({127,101,1,2});
    arp.configure(Action::Octaves,4);
    assert(next().pitch==127 && arp.status().length==1);
    arp.stop();assert(!arp.status().armed && !arp.status().length);
    arp.on({0,42,0,3});assert(next().velocity==42);
    for(uint8_t i=1;i<=8;++i) arp.on({i,100,0,3});
    arp.prepare();
    assert(arp.status().held_count==8 && arp.status().length==32 && arp.status().rejected==1);
    arp.clearChannel(0);arp.prepare();assert(!arp.status().length);
    arp.configure(Action::Octaves,1);
    assert(arp.configure(Action::Restart,1));
    arp.on({60,100,0,3});arp.on({64,100,0,3});
    assert(next().pitch==60);
    assert(next().pitch==64);
    arp.off({60,0,0,3});arp.off({64,0,0,3});
    arp.on({72,100,0,3});arp.on({76,100,0,3});arp.on({72,80,0,3});
    assert(next().pitch==72);
    arp.off({72,0,0,3});
    assert(arp.configure(Action::ClearLatch,1));
    arp.prepare();
    assert(arp.status().length==1 && arp.status().pattern[0]==76);
    arp.configure(Action::Mode,uint8_t(Mode::Random));
    arp.configure(Action::Octaves,1);
    for(unsigned run=0;run<2;++run) {
        arp.stop();
        arp.on({60,100,0,3});arp.on({64,100,0,3});arp.on({67,100,0,3});
        for(auto pitch:{60,64,60,67,67}) assert(next().pitch==pitch);
    }
}
