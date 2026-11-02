#pragma once
#include <cstdint>

namespace smk {

enum class EventType : uint8_t {
    NoteOn = 0,
    NoteOff,
    PitchBend,
    Modulation,
    ControlChange,
    ProgramChange,
    AllNotesOff,
    Panic,
    PadHit,
    ButtonPress,
    TransportPlay,
    TransportStop,
    TransportRecord,
    Clock,
    ParameterChange,
    PatchChange,
    SceneChange,
    UsbConnect,
    UsbDisconnect,
    SourceStop,
};

enum class ButtonId : uint8_t {
    KnobBank = 0,
    PadBank = 1,
    Play = 2,
    Stop = 3,
    Record = 4,
    OctavePlus = 5,
    OctaveMinus = 6,
    Arp = 7,
    Scene = 8
};

enum class EventSource : uint8_t {
    UsbMidi = 0,
    Sequencer,
    Arpeggiator,
    Ui,
    Console,
    Internal,
    RemoteUi,
    Count,
};

// Tokens cross 32-bit rollover without aliasing. Exhaustion rejects future
// generated onsets until reboot rather than falling back to physical token 0.
inline uint64_t allocateNoteToken(uint64_t& counter) {
    return counter == UINT64_MAX ? 0 : ++counter;
}

struct alignas(8) SynthEvent {
    SynthEvent() = default;
    // Retain the existing positional construction order despite using padding.
    constexpr SynthEvent(EventType t, EventSource s, uint8_t ch, uint16_t number,
                         int16_t val, uint64_t time_us, uint64_t due_us = 0)
        : type(t), source(s), channel(ch), id(number), value(val),
          timestamp_us(time_us), deadline_us(due_us) {}
    EventType type;
    EventSource source;
    uint8_t channel;        // MIDI channel 0-15
    uint8_t source_instance = 0; // Occupies former alignment padding.
    uint16_t id;            // note number, CC number, program number
    int16_t value;          // velocity, bend (-8192..8191), CC, or bit-preserving USB PID
    uint64_t timestamp_us;  // microsecond timestamp from esp_timer_get_time()
    uint64_t deadline_us = 0; // Generated trigger/release deadline; zero means immediate.
    uint64_t note_token = 0; // Generated On/Off pair; never reset with transport.

};

constexpr bool expiredTrigger(const SynthEvent& event, uint64_t now_us) {
    return event.type == EventType::NoteOn && event.value > 0 &&
           event.deadline_us && now_us >= event.deadline_us;
}

// These events must reach the application even while MIDI Learn consumes input.
constexpr bool isReleaseEvent(const SynthEvent& event) {
    return event.type == EventType::SourceStop || event.type == EventType::NoteOff ||
           (event.type == EventType::NoteOn && event.value == 0) ||
           event.type == EventType::AllNotesOff ||
           event.type == EventType::TransportStop ||
           (event.type == EventType::ButtonPress && event.id == static_cast<uint16_t>(ButtonId::Stop)) ||
           (event.type == EventType::ControlChange &&
            ((event.id == 64 && event.value < 64) || event.id == 120 || event.id == 123));
}

constexpr bool isUsbLifecycleEvent(const SynthEvent& event) {
    return event.type == EventType::UsbConnect || event.type == EventType::UsbDisconnect;
}

constexpr bool bypassMidiLearn(const SynthEvent& event) {
    return event.source == EventSource::RemoteUi || event.type == EventType::Panic ||
           isReleaseEvent(event) || isUsbLifecycleEvent(event);
}

constexpr bool canProcessWhileAudioStopped(const SynthEvent& event) {
    return event.type == EventType::Panic || isReleaseEvent(event) || isUsbLifecycleEvent(event);
}

} // namespace smk
