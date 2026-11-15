#pragma once
#include "tempo.h"

namespace smk::p4::tempo {
enum class Division : uint8_t {
    Quarter, DottedEighth, Eighth, EighthTriplet,
    DottedSixteenth, Sixteenth, SixteenthTriplet, ThirtySecond, Count
};

constexpr float durationMs(Division division,unsigned bpm) {
    // All supported divisions expressed as forty-eighths of a quarter note.
    constexpr unsigned numerators[]{48,36,24,16,18,12,8,6};
    const unsigned index=static_cast<unsigned>(division);
    return index < unsigned(Division::Count) && bpm>=kMinBpm && bpm<=kMaxBpm
        ? 60000.0f*numerators[index]/(48*bpm) : 0.0f;
}
}
