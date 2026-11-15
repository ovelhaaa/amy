#pragma once
#include <cstdint>

namespace smk::p4::arp {
enum class Rate : uint8_t { Quarter, Eighth, EighthTriplet, Sixteenth, SixteenthTriplet, ThirtySecond };

class Clock {
public:
    static constexpr unsigned kPpqn=24;
    static constexpr unsigned ticks(Rate rate) {
        constexpr unsigned divisions[]{24,12,8,6,4,3};
        return divisions[unsigned(rate)];
    }

    void reset() { phase_=0; interval_=0; odd_=false; }
    bool due() const { return phase_>=interval_; }

    // Phase is in quarter-hundredths of a PPQN tick times 60 * sample rate.
    // Retain sub-block overshoot when scheduling the next interval.
    uint32_t step(Rate rate,unsigned swing,unsigned bpm,unsigned gate) {
        phase_-=interval_;
        const bool triplet=rate==Rate::EighthTriplet || rate==Rate::SixteenthTriplet;
        const unsigned weight=triplet?100:2*(odd_?100-swing:swing);
        interval_=ticks(rate)*(kUnitsPerTick/100)*weight;
        odd_=!odd_;
        const uint32_t gate_phase=interval_/100*gate;
        const uint32_t speed=bpm*kPpqn*25;
        return gate_phase>phase_?(gate_phase-phase_+speed-1)/speed:0;
    }
    void advance(unsigned bpm,unsigned frames) { phase_+=bpm*kPpqn*25*frames; }
private:
    static constexpr uint32_t kUnitsPerTick=60*48000*25;
    uint32_t phase_=0,interval_=0;
    bool odd_=false;
};
}
