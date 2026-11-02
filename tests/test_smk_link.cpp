#include "smk_link.h"
#include "smk_client.h"
#include "smk_state.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
using namespace smk::link;

struct Engine final : Backend {
    unsigned received=0,releases=0,panics=0,physical_notes=1;
    uint16_t patch_id=0;
    Error reject=Error::None;
    Error receive(const Frame& f) override {
        if(reject!=Error::None) return reject;
        ++received;
        if(f.type==Type::Panic) {++panics; physical_notes=0;}
        if(f.type==Type::SetPatch) patch_id=get16(f.payload);
        return Error::None;
    }
    void releaseAudition() override { ++releases; }
    void snapshot(Frame& r) override { r.size=kSnapshotBytes; put16(r.payload,patch_id); r.payload[29]=0x80; }
    bool patch(uint16_t id,Frame& r) override { r.size=28; put16(r.payload,id); return true; }
};
static Frame command(Type type,uint16_t sequence=0,uint32_t session=17) {
    Frame f{}; f.type=type; f.sequence=sequence; f.session=session; return f;
}
static void codec() {
    const uint8_t check[]={'1','2','3','4','5','6','7','8','9'};
    assert(crc16(check,sizeof(check))==0x29b1);
    Decoder decoder;
    Frame in=command(Type::StateSnapshot,0xc0db,0xdbc01122),out{};
    uint8_t wire[kWireBytes]{};
    for(size_t size=0;size<=kPayloadBytes;++size) {
        in.size=uint16_t(size);
        for(size_t i=0;i<size;++i) in.payload[i]=uint8_t(i%3==0?0xc0:i%3==1?0xdb:i);
        const size_t bytes=encode(in,wire,sizeof(wire)); assert(bytes);
        assert(encode(in,wire,bytes-1)==0);
        unsigned packets=0;
        for(size_t i=0;i<bytes;++i) packets += decoder.feed(wire[i],50,out)==Decoder::Result::Packet;
        assert(packets==1 && sameRequest(in,out));
    }
    auto frame=command(Type::SetMacro,1); frame.size=2; frame.payload[0]=3; frame.payload[1]=70;
    size_t size=encode(frame,wire,sizeof(wire));
    wire[4]^=1; // Protected type byte, without altering delimiters.
    unsigned packets=0;
    for(size_t i=0;i<size;++i) packets += decoder.feed(wire[i],51,out)==Decoder::Result::Packet;
    assert(packets==0 && decoder.errors()>0);
    size=encode(frame,wire,sizeof(wire));
    for(size_t i=0;i<size-2;++i) decoder.feed(wire[i],100,out);
    assert(decoder.expire(200) && decoder.timeouts()==1);
    assert(decoder.feed(wire[size-2],201,out)!=Decoder::Result::Packet);
    decoder.feed(0xc0,201,out);
    decoder.feed(0xc0,202,out);
    for(size_t i=0;i<kPacketBytes+200;++i) decoder.feed(0x55,202,out);
    assert(decoder.feed(0xc0,202,out)==Decoder::Result::Invalid);
    decoder.feed(0xdb,203,out); decoder.feed(0x00,203,out);
    assert(decoder.feed(0xc0,203,out)==Decoder::Result::Invalid);
    // Noise and invalid frames must not prevent the next intact frame.
    uint32_t seed=1;
    for(unsigned i=0;i<20000;++i) { seed=seed*1664525u+1013904223u; decoder.feed(uint8_t(seed>>24),300,out); }
    for(size_t i=0;i<size;++i) packets += decoder.feed(wire[i],301,out)==Decoder::Result::Packet;
    assert(packets==1 && sameRequest(frame,out));
    decoder.reset();
    for(size_t i=0;i<size-2;++i) decoder.feed(wire[i],UINT32_MAX-10,out);
    assert(decoder.expire(100)); // Clock wrap.
    frame.size=kPayloadBytes+1; assert(!encode(frame,wire,sizeof(wire)));
}
static void sessions() {
    Engine engine;
    Server server(engine,99);
    auto hello=command(Type::Hello);
    auto r=server.handle(hello,10);
    assert(r.type==Type::HelloAck && get32(r.payload)==99 && get16(r.payload+4)==1);
    auto on=command(Type::AuditionNoteOn,1); on.size=2; on.payload[0]=60; on.payload[1]=100;
    r=server.handle(on,20);
    assert(r.type==Type::CommandAck && r.payload[0]==uint8_t(Ack::Received));
    assert(engine.received==1 && server.auditionNote()==60);
    server.handle(on,30); assert(engine.received==1);
    auto changed=on; changed.payload[1]=99;
    assert(server.handle(changed,35).payload[0]==uint8_t(Error::Sequence));
    auto state=command(Type::GetState,2); state.size=1; state.payload[0]=60;
    r=server.handle(state,200); assert(r.type==Type::StateSnapshot && r.size==kSnapshotBytes);
    assert((r.payload[29]&0x80)!=0); // Explicit control/requested state, not APPLIED.
    server.tick(799); assert(engine.releases==0);
    server.tick(800); assert(engine.releases==1 && engine.physical_notes==1);
    server.tick(1700); assert(!server.connected() && engine.panics==0);
    hello.session=18; server.handle(hello,1800);
    assert(server.connected() && server.reconnects()==2 && engine.panics==0);
    assert(server.handle(on,1801).payload[0]==uint8_t(Error::Session));
    on.session=18; server.handle(on,1810);
    server.handle(hello,1820); assert(engine.releases==1); // HELLO retry is harmless.
    hello.session=19; server.handle(hello,1830);
    assert(engine.releases==2 && engine.physical_notes==1); // P4 reboot, no global Panic.
    on.session=19; server.handle(on,1840);
    auto off=command(Type::AuditionNoteOff,2,19); off.size=1; off.payload[0]=60;
    server.handle(off,1850); assert(server.auditionNote()==kNoNote && engine.received==4);
    server.handle(off,1855); assert(engine.received==4);
    auto panic=command(Type::Panic,3,19); server.handle(panic,1860); server.handle(panic,1861);
    assert(engine.panics==1);
    auto patch=command(Type::SetPatch,4,19); patch.size=2; put16(patch.payload,135);
    server.handle(patch,1870); assert(engine.patch_id==135);
    patch.sequence=6; assert(server.handle(patch,1871).payload[0]==uint8_t(Error::Sequence));
    patch.sequence=5; patch.version=2;
    assert(server.handle(patch,1872).payload[0]==uint8_t(Error::Version));
    patch.version=1; patch.size=1;
    assert(server.handle(patch,1873).payload[0]==uint8_t(Error::Payload));
    patch.size=2; engine.reject=Error::Busy;
    assert(server.handle(patch,1874).payload[0]==uint8_t(Error::Busy));
    engine.reject=Error::None;
    assert(server.handle(patch,1875).payload[0]==uint8_t(Error::Busy)); // Cached error; no replay.
    patch.sequence=6; assert(server.handle(patch,1876).type==Type::CommandAck);
    patch.sequence=7; patch.type=Type::GetPatch;
    r=server.handle(patch,1877); assert(r.type==Type::PatchState && get16(r.payload)==135);
    // Explicit release hint heals a lost Note Off before the connection timeout.
    on.session=19; on.sequence=8; server.handle(on,1880);
    state.session=19; state.sequence=9; state.payload[0]=kNoNote; server.handle(state,1885);
    assert(server.auditionNote()==kNoNote && engine.releases==3);
    // A duplicate note cannot renew its lease indefinitely.
    on.sequence=10; server.handle(on,2000); server.handle(on,2500); server.tick(2600);
    assert(server.auditionNote()==kNoNote && engine.releases==4);
    // Lost S3 state rejects commands until a new handshake.
    Engine restarted_engine; Server restarted(restarted_engine,100);
    assert(restarted.handle(patch,3000).payload[0]==uint8_t(Error::Session));
    r=restarted.handle(hello,3001); assert(get32(r.payload)==100);
    // Shape/range errors do not dispatch music.
    auto macro=command(Type::SetMacro,1,19); macro.size=2; macro.payload[0]=8;
    assert(!validCommand(macro)); macro.payload[0]=7; macro.payload[1]=128; assert(!validCommand(macro));
    assert(!validCommand(command(Type::SetMasterVolume)));
}
static void client() {
    Engine engine; Server server(engine,123); Client client(71);
    Frame tx{},rx{};
    assert(client.next(0,tx) && tx.type==Type::Hello);
    rx=server.handle(tx,1); client.receive(rx,2); assert(client.connected() && client.bootId()==123);
    const uint8_t note[]={64,100};
    assert(client.request(Type::AuditionNoteOn,note,2,10)); client.heldNote(64);
    assert(!client.request(Type::Panic,nullptr,0,10));
    assert(client.next(10,tx)); rx=server.handle(tx,11);
    // Drop ACK, release finger while the Note On is in flight.
    client.releaseNote(64);
    assert(!client.request(Type::SetMasterVolume,note,1,30));
    assert(client.next(100,tx) && tx.type==Type::Hello); // Release fences the unacknowledged session.
    client.receive(rx,101); assert(!client.connected()); // Stale Note On ACK cannot revive it.
    rx=server.handle(tx,102); client.receive(rx,103);
    assert(client.connected() && engine.received==1 && server.auditionNote()==kNoNote);
    client.panic(); assert(client.next(266,tx) && tx.type==Type::Panic);
    rx=server.handle(tx,267); client.receive(rx,268); assert(engine.panics==1);
    assert(client.next(500,tx) && tx.type==Type::GetState);
    rx=server.handle(tx,501); client.receive(rx,502); assert(client.takeResponse(rx));
    assert(rx.type==Type::StateSnapshot && !client.takeResponse(rx));
    // Lost S3 replies eventually reset the client and renew the session.
    const uint32_t old_session=tx.session;
    for(uint32_t time=700;time<2300;time+=100) client.next(time,tx);
    assert(!client.connected());
    assert(tx.session!=old_session);
    // An old-session response cannot reconnect it.
    client.receive(rx,2300); assert(!client.connected());
    for(uint32_t time=2300;time<3300 && !client.connected();time+=10) {
        if(client.next(time,tx)) { rx=server.handle(tx,time); client.receive(rx,time); }
    }
    assert(client.connected());
    // Malformed ACK must leave the immutable command pending for a real ACK.
    assert(client.request(Type::AuditionNoteOn,note,2,3400)); assert(client.next(3400,tx));
    Frame telemetry{}; telemetry.type=Type::AudioDiagnostics; telemetry.session=tx.session; telemetry.size=kDiagnosticsBytes;
    client.receive(telemetry,3400); assert(client.busy() && client.takeResponse(rx) && rx.type==Type::AudioDiagnostics);
    rx=server.handle(tx,3401); auto bad=rx; bad.size=0; client.receive(bad,3402);
    assert(client.busy()); client.receive(rx,3403); assert(!client.busy());
    // Protocol version incompatibility is visible and has bounded retries.
    Client incompatible(90); assert(incompatible.next(0,tx));
    rx={}; rx.type=Type::CommandError; rx.session=tx.session; rx.size=4; rx.payload[0]=uint8_t(Error::Version);
    incompatible.receive(rx,1); assert(incompatible.incompatible() && !incompatible.connected());
    assert(!incompatible.next(100,tx)); assert(incompatible.next(2000,tx));
}
static void snapshots() {
    State source{}; source.patch_id=135; source.engine_patch=258; source.master=91;
    source.bpm_centi=12345; source.sample_rate_hz=48000; source.block_frames=256;
    source.max_render_us=0x12345678; source.deadline_misses=7; source.pcm_starvations=8;
    source.event_overflows=9; source.commands_dropped=10; source.free_internal=1111;
    source.free_psram=2222; source.active_notes=3; source.midi_events=4000000;
    std::memset(source.patch_name,'P',24);
    for(unsigned i=0;i<8;++i) { std::memset(source.macros[i].name,'A'+i,8); source.macros[i].value=uint8_t(i*17); source.macros[i].supported=i%2!=0; }
    Frame frame{}; writeState(frame,source); State decoded{};
    assert(frame.size==154 && readState(frame,decoded));
    assert(decoded.patch_id==135 && decoded.engine_patch==258 && decoded.master==91);
    assert(decoded.patch_name[24]==0 && decoded.patch_name[23]=='P');
    assert(decoded.bpm_centi==12345 && decoded.sample_rate_hz==48000 && decoded.block_frames==256);
    assert(decoded.max_render_us==0x12345678 && decoded.deadline_misses==7 && decoded.pcm_starvations==8);
    assert(decoded.event_overflows==9 && decoded.commands_dropped==10 && decoded.free_internal==1111 && decoded.free_psram==2222);
    assert(decoded.active_notes==3 && decoded.midi_events==4000000 && decoded.flags==kControlState);
    for(unsigned i=0;i<8;++i) assert(decoded.macros[i].name[8]==0 && decoded.macros[i].name[0]==char('A'+i) &&
                                  decoded.macros[i].value==i*17 && decoded.macros[i].supported==(i%2!=0));
    frame.payload[82]=128; assert(!readState(frame,decoded));
    assert(decoded.patch_id==135); // Invalid external state cannot partially overwrite the current view.
    frame.payload[82]=0; frame.payload[83]=2; assert(!readState(frame,decoded));
    --frame.size; assert(!readState(frame,decoded));
    // Sequence rollover requires a fresh handshake, never aliases an old note.
    Engine engine; Server server(engine,1); server.handle(command(Type::Hello),0);
    for(uint32_t sequence=1;sequence<=UINT16_MAX;++sequence) {
        auto request=command(Type::GetState,uint16_t(sequence)); request.size=1; request.payload[0]=kNoNote;
        assert(server.handle(request,sequence).type==Type::StateSnapshot);
    }
    auto wrapped=command(Type::Panic,0);
    assert(server.handle(wrapped,65536).payload[0]==uint8_t(Error::Sequence));
    assert(engine.panics==0);
}
int main() { codec(); sessions(); client(); snapshots(); std::cout<<"PASS: SLIP/CRC/bounds/timeout/resync, sessions/sequence/duplicates/wrap, explicit snapshot, RECEIVED, audition lease, Panic, client retries and reconnect isolation\n"; }
