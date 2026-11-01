#include "usb_midi_host.h"
#include "usb_midi_descriptor.h"
#include "diagnostics.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <chrono>
#include <thread>
namespace smk {
Diagnostics& Diagnostics::instance() { static Diagnostics value; return value; }
DiagnosticCounters& Diagnostics::counters() { return counters_; }
}
using namespace smk;
static void waitState(UsbMidiHost& host, UsbMidiHost::State state) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (host.state() != state && std::chrono::steady_clock::now() < end)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(host.state() == state);
}
static void descriptors() {
    MidiEndpoint endpoint;
    const std::vector<uint8_t> valid(std::begin(mock_usb::descriptor), std::end(mock_usb::descriptor));
    assert(findMidiEndpoint(valid.data(), valid.size(), endpoint));
    assert(endpoint.address == 0x81 && endpoint.packet_bytes == 64);
    // Every accessible prefix, including a header claiming more than available.
    for (size_t length = 0; length < valid.size(); ++length)
        assert(!findMidiEndpoint(valid.data(), length, endpoint));
    for (int offset : {0, 9, 18}) {
        for (int length : {0, 1, 255}) {
            auto data = valid; data[offset] = length;
            assert(!findMidiEndpoint(data.data(), data.size(), endpoint));
        }
    }
    for (auto change : {std::pair<int,int>{14,255}, {12,1}, {21,3}, {20,0x80}, {22,0}, {22,65}}) {
        auto data = valid; data[change.first] = change.second;
        assert(!findMidiEndpoint(data.data(), data.size(), endpoint));
    }
    auto no_endpoint = valid; no_endpoint.resize(18); no_endpoint[2] = 18;
    assert(!findMidiEndpoint(no_endpoint.data(), no_endpoint.size(), endpoint));
    auto trailing = valid; trailing.push_back(7); trailing[2] = trailing.size();
    assert(!findMidiEndpoint(trailing.data(), trailing.size(), endpoint));
    // Exhaustive one-byte corruption sweep checks termination and span safety.
    for (size_t i = 0; i < valid.size(); ++i) for (int byte = 0; byte < 256; ++byte) {
        auto data = valid; data[i] = byte;
        findMidiEndpoint(data.data(), data.size(), endpoint);
    }
}
static void lifecycle() {
    mock_task::async = true;
    for (int fault = 0; fault < 8; ++fault) {
        mock_usb::reset();
        EventBus bus;
        UsbMidiHost host(bus);
        mock_usb::fail_install = fault == 0;
        mock_usb::fail_register = fault == 1;
        mock_task::fail_name = fault == 2 ? "usb_host" : fault == 3 ? "midi_client" : "";
        mock_usb::fail_alloc = fault == 4;
        mock_usb::fail_submit = fault == 5;
        mock_usb::fail_claim = fault == 6;
        mock_usb::fail_descriptor = fault == 7;
        if (fault < 4) assert(!host.begin());
        else {
            assert(host.begin()); mock_usb::event(1);
            // Wait for cleanup following an attempted connection, not initial DISCONNECTED.
            const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            SynthEvent event{};
            while (!bus.tryReceive(event) && std::chrono::steady_clock::now() < end)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            assert(event.type == EventType::Panic);
            assert(!host.isDeviceConnected() && host.reconnectCount() == 0);
        }
        host.stop(); mock_task::join();
        assert(mock_usb::allocations == mock_usb::frees);
        assert(!mock_usb::installed && !mock_usb::opened && !mock_usb::registered);
    }
    mock_task::fail_name.clear();
    for (int failure : {2,3,4,5,6,7}) {
        mock_usb::reset();
        EventBus bus(4);
        mock_usb::fail_free_once = mock_usb::fail_release_once = mock_usb::fail_close_once = true;
        UsbMidiHost host(bus);
        assert(host.begin());
        mock_usb::event(1); waitState(host, UsbMidiHost::State::Ready);
        SynthEvent event{};
        mock_usb::event(8);
        const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        bool note_on = false, note_off = false;
        while (!note_off && std::chrono::steady_clock::now() < end) {
            if (bus.tryReceive(event)) {
                if (event.type == EventType::NoteOn) note_on = true;
                if (event.type == EventType::NoteOff) { assert(note_on); note_off = true; }
            } else std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        assert(note_on && note_off);
        // Saturate every slot with releases: disconnect still latches Panic.
        for (int i=0; i<4; ++i) assert(bus.send({EventType::NoteOff, EventSource::UsbMidi,0,60,0,0}));
        mock_usb::event(failure);
        waitState(host, UsbMidiHost::State::Disconnected);
        assert(bus.tryReceive(event) && event.type == EventType::Panic);
        bus.acknowledgePanic(); while (bus.tryReceive(event)) {}
        // Clear injected submit error through owner task by stopping/restarting.
        if (failure != 4) {
            mock_usb::event(1); waitState(host, UsbMidiHost::State::Ready);
            assert(host.reconnectCount() == 2);
        }
        host.stop(); mock_task::join();
        assert(mock_usb::allocations == mock_usb::frees);
        assert(!mock_usb::opened && !mock_usb::registered && !mock_usb::installed);
    }
    mock_usb::reset();
    {
        EventBus bus;
        UsbMidiHost host(bus);
        assert(host.begin());
        mock_usb::event(1); waitState(host, UsbMidiHost::State::Ready);
        host.stop(); mock_task::join();
        assert(host.begin() && host.state() == UsbMidiHost::State::Disconnected);
        mock_usb::event(1); waitState(host, UsbMidiHost::State::Ready);
        host.stop(); mock_task::join();
        assert(mock_usb::allocations == mock_usb::frees);
    }
    mock_task::async = false;
}
int main() {
    descriptors(); lifecycle();
    std::puts("PASS: USB descriptor bounds, class/endpoint filtering, task/register/alloc/submit faults, disconnect flush/join/reconnect and saturated Panic");
}
