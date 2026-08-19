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
    Arp = 7
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

} // namespace smk
