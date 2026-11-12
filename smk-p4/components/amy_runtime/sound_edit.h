#pragma once
#include <atomic>
#include <cmath>
#include <cstdint>
#include "performance_macros.h"
namespace smk::p4::edit {
using Mask = uint64_t;
constexpr Mask bit(unsigned p) { return Mask(1) << p; }

// Stable persisted IDs. Operator IDs follow materialized algo_source order (OP1..OP6).
// IDs 0..28 are frozen for V1 (0..16) and legacy V2 (0..28) compatibility.
enum Param : uint8_t {
    Cutoff = 0, Resonance = 1, Attack = 2, Release = 3,
    OpLevel0 = 4, OpLevel5 = 9,
    OpRatio0 = 10, OpRatio5 = 15,
    Depth = 16,
    Decay = 17, Sustain = 18, FilterEnv = 19, FilterKeytrack = 20,
    OscLevel = 21, SubLevel = 22, NoiseLevel = 23, PulseWidth = 24, Detune = 25,
    PatchLevel = 26, Pan = 27, Velocity = 28,
    FmFeedback = 29,
    OpAttack0 = 30, OpAttack5 = 35,
    OpDecay0 = 36, OpDecay5 = 41,
    OpVel0 = 42, OpVel5 = 47,
    LfoRate = 48, LfoDepth = 49, LfoWave = 50, LfoDest = 51,
    ModEnable = 52, ModSource = 53, ModDest = 54, ModAmount = 55, ModPolarity = 56,
    FmAlgorithm = 57,
    Count = 58
};
static_assert(Count <= 64, "Widen edit::Mask before admitting more than 64 fields");
constexpr unsigned kSlots = 16;

struct State {
    uint16_t patch = 128;
    uint8_t family = 0;
    Mask available = 0, mask = 0;
    uint32_t revision = 0, request = 0, errors = 0, epoch = 0, apply_max_us = 0;
    uint8_t macros[3]{64, 64, 64};
    performance::Positions performance_macros = performance::kNeutral;
    performance::RouteConfiguration performance_routes;
    Mask performance_mask = 0;
    float original[Count]{}, manual[Count]{}, applied[Count]{};
    bool compare = false, dirty = false;
    int8_t user_slot = -1;
};

struct Domain {
    float minimum = 0;
    float maximum = 0;
    constexpr bool continuous() const { return minimum < maximum; }
};

constexpr Domain continuousDomain(unsigned p) {
    if (p == Cutoff) return {20.0f, 18000.0f};
    if (p == Resonance) return {0.1f, 2.0f};
    if (p == Attack || p == Release) return {1.0f, 8000.0f};
    if (p == Decay) return {1.0f, 30000.0f};
    if (p >= OpLevel0 && p <= OpLevel5) return {0.0f, 100.0f};
    if (p >= OpRatio0 && p <= OpRatio5) return {0.5f, 16.0f};
    if (p == FilterEnv) return {-11.0f, 11.0f};
    if (p == FilterKeytrack) return {0.0f, 200.0f};
    if (p == OscLevel || p == SubLevel || p == NoiseLevel || p == PatchLevel || p == Velocity) return {0.0f, 100.0f};
    if (p == PulseWidth) return {5.0f, 95.0f};
    if (p == Detune) return {-100.0f, 100.0f};
    if (p == Pan) return {-64.0f, 63.0f};
    if (p == Sustain) return {0.0f, 100.0f};
    if (p == Depth) return {0.0f, 1.0f};
    if (p == FmFeedback) return {0.0f, 100.0f};
    if (p >= OpAttack0 && p <= OpAttack5) return {0.0f, 2000.0f};
    if (p >= OpDecay0 && p <= OpDecay5) return {0.0f, 4000.0f};
    if (p >= OpVel0 && p <= OpVel5) return {0.0f, 100.0f};
    if (p == LfoRate) return {0.1f, 20.0f};
    if (p == LfoDepth) return {0.0f, 100.0f};
    if (p == ModAmount) return {-100.0f, 100.0f};
    return {};
}

inline bool valid(unsigned p, float v) {
    if (!std::isfinite(v)) return false;
    const auto domain = continuousDomain(p);
    if (domain.continuous()) return v >= domain.minimum && v <= domain.maximum;
    if (p == LfoWave) return v >= 0.0f && v <= 4.0f && std::floor(v) == v;
    if (p == LfoDest) return v >= 0.0f && v <= 3.0f && std::floor(v) == v;
    if (p == ModEnable) return v == 0.0f || v == 1.0f;
    if (p == ModSource) return v == 0.0f || v == 1.0f;
    if (p == ModDest) return v == 0.0f || v == 1.0f || v == 3.0f;
    if (p == ModPolarity) return v >= 0.0f && v <= 2.0f && std::floor(v) == v;
    if (p == FmAlgorithm) return v >= 1.0f && v <= 32.0f && std::floor(v) == v;
    return false;
}

constexpr Mask familyParameterMask(uint8_t family) {
    constexpr Mask shared = bit(PatchLevel) | bit(LfoRate) | bit(LfoDepth) | bit(LfoWave) |
        bit(LfoDest) | bit(ModEnable) | bit(ModSource) | bit(ModDest) | bit(ModAmount) | bit(ModPolarity);
    constexpr Mask legacy_juno = bit(Release + 1) - 1;
    constexpr Mask legacy_fm = (bit(Depth + 1) - 1) & ~legacy_juno;
    if (family == 1)
        return legacy_juno | shared | ((bit(Velocity + 1) - 1) & ~(bit(Depth + 1) - 1));
    return legacy_fm | shared | bit(FmAlgorithm) | ((bit(OpVel5 + 1) - 1) & ~(bit(FmFeedback) - 1));
}

struct Request {
    uint16_t patch = 0;
    uint32_t generation = 0, request = 0;
    Mask mask = 0;
    uint8_t macros[3]{64, 64, 64};
    performance::Positions performance_macros = performance::kNeutral;
    performance::RouteConfiguration performance_routes;
    float values[Count]{};
    bool compare = false;
    int8_t slot = -1;
    uint32_t recall_started_ms = 0;
    bool preset_recall = false;
};

// Single UI producer, AMY owner consumer. One request in flight; producer keeps
// latest desired complete state locally until space opens. Independent of notes.
class Mailbox {
public:
    bool push(const Request& r) {
        if (full_.load(std::memory_order_acquire)) return false;
        request_ = r;
        full_.store(true, std::memory_order_release);
        return true;
    }
    bool pop(Request& r) {
        if (!full_.load(std::memory_order_acquire)) return false;
        r = request_;
        full_.store(false, std::memory_order_release);
        return true;
    }
    void clear() {
        full_.store(false, std::memory_order_release);
    }
private:
    Request request_{};
    std::atomic<bool> full_{false};
};
}
