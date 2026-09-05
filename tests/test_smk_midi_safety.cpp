#include "event_bus.h"
#include "midi_parser.h"
#include "midi_learn.h"
#include "diagnostics.h"
#include <cassert>
#include <cstdio>
#include <thread>
#include <vector>

namespace smk {
Diagnostics& Diagnostics::instance() { static Diagnostics instance; return instance; }
DiagnosticCounters& Diagnostics::counters() { return counters_; }
}
using namespace smk;

static SynthEvent event(EventType type, uint16_t id = 60, int32_t value = 100) {
    return {type, EventSource::UsbMidi, 0, id, value, 1234};
}
static std::vector<SynthEvent> parsed;
static void capture(const SynthEvent& value, void*) { parsed.push_back(value); }

static void testParser() {
    MidiParser parser;
    parser.setCallback(capture, nullptr);
    const uint8_t maximum[] = {0x0e, 0xe0, 127, 127};
    const uint8_t minimum_other_channel[] = {0x0e, 0xe1, 0, 0};
    const uint8_t center[] = {0x0e, 0xe0, 0, 64};
    parser.processUsbMidiPacket(maximum);
    parser.processUsbMidiPacket(minimum_other_channel);
    parser.processUsbMidiPacket(center);
    assert(parsed[0].value == 8191 && parsed[0].channel == 0);
    assert(parsed[1].value == -8192 && parsed[1].channel == 1);
    assert(parsed[2].value == 0 && parsed[2].channel == 0);
    parser.processByte(0xe0); parser.processByte(0); parser.processByte(64);
    assert(parsed.back().value == 0);
    for (int curve = 0; curve < 5; ++curve) {
        parser.setVelocityCurve(static_cast<VelocityCurve>(curve));
        const uint8_t zero[] = {0x09, 0x90, 60, 0};
        const uint8_t soft[] = {0x09, 0x90, 60, 1};
        parser.processUsbMidiPacket(zero);
        assert(parsed.back().type == EventType::NoteOff);
        parser.processUsbMidiPacket(soft);
        assert(parsed.back().type == EventType::NoteOn && parsed.back().value > 0);
    }
}

static void testQueue() {
    EventBus bus(4); // 3 ordinary slots, 1 reserved release slot.
    SynthEvent received{};
    assert(bus.ready());
    assert(bus.send(event(EventType::ControlChange, 21)));
    assert(bus.send(event(EventType::NoteOn)));
    assert(bus.send(event(EventType::ControlChange, 22)));
    assert(!bus.send(event(EventType::NoteOn, 61)));
    assert(bus.send(event(EventType::NoteOff, 60, 0)));
    assert(bus.tryReceive(received) && received.id == 21);
    assert(bus.tryReceive(received) && received.type == EventType::NoteOn);
    assert(bus.tryReceive(received) && received.id == 22);
    assert(bus.tryReceive(received) && received.type == EventType::NoteOff);
    assert(!bus.tryReceive(received));
    assert(bus.overflowCount() == 1);

    // A full queue of releases cannot reject Panic, including the ISR path.
    for (int i = 0; i < 4; ++i) assert(bus.send(event(EventType::NoteOff, i, 0)));
    BaseType_t woken = pdFALSE;
    assert(bus.sendFromISR(event(EventType::Panic), &woken));
    assert(bus.tryReceive(received) && received.type == EventType::Panic);
    assert(!bus.send(event(EventType::NoteOn))); // Gate until producer reset.
    bus.acknowledgePanic();
    assert(!bus.tryReceive(received)); // Old releases invalidated too.
    assert(bus.send(event(EventType::NoteOn, 62)));
    assert(bus.tryReceive(received) && received.id == 62);

    // Exhausted release reserve escalates to Panic instead of losing Note Off.
    for (int i = 0; i < 4; ++i) assert(bus.send(event(EventType::NoteOff, i, 0)));
    assert(!bus.sendFromISR(event(EventType::ControlChange, 64, 0), &woken));
    assert(bus.tryReceive(received) && received.type == EventType::Panic);
    // A second request cannot be erased by acknowledgement of the first.
    assert(bus.send(event(EventType::Panic)));
    bus.acknowledgePanic();
    assert(!bus.send(event(EventType::NoteOn)));
    assert(bus.tryReceive(received) && received.type == EventType::Panic);
    bus.acknowledgePanic();
    assert(!bus.tryReceive(received));

    // Lifecycle survives reset; queued notes and clocks cannot restart playback.
    assert(bus.send(event(EventType::UsbConnect)));
    assert(bus.send(event(EventType::NoteOn)));
    assert(bus.send(event(EventType::Clock)));
    assert(bus.send(event(EventType::UsbDisconnect)));
    assert(bus.tryReceive(received) && received.type == EventType::Panic);
    bus.acknowledgePanic();
    assert(bus.tryReceive(received) && received.type == EventType::UsbConnect);
    assert(bus.tryReceive(received) && received.type == EventType::UsbDisconnect);
    assert(!bus.tryReceive(received));

    EventBus unavailable(0);
    assert(!unavailable.ready() && !unavailable.send(event(EventType::NoteOn)));
}

static void testWakeAndConcurrentProducers() {
    EventBus bus(512);
    std::thread producer([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        assert(bus.send(event(EventType::Panic)));
    });
    SynthEvent received{};
    assert(bus.receive(received, 500) && received.type == EventType::Panic);
    producer.join();
    bus.acknowledgePanic();
    auto produce = [&](int channel) {
        for (int i = 0; i < 100; ++i) {
            auto note = event(EventType::NoteOn, i);
            note.channel = channel;
            assert(bus.send(note));
            note.type = EventType::NoteOff;
            assert(bus.send(note));
        }
    };
    std::thread a(produce, 0), b(produce, 1);
    a.join(); b.join();
    bool active[2][100]{};
    int count = 0;
    while (bus.tryReceive(received)) {
        bool& held = active[received.channel][received.id];
        if (received.type == EventType::NoteOn) { assert(!held); held = true; }
        else { assert(held); held = false; }
        ++count;
    }
    assert(count == 400 && bus.overflowCount() == 0);
    for (auto& channel : active) for (bool held : channel) assert(!held);
}

static void testLearn() {
    auto profile = ProfileManager::createDefaultSmk25Profile();
    MidiLearn learn;
    learn.begin(&profile);
    learn.startWizard();
    for (auto type : {EventType::Panic, EventType::NoteOff, EventType::AllNotesOff,
                      EventType::UsbDisconnect, EventType::UsbConnect, EventType::TransportStop}) {
        const auto safety = event(type);
        assert(bypassMidiLearn(safety));
        assert(!learn.processEvent(safety));
        assert(learn.currentStep() == LearnStep::PressKey);
    }
    assert(bypassMidiLearn(event(EventType::ControlChange, 64, 0)));
    assert(!bypassMidiLearn(event(EventType::ControlChange, 64, 127)));
    assert(!learn.processEvent(event(EventType::Clock)));
    // Real modulation events must reach the CC learning step.
    learn.skipStep(); learn.skipStep();
    assert(learn.currentStep() == LearnStep::MoveModulation);
    std::this_thread::sleep_for(std::chrono::milliseconds(260));
    assert(learn.processEvent(event(EventType::Modulation, 1, 90)));
    assert(learn.currentStep() == LearnStep::TurnKnob1);
    assert(profile.modulation.number == 1);
}

int main() {
    testParser(); testQueue(); testWakeAndConcurrentProducers(); testLearn();
    std::puts("PASS: MIDI targets, release ordering/reserve, Panic/ISR/epochs, wakeup, concurrent producers and Learn safety");
}
