#include "amy_time_bridge.h"
#include <cassert>
#include <initializer_list>
int main() {
    smk::AmyTimeBridge bridge;
    assert(!bridge.anchor(1000000, 48000, 48000, 1, 1024));
    assert(!bridge.convert(1000000,48000,1).valid);
    for (uint32_t rate : {44100u,48000u}) {
        assert(bridge.anchor(1000000,rate,rate,1,1024,true));
        auto current=bridge.convert(1000500,rate,1);
        assert(current.valid && current.frame==rate+rate/2000);
        auto next=bridge.convert(1006000,rate,1);
        assert(next.valid && next.frame==rate+rate*6/1000);
        auto late=bridge.convert(1,rate+300,1);
        assert(late.valid && late.late && late.frame==rate+300);
        assert(!bridge.convert(1006000,rate,2).valid);
        auto distant=bridge.convert(UINT64_MAX,rate,1);
        assert(!distant.valid);
        bridge.invalidate(); assert(!bridge.convert(1000000,rate,1).valid);
        // No accumulation of fractional-frame truncation over long intervals.
        bridge.anchor(1,0,rate,2,UINT32_MAX,true);
        for (uint64_t us=1;us<100000000;us+=997) {
            auto r=bridge.convert(us,0,2);
            assert(r.valid && r.frame==(us-1)*rate/1000000);
        }
    }
    uint64_t frame=uint64_t{UINT32_MAX}*48;
    bridge.anchor(0,frame,48000,3,1024,true);
    auto sentinel=bridge.convert(0,frame,3);
    assert(sentinel.valid && sentinel.amy_time_ms==0);
    auto wrapped=bridge.convert(2000,frame,3);
    assert(wrapped.valid && wrapped.amy_time_ms==1);
    bridge.anchor(0,0,48000,1,128,true);
    auto distant=bridge.convert(1000000,0,1);
    assert(!distant.valid && distant.beyond_lookahead);
}
