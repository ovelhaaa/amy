#include <cstdio>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include "arpeggiator.h"
#include "step_sequencer.h"
#include "midi_parser.h"

using namespace smk;

static std::vector<SynthEvent> g_dispatched_events;

static void testCallback(const SynthEvent& ev, void* ctx) {
    g_dispatched_events.push_back(ev);
}

void test_arpeggiator_chord_no_stuck_notes() {
    printf("[TEST] Arpeggiator Chord Mode All Notes Off Validation...\n");
    g_dispatched_events.clear();
    EventBus bus(64);

    Arpeggiator arp;
    arp.setEnabled(true);
    arp.setMode(ArpMode::Chord);
    arp.setDivision(ArpDivision::Div1_16); // 6 ticks per step
    arp.setGatePercent(50.0f); // 3 ticks gate

    // Hold C-E-G triad: 60, 64, 67
    arp.noteOn(60, 100);
    arp.noteOn(64, 90);
    arp.noteOn(67, 80);

    // Tick 0: trigger step -> 3 NoteOn events
    arp.processTick(0, bus);
    // Drain events from bus
    SynthEvent ev;
    std::vector<SynthEvent> tick0_events;
    while (bus.receive(ev, 0)) tick0_events.push_back(ev);

    assert(tick0_events.size() == 3);
    assert(tick0_events[0].type == EventType::NoteOn && tick0_events[0].id == 60);
    assert(tick0_events[1].type == EventType::NoteOn && tick0_events[1].id == 64);
    assert(tick0_events[2].type == EventType::NoteOn && tick0_events[2].id == 67);

    // Ticks 1 and 2: no new events
    arp.processTick(1, bus);
    arp.processTick(2, bus);
    assert(!bus.receive(ev, 0));

    // Tick 3: Gate expires -> MUST emit NoteOff for ALL 3 notes (60, 64, 67)
    arp.processTick(3, bus);
    std::vector<SynthEvent> tick3_events;
    while (bus.receive(ev, 0)) tick3_events.push_back(ev);

    assert(tick3_events.size() == 3);
    assert(tick3_events[0].type == EventType::NoteOff && tick3_events[0].id == 60);
    assert(tick3_events[1].type == EventType::NoteOff && tick3_events[1].id == 64);
    assert(tick3_events[2].type == EventType::NoteOff && tick3_events[2].id == 67);

    printf("  -> Arpeggiator Chord Mode cleanly turns off all active voices. [PASS]\n");
}

void test_arpeggiator_disable_safety() {
    printf("[TEST] Arpeggiator Disable Safety NoteOff...\n");
    EventBus bus(64);
    Arpeggiator arp;
    arp.setEnabled(true);
    arp.setMode(ArpMode::Up);
    arp.setDivision(ArpDivision::Div1_16);

    arp.noteOn(72, 100);
    arp.processTick(0, bus);

    SynthEvent ev;
    assert(bus.receive(ev, 0));
    assert(ev.type == EventType::NoteOn && ev.id == 72);

    // Disable arp at tick 1 while note is sounding
    arp.setEnabled(false);
    arp.processTick(1, bus);

    // Must emit NoteOff for 72 immediately
    assert(bus.receive(ev, 0));
    assert(ev.type == EventType::NoteOff && ev.id == 72);

    printf("  -> Arpeggiator emits NoteOff when disabled mid-note. [PASS]\n");
}

void test_step_sequencer_stop_safety() {
    printf("[TEST] StepSequencer Stop Safety NoteOffs...\n");
    EventBus bus(64);
    StepSequencer seq;
    seq.play();

    // Trigger BD and SD on Step 0
    seq.setStep(0, 36, 100, true);
    seq.selectTrack(1);
    seq.setStep(0, 38, 100, true);

    seq.processTick(0, bus);

    SynthEvent ev;
    std::vector<SynthEvent> on_events;
    while (bus.receive(ev, 0)) on_events.push_back(ev);
    assert(on_events.size() >= 2);

    // Stop sequencer before gate expires -> Must emit NoteOff for both active tracks
    seq.stop(bus);

    std::vector<SynthEvent> stop_events;
    while (bus.receive(ev, 0)) stop_events.push_back(ev);
    assert(stop_events.size() >= 2);
    assert(stop_events[0].type == EventType::NoteOff);
    assert(stop_events[1].type == EventType::NoteOff);

    printf("  -> StepSequencer cleanly silences tracks on stop. [PASS]\n");
}

void test_midi_parser_modulation_decoupling() {
    printf("[TEST] MidiParser Modulation Decoupling from CC 1...\n");
    g_dispatched_events.clear();
    MidiParser parser;
    parser.setSource(EventSource::UsbMidi);
    parser.setCallback(testCallback, nullptr);

    // Send USB MIDI CC 1 (Modulation Wheel, val=95)
    // Packet: CIN=0x0B, Status=0xB0 (Ch 1 CC), Data1=0x01 (Modulation), Data2=0x5F (95)
    uint8_t usb_mod_packet[4] = { 0x0B, 0xB0, 0x01, 0x5F };
    parser.processUsbMidiPacket(usb_mod_packet);

    assert(g_dispatched_events.size() == 1);
    assert(g_dispatched_events[0].type == EventType::Modulation);
    assert(g_dispatched_events[0].id == 1);
    assert(g_dispatched_events[0].value == 95);

    // Send regular CC 10 (Pan, val=64) -> Must be EventType::ControlChange
    g_dispatched_events.clear();
    uint8_t usb_pan_packet[4] = { 0x0B, 0xB0, 0x0A, 0x40 };
    parser.processUsbMidiPacket(usb_pan_packet);

    assert(g_dispatched_events.size() == 1);
    assert(g_dispatched_events[0].type == EventType::ControlChange);
    assert(g_dispatched_events[0].id == 10);
    assert(g_dispatched_events[0].value == 64);

    printf("  -> Modulation (CC 1) properly decoupled without duplicate ControlChange. [PASS]\n");
}

int main() {
    printf("====================================================\n");
    printf("=== Running SMK-S3 Live Audio/Event Unit Tests   ===\n");
    printf("====================================================\n");

    test_arpeggiator_chord_no_stuck_notes();
    test_arpeggiator_disable_safety();
    test_step_sequencer_stop_safety();
    test_midi_parser_modulation_decoupling();

    printf("\n=== ALL AUDIO & EVENT SUITE TESTS PASSED! ===\n");
    return 0;
}
