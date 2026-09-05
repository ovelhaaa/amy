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
};

struct SynthEvent {
    EventType type;
    EventSource source;
    uint8_t channel;        // MIDI channel 0-15
    uint16_t id;            // note number, CC number, program number
    int32_t value;          // velocity, bend value (-8192..8191), CC value (0-127)
    uint32_t timestamp_us;  // microsecond timestamp from esp_timer_get_time()
};

// These events must reach the application even while MIDI Learn consumes input.
constexpr bool isReleaseEvent(const SynthEvent& event) {
    return event.type == EventType::NoteOff ||
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
    return event.type == EventType::Panic || isReleaseEvent(event) || isUsbLifecycleEvent(event);
}

} // namespace smk
