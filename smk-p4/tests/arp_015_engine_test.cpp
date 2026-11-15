#include "arpeggiator.h"
#include <cassert>
#include <cstdint>

using namespace smk::p4::arp;

int main() {
    for(unsigned bpm:{40,60,90,120,180,240}) {
        for(unsigned r=0;r<=unsigned(Rate::ThirtySecond);++r) {
            for(unsigned swing:{50,54,75}) {
                Clock clock;
                const auto rate=Rate(r);
                const bool triplet=rate==Rate::EighthTriplet || rate==Rate::SixteenthTriplet;
                unsigned steps=0;
                for(uint64_t frame=0;steps<1000;frame+=256) {
                    if(clock.due()) {
                        const uint64_t weight=steps*100ull+(steps%2 && !triplet ? 2*swing-100 : 0);
                        const uint64_t ideal=weight*Clock::ticks(rate)*60*48000;
                        const uint64_t denominator=100*bpm*Clock::kPpqn;
                        assert(frame*denominator>=ideal);
                        assert(frame*denominator-ideal<256*denominator);
                        const unsigned gate=clock.step(rate,swing,bpm,100);
                        assert(gate>0);
                        ++steps;
                    }
                    clock.advance(bpm,256);
                }
            }
        }
    }
    Clock live;
    assert(live.due());
    live.step(Rate::Quarter,50,60,100);
    for(unsigned i=0;i<100;++i) {
        assert(!live.due());
        live.advance(60,256);
    }
    unsigned fast_blocks=0;
    while(!live.due()) { live.advance(240,256);++fast_blocks; }
    assert(fast_blocks==22);
    live.step(Rate::EighthTriplet,75,240,10);
    for(unsigned i=0;i<16;++i) {
        assert(!live.due());
        live.advance(240,256);
    }
    assert(live.due());

    Engine arp;
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
            if(arp.next(note,gate)) { assert(gate>0); return note; }
            arp.advance(256);
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
    arp.configure(Action::Mode,uint8_t(Mode::Random));
    arp.configure(Action::Octaves,1);
    for(unsigned run=0;run<2;++run) {
        arp.stop();
        arp.on({60,100,0,3});arp.on({64,100,0,3});arp.on({67,100,0,3});
        for(auto pitch:{60,64,60,67,67}) assert(next().pitch==pitch);
    }
}
