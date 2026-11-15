#pragma once
#include "tempo.h"

namespace smk::p4::arp {
enum class Rate : uint8_t { Quarter, Eighth, EighthTriplet, Sixteenth, SixteenthTriplet, ThirtySecond };

class Clock {
public:
    static constexpr unsigned kPpqn=tempo::kPpqn;
    static constexpr unsigned ticks(Rate rate) {
        constexpr unsigned divisions[]{24,12,8,6,4,3};
        return divisions[unsigned(rate)];
    }

    void reset() { started_=false; odd_=false; }
    bool due(const tempo::Position& position) const {
        return !started_ || position.phase-start_>=interval_;
    }

    // Phase is in quarter-hundredths of a PPQN tick times 60 * sample rate.
    // Retain sub-block overshoot when scheduling the next interval.
    uint32_t step(Rate rate,unsigned swing,const tempo::Position& position,unsigned gate) {
        start_=started_?start_+interval_:position.phase;
        started_=true;
        const bool triplet=rate==Rate::EighthTriplet || rate==Rate::SixteenthTriplet;
        const unsigned weight=triplet?100:2*(odd_?100-swing:swing);
        interval_=ticks(rate)*(tempo::kUnitsPerTick/100)*weight;
        odd_=!odd_;
        const uint32_t gate_phase=interval_/100*gate;
        const uint32_t speed=position.unitsPerFrame();
        const uint64_t overshoot=position.phase-start_;
        return gate_phase>overshoot?static_cast<uint32_t>((gate_phase-overshoot+speed-1)/speed):0;
    }
private:
    uint64_t start_=0;
    uint32_t interval_=0;
    bool started_=false, odd_=false;
};
}
