#include "usb_midi_host.h"
#include "usb_midi_descriptor.h"
#include "descriptor.h"
#include "esp_timer.h"
#include <cassert>
#include <cstdio>
using namespace smk::p4;
static void connect(usb::Host& h) { mock_usb::event(USB_HOST_CLIENT_EVENT_NEW_DEV); h.service(); }
static void close(usb::Host& h) { for(unsigned i=0;i<100 && !h.shutdown();++i) {} assert(!mock_usb::installed); }
int main() {
    amy::UsbInput input;
    input.enableTrace();
    // Same checked descriptor predicate as S3, explicit FS-only/alt0 policy.
    smk::MidiEndpoint ep;
    assert(smk::findMidiEndpoint(mock_usb::descriptor,sizeof(mock_usb::descriptor),ep));
    assert(ep.packet_bytes==64 && ep.address==0x81);
    usb::Endpoint p4ep;
    mock_usb::descriptor[12]=1;
    assert(usb::findEndpoint(mock_usb::descriptor,sizeof(mock_usb::descriptor),p4ep));
    assert(p4ep.alternate==1);
    mock_usb::descriptor[22]=0; mock_usb::descriptor[23]=2;
    assert(usb::findEndpoint(mock_usb::descriptor,sizeof(mock_usb::descriptor),p4ep));
    assert(p4ep.packet_bytes==512);
    mock_usb::descriptor[12]=0; mock_usb::descriptor[22]=64; mock_usb::descriptor[23]=0;
    for(size_t size=0;size<sizeof(mock_usb::descriptor);++size)
        assert(!usb::findEndpoint(mock_usb::descriptor,size,p4ep));
    for(unsigned index:{0,9,18}) {
        auto saved=mock_usb::descriptor[index]; mock_usb::descriptor[index]=1;
        assert(!smk::findMidiEndpoint(mock_usb::descriptor,sizeof(mock_usb::descriptor),ep));
        mock_usb::descriptor[index]=saved;
    }
    for(unsigned index:{12,14,15,20,21,22}) {
        auto saved=mock_usb::descriptor[index]; mock_usb::descriptor[index]=index==20?1:0xff;
        assert(!smk::findMidiEndpoint(mock_usb::descriptor,sizeof(mock_usb::descriptor),ep));
        mock_usb::descriptor[index]=saved;
    }
    for(unsigned controller:{0,1}) {
        mock_usb::reset(); usb::Host h(input); assert(h.initialize(controller));
        mock_usb::descriptor[12]=controller?1:0;
        assert(selected_controller==(1U<<controller)); connect(h);
        assert(claimed_alt==int(controller));
        assert(h.state()==usb::Host::State::Connected);
        auto generation=input.commands.generation();
        mock_usb::event(9); h.service(); assert(input.events>0);
        amy::UsbInput::Trace trace; assert(input.popTrace(trace));
        assert(trace.packet[1]==0x90 && trace.packet[2]==60 && trace.packet[3]==100);
        mock_usb::event(USB_HOST_CLIENT_EVENT_DEV_GONE); h.service();
        assert(input.commands.generation()!=generation && h.disconnects()==1);
        // Reconnect arrives while the old canceled callback is still outstanding.
        mock_usb::event(USB_HOST_CLIENT_EVENT_NEW_DEV);
        for(unsigned i=0;i<4;++i) h.service();
        assert(h.state()==usb::Host::State::Connected && h.connects()==2);
        close(h); assert(mock_usb::allocations==mock_usb::frees);
        mock_usb::descriptor[12]=0;
    }
    // Physical removal can complete the transfer before or after DEV_GONE.
    for(bool callback_first:{false,true}) {
        mock_usb::reset(); usb::Host detached(input); assert(detached.initialize(0)); connect(detached);
        const auto generation=input.commands.generation();
        mock_usb::event(callback_first?10:USB_HOST_CLIENT_EVENT_DEV_GONE);
        detached.service();
        mock_usb::event(callback_first?USB_HOST_CLIENT_EVENT_DEV_GONE:10);
        detached.service();
        assert(detached.disconnects()==1 && detached.errors()==0);
        assert(input.commands.generation()==generation+1);
        mock_time::us+=2000000; detached.service();
        assert(detached.state()==usb::Host::State::Disconnected && detached.connects()==1);
        connect(detached);
        assert(detached.connects()==2 && detached.state()==usb::Host::State::Connected);
        close(detached); assert(mock_usb::allocations==mock_usb::frees);
    }
    // Actual HS trace: ERROR arrives before DEV_GONE. Keep the open handle
    // subscribed while canceling notes immediately, across repeated cycles.
    mock_usb::reset();
    {
        usb::Host removed(input); assert(removed.initialize(0)); connect(removed);
        for(unsigned cycle=0;cycle<3;++cycle) {
            const auto generation=input.commands.generation();
            mock_usb::event(11); removed.service();
            assert(input.commands.generation()==generation+1 && mock_usb::opened);
            assert(removed.errors()==0 && !mock_usb::flight);
            mock_time::us+=10000; removed.service();
            assert(mock_usb::opened); // no early close before delayed DEV_GONE
            mock_usb::event(USB_HOST_CLIENT_EVENT_DEV_GONE); removed.service();
            assert(input.commands.generation()==generation+1);
            mock_time::us+=2000000; removed.service();
            assert(removed.disconnects()==cycle+1 && removed.errors()==0);
            assert(removed.connects()==cycle+1 && !mock_usb::opened);
            connect(removed); assert(removed.connects()==cycle+2);
        }
        close(removed); assert(mock_usb::allocations==mock_usb::frees);
    }
    // A bus error without DEV_GONE remains a real fault, with bounded recovery.
    mock_usb::reset();
    {
        usb::Host faulted(input); assert(faulted.initialize(0)); connect(faulted);
        const auto generation=input.commands.generation();
        mock_usb::event(11); faulted.service();
        mock_time::us+=(usb::kRemovalGraceMs-1)*1000; faulted.service();
        assert(faulted.errors()==0 && mock_usb::opened);
        mock_time::us+=1000; faulted.service();
        assert(faulted.errors()==1 && !mock_usb::opened);
        assert(input.commands.generation()==generation+1);
        mock_time::us+=usb::kRetryMs*1000; faulted.service();
        assert(faulted.connects()==2);
        close(faulted); assert(mock_usb::allocations==mock_usb::frees);
    }
    // Every failure retains resources until callbacks and dependent APIs finish.
    mock_usb::reset();
    {
        usb::Host stopping(input); assert(stopping.initialize(0)); connect(stopping);
        mock_usb::event(11); stopping.service();
        close(stopping); // shutdown must not wait for the removal grace
        assert(mock_usb::allocations==mock_usb::frees);
        assert(stopping.initialize(0)); connect(stopping);
        assert(stopping.state()==usb::Host::State::Connected);
        close(stopping);
    }
    for(unsigned fault=0;fault<10;++fault) {
        mock_usb::reset(); usb::Host h(input);
        if(fault==0) mock_usb::fail_install=true;
        if(fault==1) mock_usb::fail_register=true;
        if(fault==2) mock_usb::fail_descriptor=true;
        if(fault==3) mock_usb::fail_claim=true;
        if(fault==4) mock_usb::fail_alloc=true;
        if(fault==5) mock_usb::fail_submit=true;
        const bool init=h.initialize(0);
        if(fault<2) assert(!init); else { assert(init); connect(h); }
        if(fault>=6) { mock_usb::event(fault==6?3:fault==7?7:fault==8?5:4); h.service(); }
        if(fault>=2) assert(h.state()==usb::Host::State::Error);
        mock_usb::fail_free_once=mock_usb::fail_release_once=mock_usb::fail_close_once=true;
        close(h); assert(mock_usb::allocations==mock_usb::frees);
    }
    mock_usb::reset(); usb::Host h(input); assert(h.initialize(0)); connect(h);
    auto epoch=input.commands.generation();
    mock_usb::event(7); h.service(); assert(input.commands.generation()!=epoch);
    for(unsigned i=0;i<4;++i) h.service();
    assert(h.state()==usb::Host::State::Error && !mock_usb::opened);
    mock_time::us+=1001000; h.service();
    assert(h.state()==usb::Host::State::Connected && h.connects()==2);
    close(h); assert(mock_usb::allocations==mock_usb::frees);
    mock_usb::reset();
    {
        usb::Host failed(input); mock_task::create_result=pdFALSE;
        assert(!failed.begin(0) && failed.state()==usb::Host::State::Error);
        mock_task::create_result=pdPASS;
    }
    mock_usb::reset(); mock_task::async=true;
    {
        usb::Host threaded(input); assert(threaded.begin(0));
        mock_usb::event(USB_HOST_CLIENT_EVENT_NEW_DEV);
        for(unsigned i=0;i<1000 && threaded.state()!=usb::Host::State::Connected;++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        assert(threaded.state()==usb::Host::State::Connected);
        threaded.stop(); mock_task::join();
        assert(!mock_usb::installed && mock_usb::allocations==mock_usb::frees);
    }
    mock_usb::reset(); mock_usb::fail_register=true;
    {
        usb::Host failed(input); assert(!failed.begin(0)); mock_task::join();
        assert(!mock_usb::installed && failed.state()==usb::Host::State::Error);
    }
    mock_task::async=false;
    std::puts("PASS: P4 USB controller, descriptors, enumeration, RX, failure drain, disconnect/reconnect while in-flight, cleanup retries and shutdown (mock IDF)");
}
