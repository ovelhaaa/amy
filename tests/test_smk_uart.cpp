#include "smk_uart.h"
#include <cassert>
#include <cstdio>
using namespace smk::link;
int main() {
    Uart uart; assert(uart.begin(1,4,5)==ESP_OK);
    Frame old{}; old.type=Type::SetPatch; old.session=1; old.sequence=1;
    old.size=2; old.payload[0]=99;
    for(unsigned i=0;i<kTxSlots-1;++i) assert(uart.send(old));
    assert(!uart.send(old)); // Ordinary traffic cannot consume release reserve.
    Frame off=old; off.type=Type::AuditionNoteOff; off.size=1; off.payload[0]=60;
    assert(uart.send(off,true)); assert(!uart.send(off,true));
    Frame ignored{};
    for(unsigned i=0;i<100;++i) assert(!uart.poll(i,ignored));
    assert(mock_uart::wire.empty()); // Full hardware FIFO never blocks.
    mock_uart::allowance=3;
    uart.poll(100,ignored); assert(mock_uart::wire.size()==3); // Partial stale frame.
    const auto drops=uart.transmitDrops(); uart.discardPendingTx();
    assert(uart.transmitDrops()==drops+kTxSlots);
    Frame hello{}; hello.type=Type::Hello; hello.session=2;
    assert(uart.send(hello)); mock_uart::allowance=64;
    for(unsigned i=101;i<120;++i) uart.poll(i,ignored);
    Decoder decoder; unsigned packets=0;
    for(auto byte:mock_uart::wire) if(decoder.feed(byte,120,ignored)==Decoder::Result::Packet) {
        ++packets; assert(ignored.type==Type::Hello && ignored.session==2);
    }
    assert(packets==1); // Leading HELLO delimiter heals partial old transmission.
    std::puts("PASS: real UART TX congestion, release reserve, nonblocking service and stale-session purge/resync");
}
