#include "amy_runtime.h"
#include <cassert>
#include <cstdio>
extern "C" { void probe_realtime(bool); uint32_t probe_calls(); size_t probe_bytes(); }
using namespace smk::p4;
void testHarmonicPadOwner() {
    amy::Runtime r;assert(r.begin());
    uint32_t now=100;
    auto beat=[&] {r.commands().pad_heartbeat_ms.store(now);r.apply(now);};
    auto on=[&](unsigned pad,pads::Config config={},int input=-1) {
        const auto intent=pads::resolve(pad,config);assert(intent.valid);
        amy::Command c{};c.type=amy::CommandType::PadOn;c.channel=uint8_t(pad);
        c.id=uint16_t(input>=0?input:intent.input);c.value=100;c.token=r.commands().nextToken();
        c.timestamp_ms=now;c.pad_config=harmony::pack(intent.configuration);
        assert(r.commands().push(c));beat();return c.token;
    };
    auto off=[&](unsigned pad,uint32_t token){r.commands().releasePad(pad,token);beat();};
    pads::Config seventh;seventh.seventh=true;
    auto a=on(0,seventh);auto b=on(1,seventh);assert(r.snapshot().active_notes==8);on(2);assert(r.snapshot().active_notes==8);off(0,a);off(1,b);
    a=on(0);b=on(1,{},60);assert(r.snapshot().active_notes==6);off(0,a);assert(r.snapshot().active_notes==3);off(1,b);
    a=on(0);b=on(2);assert(r.snapshot().active_notes==6);
    auto rejected=on(4);assert(r.snapshot().active_notes==6);
    assert(r.snapshot().pads.pads[4].token==rejected);
    assert(r.snapshot().pads.pads[4].result==pads::Result::Capacity);
    const auto newer_rejection=on(4);
    amy::Command replay{};replay.type=amy::CommandType::PadOn;replay.channel=4;replay.id=pads::resolve(4,{}).input;
    replay.value=100;replay.token=rejected;replay.timestamp_ms=now;replay.pad_config=harmony::pack(pads::resolve(4,{}).configuration);
    assert(r.commands().push(replay));beat();assert(r.snapshot().pads.pads[4].token==newer_rejection);
    off(0,a);assert(r.snapshot().active_notes==3);off(2,b);assert(!r.snapshot().active_notes);
    // Shared pitches never imply shared ownership. Old Off cannot release a new token.
    a=on(0);off(0,a);b=on(0);off(0,a);assert(r.snapshot().active_notes==3);
    r.usbInput().disconnect();beat();assert(r.snapshot().active_notes==3);
    // Config is frozen; a later global Harmony update cannot transform an active pad.
    amy::Command h{};h.type=amy::CommandType::Harmony;h.token=harmony::pack(harmony::preset(3));
    assert(r.commands().push(h));beat();assert(r.snapshot().active_notes==3);
    assert(r.snapshot().pads.pads[0].notes[0]==60);
    off(0,b);assert(!r.snapshot().active_notes);
    // Global Assist is enabled, but a triad pad remains exactly three voices.
    a=on(0);assert(r.snapshot().active_notes==3);off(0,a);
    // USB and keyboard Touch own their same-pitch gates independently.
    h.token=harmony::pack(harmony::Config{});assert(r.commands().push(h));beat();
    amy::Command touch{};touch.type=amy::CommandType::NoteOn;touch.id=60;touch.value=100;
    touch.token=r.commands().nextToken();touch.timestamp_ms=now;assert(r.commands().push(touch));
    const uint8_t usbOn[]={0x09,0x90,60,100};assert(r.usbInput().transfer(usbOn,4,now));beat();
    a=on(0);assert(r.snapshot().active_notes==5);off(0,a);assert(r.snapshot().active_notes==2);
    r.commands().release(60,touch.token);beat();assert(r.snapshot().active_notes==1);
    r.usbInput().disconnect();beat();assert(!r.snapshot().active_notes);
    // Queued On canceled before processing must not resurrect after UI teardown.
    amy::Command c{};c.type=amy::CommandType::PadOn;c.channel=0;c.id=60;c.value=100;
    c.token=r.commands().nextToken();c.timestamp_ms=now;
    c.pad_config=harmony::pack(pads::resolve(0,{}).configuration);
    assert(r.commands().push(c));off(0,c.token);assert(!r.snapshot().active_notes);
    // Existing queue capacity is retained; Off latches work even when full.
    a=on(0);
    for(unsigned i=0;i<amy::kCommandCapacity;++i)assert(r.commands().push({amy::CommandType::Master,0,64}));
    assert(!r.commands().push({amy::CommandType::Master,0,64}));off(0,a);assert(!r.snapshot().active_notes);
    // Long HOLD survives with heartbeat; a dead UI session expires on owner.
    a=on(0);now+=11000;beat();assert(r.snapshot().active_notes==3);
    now+=pads::kLeaseMs;r.apply(now);assert(!r.snapshot().active_notes);
    assert(r.snapshot().pads.pads[0].result==pads::Result::Expired);
    a=on(0);r.commands().panic();beat();assert(!r.snapshot().active_notes);
    off(0,a);assert(!r.snapshot().active_notes);
    assert(r.render());
    const auto heap=probe_bytes();const auto calls=probe_calls();probe_realtime(true);
    for(unsigned i=0;i<100;++i){a=on(0);b=on(1);on(2);assert(r.snapshot().active_notes==6);assert(r.render());off(0,a);off(1,b);assert(r.render());assert(!r.snapshot().active_notes);}
    probe_realtime(false);assert(probe_bytes()==heap && probe_calls()==calls);
    now=UINT32_MAX-100;a=on(0);now+=200;beat();assert(r.snapshot().active_notes==3);off(0,a);
    a=on(0);
    assert(r.commands().push({amy::CommandType::Patch,135}));beat();r.loadPendingPatch();
    assert(r.snapshot().patch_state==amy::PatchState::Ready && !r.snapshot().active_notes);
    b=on(0);off(0,a);assert(r.snapshot().active_notes==3);off(0,b);
    // Session loss cannot be repaired by replaying a command queued before expiry.
    now+=1000;a=on(0);
    c.channel=1;c.id=62;c.token=r.commands().nextToken();c.timestamp_ms=now+1;
    assert(r.commands().push(c));
    now+=pads::kLeaseMs;r.apply(now);assert(!r.snapshot().active_notes);
    beat();assert(!r.snapshot().active_notes); // new heartbeat alone does not resurrect
    b=on(0);assert(r.snapshot().active_notes==3);off(0,a);assert(r.snapshot().active_notes==3);off(0,b);
    // Older/replayed operations cannot replace the last result after a group was released.
    c.channel=0;c.id=60;c.token=a;c.timestamp_ms=now;
    assert(r.commands().push(c));beat();assert(!r.snapshot().active_notes);
    assert(r.snapshot().pads.pads[0].token==b);
    // Malformed musical configuration must return Invalid for its own token, without any gates.
    c.token=r.commands().nextToken();c.pad_config=0;c.timestamp_ms=now;
    assert(r.commands().push(c));beat();assert(!r.snapshot().active_notes);
    assert(r.snapshot().pads.pads[0].token==c.token);
    assert(r.snapshot().pads.pads[0].result==pads::Result::Invalid);
    a=on(0);const auto active_status=r.snapshot().pads.pads[0];
    c.token=r.commands().nextToken();assert(r.commands().push(c));beat();
    assert(r.snapshot().active_notes==3 && r.snapshot().pads.pads[0].active_token==active_status.active_token);
    off(0,a);
    now=UINT32_MAX-100;a=on(0);now+=pads::kLeaseMs;r.apply(now);
    assert(!r.snapshot().active_notes && r.snapshot().pads.pads[0].result==pads::Result::Expired);
    beat();assert(!r.snapshot().active_notes);
    c.pad_config=harmony::pack(pads::resolve(0,{}).configuration);c.token=r.commands().nextToken();c.timestamp_ms=now;
    const auto old_generation=r.commands().generation();assert(r.commands().push(c));
    r.commands().panic();beat();assert(!r.snapshot().active_notes);
    assert(r.commands().pushForGeneration(c,old_generation));beat();assert(!r.snapshot().active_notes);
    std::printf("PAD TIMING transform_max_us=%u admission_max_us=%u musical_heap_delta=0\n",r.snapshot().transform_max_us,r.snapshot().admission_max_us);
    std::printf("PASS pads owner; command=%zu commands=%zu runtime=%zu snapshot=%zu\n",sizeof(amy::Command),sizeof(amy::Commands),sizeof(r),sizeof(amy::Snapshot));
    r.end();
}
