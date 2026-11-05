#include "local_performance_backend.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
extern "C" {
#include "amy.h"
#include "patches.h"
void probe_realtime(bool);
size_t probe_bytes();
}
using namespace smk::p4;
int main() {
    for(const auto& p:library::kCandidates) {
        assert(p.id<_PATCHES_NUM_BUILTIN && patch_commands[p.id]);
        assert(p.oscillators==patch_oscs[p.id]);
    }
    const auto baseline=probe_bytes();
    amy::Runtime r;assert(r.begin());
    LocalAmyPerformanceBackend b(r.commands());
    auto publish=[&] { b.publish(r.snapshot(),false,0,0); };
    publish();
    assert(b.voiceCapacity()==8 && b.patchCount()==4);
    const auto bytes=probe_bytes();probe_realtime(true);
    uint32_t now=0;
    for(unsigned n=0;n<b.patchCount();++n) {
        PatchInfo p;assert(b.patchInfo(n,p,now) && p.available && p.id==b.patchAt(n));
        const auto old=b.state().patch_id;
        assert(b.noteOn(60,100,now));r.apply(now++);assert(r.render());
        assert(b.setPatch(p.id,now));assert(b.state().patch_id==old);
        // USB events queued before transition are invalidated, never replayed.
        uint8_t usb[]={9,0x90,64,100};assert(r.usbInput().transfer(usb,4,now));
        r.apply(now++);assert(r.render());
        assert(!r.snapshot().active_notes && r.snapshot().patch_id==p.id);
        assert(b.state().patch_id==old);publish();assert(b.state().engine_patch==p.id);
        assert(b.noteOn(65,100,now));r.apply(now++);assert(r.render());
        assert(r.snapshot().active_notes==1);
        const auto generation=r.commands().generation();
        for(uint16_t bad:{uint16_t(4),uint16_t(8),uint16_t(256),uint16_t(391),uint16_t(65535)})
            assert(!b.setPatch(bad,now));
        assert(generation==r.commands().generation() && r.snapshot().active_notes==1);
        assert(r.commands().push({amy::CommandType::Patch,4,0,0,0,now}));
        r.apply(now++);assert(r.snapshot().patch_id==p.id && r.snapshot().active_notes==1);
        b.panic();r.apply(now++);const auto pcm=r.render();
        assert(!r.snapshot().active_notes);
        for(unsigned i=0;i<512;++i)assert(!pcm[i]);
    }
    assert(probe_bytes()==bytes && !r.snapshot().oom_count);
    probe_realtime(false);r.end();assert(probe_bytes()==baseline);
    std::puts("PASS canonical costs, four real resident banks, deferred confirmation, invalid-resource rejection, stale USB invalidation, Panic, no service heap");
}
