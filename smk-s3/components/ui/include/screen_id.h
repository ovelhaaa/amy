#pragma once

#include <cstdint>

namespace smk {

enum class ScreenId : uint8_t {
    Splash,
    Home,
    System,
    MidiMonitor,
    Sequencer,
    Pads,
    MidiLearn,
    Scenes,
};

} // namespace smk
