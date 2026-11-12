#pragma once

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace smk::p4::performance {

enum class Curve : uint8_t { Linear, Exponential, Logarithmic, SCurve };
enum class Polarity : uint8_t { Bipolar, Positive, Negative };
enum Family : uint8_t { Fm = 1, Juno = 2, Both = Fm | Juno };

inline constexpr unsigned kMacroCount = 8;
inline constexpr unsigned kRoutesPerMacro = 6;
using Positions = std::array<uint8_t, kMacroCount>;
inline constexpr Positions kNeutral{64, 64, 24, 64, 32, 52, 36, 16};

struct Route {
    uint16_t destination = 0;
    float minimum = 0;
    float maximum = 0;
    float depth = 0;
    Curve curve = Curve::Linear;
    Polarity polarity = Polarity::Bipolar;
    uint8_t families = Both;
    bool enabled = false;
    bool inverted = false;

    constexpr bool operator==(const Route& other) const {
        return destination == other.destination && minimum == other.minimum &&
            maximum == other.maximum && depth == other.depth && curve == other.curve &&
            polarity == other.polarity && families == other.families &&
            enabled == other.enabled && inverted == other.inverted;
    }
};

using Routes = std::array<std::array<Route, kRoutesPerMacro>, kMacroCount>;

struct RouteConfiguration {
    Routes routes{};
    bool custom = false;

    bool operator==(const RouteConfiguration& other) const {
        return custom == other.custom && (!custom || routes == other.routes);
    }
    bool operator!=(const RouteConfiguration& other) const { return !(*this == other); }
};

inline bool valid(const Positions& positions) {
    return std::all_of(positions.begin(), positions.end(), [](uint8_t value) { return value <= 127; });
}

inline float contribution(const Route& route, uint8_t value, uint8_t neutral) {
    if (value == neutral) return 0;
    const bool below = value < neutral;
    const float span = below ? neutral : 127 - neutral;
    float magnitude = std::abs(int(value) - int(neutral)) / span;
    switch (route.curve) {
        case Curve::Linear: break;
        case Curve::Exponential: magnitude *= magnitude; break;
        case Curve::Logarithmic: magnitude = std::sqrt(magnitude); break;
        case Curve::SCurve: magnitude *= magnitude * (3.0f - 2.0f * magnitude); break;
    }
    float offset = (below != route.inverted) ? -magnitude : magnitude;
    if (route.polarity == Polarity::Positive) offset = std::max(offset, 0.0f);
    if (route.polarity == Polarity::Negative) offset = std::min(offset, 0.0f);
    return offset * route.depth;
}

struct Composition {
    float value;
    bool active;
};

// A destination has one canonical domain. Route limits restrict each offset
// relative to the same baseline. The combined result is clamped once.
inline Composition compose(const Routes& routes, const Positions& positions,
                           uint8_t family, uint16_t destination, float manual,
                           float minimum, float maximum) {
    if (!valid(positions) || !std::isfinite(manual)) return {manual, false};
    float total = 0;
    bool active = false;
    for (unsigned m = 0; m < kMacroCount; ++m) {
        for (const auto& route : routes[m]) {
            if (!route.enabled || route.destination != destination || !(route.families & family) ||
                !std::isfinite(route.minimum) || !std::isfinite(route.maximum) ||
                !std::isfinite(route.depth) || route.minimum > route.maximum) continue;
            const float offset = contribution(route, positions[m], kNeutral[m]);
            if (offset == 0) continue;
            const float limited = std::clamp(offset, std::min(0.0f, route.minimum - manual),
                                                      std::max(0.0f, route.maximum - manual));
            total += limited;
            active |= limited != 0;
        }
    }
    if (!active || !std::isfinite(total)) return {manual, false};
    return {std::clamp(manual + total, minimum, maximum), true};
}

}
