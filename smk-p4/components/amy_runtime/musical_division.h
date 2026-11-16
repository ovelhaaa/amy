#pragma once
#include "tempo.h"

namespace smk::p4::tempo {
enum class Division : uint8_t {
    Quarter, DottedEighth, Eighth, EighthTriplet,
    DottedSixteenth, Sixteenth, SixteenthTriplet, ThirtySecond, Count
};

constexpr unsigned divisionNumerator(Division division) {
    // Forty-eighths of a quarter note.
    constexpr unsigned numerators[]{48,36,24,16,18,12,8,6};
    const unsigned index=static_cast<unsigned>(division);
    return index < unsigned(Division::Count) ? numerators[index] : 0;
}

constexpr float durationMs(Division division,unsigned bpm) {
    return bpm>=kMinBpm && bpm<=kMaxBpm
        ? 60000.0f*divisionNumerator(division)/(48*bpm) : 0.0f;
}
}
