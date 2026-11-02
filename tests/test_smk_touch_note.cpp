#include "touch_note.h"
#include "smk_link.h"
#include <cassert>
#include <iostream>
using namespace smk::link;
struct Synth final : Backend {
    unsigned on=0,off=0,physical=1;
    uint8_t note=kNoNote;
    Error receive(const Frame& f) override {
        if(f.type==Type::AuditionNoteOn) { note=f.payload[0]; ++on; }
        if(f.type==Type::AuditionNoteOff) { note=kNoNote; ++off; }
        return Error::None;
    }
    void releaseAudition() override { note=kNoNote; ++off; }
    void snapshot(Frame& response) override { response.size=kSnapshotBytes; }
    bool patch(uint16_t,Frame&) override { return false; }
};
static bool transfer(Client& client,Server& server,uint32_t now_ms) {
    Frame transmit{};
    if(!client.next(now_ms,transmit)) return false;
    client.receive(server.handle(transmit,now_ms),now_ms);
    return true;
}
int main() {
    Synth synth; Server server(synth,1); Client client(7); smk::p4::TouchNote touch;
    assert(transfer(client,server,0) && client.connected());
    touch.press(60,10); touch.service(client,10,true); assert(transfer(client,server,10) && synth.note==60);
    touch.release(60); touch.service(client,11,true); assert(transfer(client,server,11) && synth.note==kNoNote);
    // Press and leave the key while another packet is in flight: never play it later.
    Frame pending{}; assert(client.next(220,pending));
    touch.press(64,221); touch.service(client,221,true); touch.release(64); touch.service(client,222,true);
    client.receive(server.handle(pending,223),223); touch.service(client,224,true);
    transfer(client,server,224); assert(synth.on==1 && synth.physical==1);
    // Release during an unacknowledged Note On retains a guaranteed Off.
    touch.press(67,230); touch.service(client,230,true); assert(client.next(230,pending));
    auto ack=server.handle(pending,230); assert(synth.note==67);
    touch.release(67); touch.service(client,231,true); client.receive(ack,232);
    assert(transfer(client,server,233) && synth.note==kNoNote);
    // Lost touch has a bounded maximum hold, independently of heartbeat traffic.
    touch.press(69,300); touch.service(client,300,true); transfer(client,server,300); assert(synth.note==69);
    for(uint32_t now=400;now<10300;now+=100) { touch.service(client,now,true); transfer(client,server,now); }
    assert(client.connected() && synth.note==69);
    touch.service(client,300+smk::p4::config::kTouchMaxHoldMs,true);
    transfer(client,server,10300); assert(synth.note==kNoNote && synth.physical==1);
    // Reconnection cannot replay the old held finger.
    for(uint32_t now=10310;now<11000 && !client.connected();now+=10) transfer(client,server,now);
    const unsigned previous_on=synth.on;
    touch.disconnect(); touch.service(client,11001,true); transfer(client,server,11001);
    assert(synth.on==previous_on);
    // Offline presses and an audio-fault transition cannot arm a future note.
    client.disconnect(); touch.press(72,12000); touch.service(client,12000,true);
    transfer(client,server,12000); touch.service(client,12001,true); transfer(client,server,12001);
    assert(synth.on==previous_on);
    touch.press(72,12010); touch.service(client,12010,false); transfer(client,server,12010);
    assert(synth.on==previous_on);
    std::cout<<"PASS: real touch gesture model, press/release/leave, busy and lost ACK, defensive timeout, offline/audio fault and no reconnect replay\n";
}
