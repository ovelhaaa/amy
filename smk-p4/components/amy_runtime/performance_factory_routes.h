#pragma once

#include "sound_edit.h"
#include "fx_state.h"

namespace smk::p4::performance {

enum FxDestination : uint16_t {
    ChorusDepth = edit::Count, DelayMix, DelayFeedback, ReverbMix, ReverbSize, MasterTone
};

constexpr Route route(uint16_t destination, float min, float max, float depth,
                      uint8_t family = Both, Curve curve = Curve::Linear) {
    return {destination, min, max, depth, curve, Polarity::Bipolar, family, true};
}

inline constexpr Routes kFactoryRoutes{{
    {{route(edit::Detune,-100,100,18,Juno), route(edit::FilterEnv,-11,11,0.8f,Juno),
      route(edit::Depth,0,1,0.25f,Fm), route(edit::FmFeedback,0,100,12,Fm)}},
    {{route(edit::Cutoff,20,18000,3500,Juno,Curve::Exponential),
      route(edit::Resonance,0.1f,2,0.25f,Juno), route(edit::Depth,0,1,0.5f,Fm)}},
    {{route(ChorusDepth,0,1,0.4f), route(DelayFeedback,0,0.95f,0.16f), route(DelayMix,0,1,0.10f)}},
    {{route(edit::Decay,1,30000,400,Juno), route(edit::Sustain,0,100,25,Juno),
      route(edit::OpDecay0,0,4000,180,Fm), route(edit::OpDecay0+1,0,4000,180,Fm)}},
    {{route(edit::Attack,1,8000,300,Juno,Curve::Exponential),
      route(edit::OpAttack0,0,2000,180,Fm,Curve::Exponential),
      route(edit::OpAttack0+1,0,2000,180,Fm,Curve::Exponential)}},
    {{route(edit::Release,1,8000,700,Juno,Curve::Exponential),
      route(ReverbMix,0,1,0.12f,Fm,Curve::Exponential)}},
    {{route(ReverbMix,0,1,0.35f), route(ReverbSize,0,1,0.25f), route(DelayMix,0,1,0.16f)}},
    {{route(MasterTone,-1,1,0.45f), route(edit::Resonance,0.1f,2,0.35f,Juno),
      route(edit::FmFeedback,0,100,20,Fm)}}
}};

constexpr uint8_t familyMask(uint8_t family) { return family == 0 ? Fm : Juno; }

inline bool available(unsigned macro,const edit::State& state) {
    if(macro>=kMacroCount || !state.available)return false;
    for(const auto& route:kFactoryRoutes[macro]){
        if(route.enabled && (route.families&familyMask(state.family)) &&
           (route.destination>=edit::Count || (state.available&edit::bit(route.destination))))return true;
    }
    return false;
}

inline bool fxRoutesChanged(const edit::State& state, const edit::Request& request) {
    const auto& before = state.compare ? kNeutral : state.performance_macros;
    const auto& after = request.compare ? kNeutral : request.performance_macros;
    const auto family = familyMask(state.family);
    for (unsigned m = 0; m < kMacroCount; ++m) {
        if (before[m] == after[m]) continue;
        for (const auto& route : kFactoryRoutes[m]) {
            if (route.enabled && route.destination >= edit::Count && (route.families & family)) return true;
        }
    }
    return false;
}

inline fx::FxConfig effectiveFx(fx::FxConfig manual, const edit::State& state) {
    if (state.compare || state.performance_macros == kNeutral) return manual;
    const auto composeFx = [&](FxDestination destination, float base, float min, float max) {
        return compose(kFactoryRoutes, state.performance_macros, familyMask(state.family),
                       destination, base, min, max).value;
    };
    manual.chorus_depth = composeFx(ChorusDepth, manual.chorus_depth, 0, 1);
    manual.delay_feedback = composeFx(DelayFeedback, manual.delay_feedback, 0, 0.95f);
    manual.delay_mix = composeFx(DelayMix, manual.delay_mix, 0, 1);
    manual.reverb_mix = composeFx(ReverbMix, manual.reverb_mix, 0, 1);
    manual.reverb_size = composeFx(ReverbSize, manual.reverb_size, 0, 1);
    manual.master_tone = composeFx(MasterTone, manual.master_tone, -1, 1);
    return manual;
}

}
