#pragma once

#include "screen_id.h"

namespace smk {

// B5/B6 traverse only performance pages. Utility and modal screens return to
// Home instead of entering the performance ring at an ambiguous position.
constexpr ScreenId nextPerformancePage(ScreenId current) {
    switch (current) {
        case ScreenId::Home: return ScreenId::Sequencer;
        case ScreenId::Sequencer: return ScreenId::Pads;
        case ScreenId::Pads: return ScreenId::Scenes;
        case ScreenId::Scenes: return ScreenId::Home;
        default: return ScreenId::Home;
    }
}

constexpr ScreenId previousPerformancePage(ScreenId current) {
    switch (current) {
        case ScreenId::Home: return ScreenId::Scenes;
        case ScreenId::Sequencer: return ScreenId::Home;
        case ScreenId::Pads: return ScreenId::Sequencer;
        case ScreenId::Scenes: return ScreenId::Pads;
        default: return ScreenId::Home;
    }
}

} // namespace smk
