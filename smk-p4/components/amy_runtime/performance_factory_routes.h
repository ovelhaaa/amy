#pragma once

#include "sound_edit.h"
#include "fx_state.h"

namespace smk::p4::performance {

enum FxDestination : uint16_t {
    ChorusDepth = edit::Count, DelayMix, DelayFeedback, ReverbMix, ReverbSize, MasterTone,
    DelayTime, EqLow, EqMid, EqHigh
};

constexpr Route route(uint16_t destination, float min, float max, float depth,
                      uint8_t family = Both, Curve curve = Curve::Linear) {
    return {destination, min, max, depth, curve, Polarity::Bipolar, family, true};
}

inline constexpr Routes kFactoryRoutes{{
    {{route(edit::Detune,-100,100,18,Juno), route(edit::FilterEnv,-11,11,0.8f,Juno),
      route(edit::Depth,0,1,0.25f,Fm), route(edit::FmFeedback,0,100,12,Fm)}},
    {{route(edit::Cutoff,20,18000,3500,Juno,Curve::Exponential),
      route(edit::Resonance,0.1f,2,0.25f,Juno), route(edit::Depth,0,1,0.5f,Fm),
      route(MasterTone,-1,1,0.18f,Fm)}},
    {{route(ChorusDepth,0,1,0.4f), route(DelayFeedback,0,0.95f,0.16f), route(DelayMix,0,1,0.10f)}},
    {{route(edit::Decay,1,30000,400,Juno), route(edit::Sustain,0,100,25,Juno),
      route(edit::OpDecay0,0,4000,180,Fm), route(edit::OpDecay0+1,0,4000,180,Fm)}},
    {{route(edit::Attack,1,8000,300,Juno,Curve::Exponential),
      route(edit::FilterEnv,-11,11,0.4f,Juno,Curve::Exponential),
      route(edit::OpAttack0,0,2000,180,Fm,Curve::Exponential),
      route(edit::OpAttack0+1,0,2000,180,Fm,Curve::Exponential)}},
    {{route(edit::Release,1,8000,700,Juno,Curve::Exponential),
      route(ReverbMix,0,1,0.12f,Both,Curve::Exponential),
      route(ReverbSize,0,1,0.15f,Fm,Curve::Exponential)}},
    {{route(ReverbMix,0,1,0.35f), route(ReverbSize,0,1,0.25f), route(DelayMix,0,1,0.16f)}},
    {{route(MasterTone,-1,1,0.45f), route(edit::Resonance,0.1f,2,0.35f,Juno),
      route(edit::FmFeedback,0,100,20,Fm)}}
}};

constexpr uint8_t familyMask(uint8_t family) { return family == 0 ? Fm : Juno; }

constexpr edit::Domain destinationDomain(unsigned destination) {
    if (destination < edit::Count) return edit::continuousDomain(destination);
    switch (destination) {
        case ChorusDepth: case DelayMix: case ReverbMix: case ReverbSize: return {0, 1};
        case DelayFeedback: return {0, 0.95f};
        case MasterTone: return {-1, 1};
        case DelayTime: return {10, 1000};
        case EqLow: case EqMid: case EqHigh: return {-12, 12};
        default: return {};
    }
}

constexpr uint8_t destinationFamilies(unsigned destination) {
    if (!destinationDomain(destination).continuous()) return 0;
    if (destination >= edit::Count) return Both;
    const auto bit = edit::bit(destination);
    return ((edit::familyParameterMask(0) & bit) ? Fm : 0) |
           ((edit::familyParameterMask(1) & bit) ? Juno : 0);
}

inline const Routes& activeRoutes(const RouteConfiguration& config) {
    return config.custom ? config.routes : kFactoryRoutes;
}

inline bool valid(const RouteConfiguration& config, uint8_t patch_family, edit::Mask available) {
    if (!config.custom) return true;
    const auto family = familyMask(patch_family);
    for (unsigned macro = 0; macro < kMacroCount; ++macro) {
        for (unsigned slot = 0; slot < kRoutesPerMacro; ++slot) {
            const auto& candidate = config.routes[macro][slot];
            // ROM routes may target topology-dependent parameters missing on this patch.
            if (candidate == Route{} || candidate == kFactoryRoutes[macro][slot]) continue;
            const auto domain = destinationDomain(candidate.destination);
            if (!candidate.families || (candidate.families & ~destinationFamilies(candidate.destination)) ||
                (candidate.families & family && candidate.destination < edit::Count &&
                 !(available & edit::bit(candidate.destination))) ||
                !std::isfinite(candidate.minimum) || !std::isfinite(candidate.maximum) ||
                !std::isfinite(candidate.depth) || candidate.minimum < domain.minimum ||
                candidate.maximum > domain.maximum || candidate.minimum > candidate.maximum ||
                std::abs(candidate.depth) > domain.maximum - domain.minimum ||
                candidate.curve > Curve::SCurve || candidate.polarity > Polarity::Negative)
                return false;
        }
    }
    return true;
}

inline bool available(unsigned macro,const edit::State& state) {
    if(macro>=kMacroCount || !state.available)return false;
    for(const auto& route:activeRoutes(state.performance_routes)[macro]){
        if(route.enabled && (route.families&familyMask(state.family)) &&
           (route.destination>=edit::Count || (state.available&edit::bit(route.destination))))return true;
    }
    return false;
}

inline bool fxRoutesChanged(const edit::State& state, const edit::Request& request) {
    const auto& before = state.compare ? kNeutral : state.performance_macros;
    const auto& after = request.compare ? kNeutral : request.performance_macros;
    const auto family = familyMask(state.family);
    const auto& old_routes = activeRoutes(state.performance_routes);
    const auto& new_routes = activeRoutes(request.performance_routes);
    for (unsigned m = 0; m < kMacroCount; ++m) {
        if (before[m] == kNeutral[m] && after[m] == kNeutral[m]) continue;
        for (unsigned slot = 0; slot < kRoutesPerMacro; ++slot) {
            const auto& old_route = old_routes[m][slot];
            const auto& new_route = new_routes[m][slot];
            if (before[m] == after[m] && old_route == new_route) continue;
            const auto affects_fx = [family](const Route& route) {
                return route.enabled && route.destination >= edit::Count && (route.families & family);
            };
            if ((before[m] != kNeutral[m] && affects_fx(old_route)) ||
                (after[m] != kNeutral[m] && affects_fx(new_route))) return true;
        }
    }
    return false;
}

inline fx::FxConfig effectiveFx(fx::FxConfig manual, const edit::State& state, uint8_t* sources = nullptr) {
    if (sources) std::fill_n(sources, EqHigh - ChorusDepth + 1, 0);
    if (state.compare || state.performance_macros == kNeutral) return manual;
    const auto composeFx = [&](FxDestination destination, float base, float min, float max) {
        const auto result = compose(activeRoutes(state.performance_routes), state.performance_macros, familyMask(state.family),
                                    destination, base, min, max);
        if (sources) sources[destination - ChorusDepth] = result.sources;
        return result.value;
    };
    manual.chorus_depth = composeFx(ChorusDepth, manual.chorus_depth, 0, 1);
    manual.delay_feedback = composeFx(DelayFeedback, manual.delay_feedback, 0, 0.95f);
    manual.delay_mix = composeFx(DelayMix, manual.delay_mix, 0, 1);
    manual.reverb_mix = composeFx(ReverbMix, manual.reverb_mix, 0, 1);
    manual.reverb_size = composeFx(ReverbSize, manual.reverb_size, 0, 1);
    manual.master_tone = composeFx(MasterTone, manual.master_tone, -1, 1);
    if (state.performance_routes.custom) {
        manual.delay_time_ms = composeFx(DelayTime, manual.delay_time_ms, 10, 1000);
        manual.eq_low_db = composeFx(EqLow, manual.eq_low_db, -12, 12);
        manual.eq_mid_db = composeFx(EqMid, manual.eq_mid_db, -12, 12);
        manual.eq_high_db = composeFx(EqHigh, manual.eq_high_db, -12, 12);
    }
    return manual;
}

}
