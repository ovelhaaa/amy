#pragma once
#include <cstdint>

namespace smk::p4::tempo {
constexpr unsigned kMinBpm=40, kMaxBpm=240, kDefaultBpm=120;
constexpr unsigned kPpqn=24;
constexpr uint32_t kUnitsPerTick=60*48000*25;

struct Position {
    // Unsigned differences preserve elapsed phase across wrap.
    uint64_t phase=0;
    uint32_t revision=0;
    uint16_t bpm=kDefaultBpm;
    uint32_t unitsPerFrame() const { return bpm*kPpqn*25; }
    uint64_t tick() const { return phase/kUnitsPerTick; }
};

class Clock {
public:
    const Position& position() const { return position_; }
    bool setBpm(unsigned bpm) {
        if(bpm<kMinBpm || bpm>kMaxBpm) return false;
        if(bpm!=position_.bpm) {
            position_.bpm=static_cast<uint16_t>(bpm);
            ++position_.revision;
        }
        return true;
    }
    void advance(unsigned frames) {
        position_.phase+=uint64_t(position_.unitsPerFrame())*frames;
    }
private:
    Position position_;
};
}
