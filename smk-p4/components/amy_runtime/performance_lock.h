#pragma once
#include <cstdint>

namespace smk::p4::perf_lock {

enum Flag : uint8_t {
    None    = 0,
    Fx      = 1 << 0,
    Harmony = 1 << 1,
    Arp     = 1 << 2,
    Macros  = 1 << 3
};

// Default policy per specification:
// Keep FX: ON, Keep Harmony: ON, Keep Arpeggiator: ON, Keep Macros: OFF
inline constexpr uint8_t kDefaultLockMask = Fx | Harmony | Arp;

struct Options {
    uint16_t patch_id = 0;
    uint8_t preserve_mask = 0;

    constexpr bool keepFx() const { return (preserve_mask & Fx) != 0; }
    constexpr bool keepHarmony() const { return (preserve_mask & Harmony) != 0; }
    constexpr bool keepArp() const { return (preserve_mask & Arp) != 0; }
    constexpr bool keepMacros() const { return (preserve_mask & Macros) != 0; }
};

} // namespace smk::p4::perf_lock
