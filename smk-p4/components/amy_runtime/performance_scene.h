#pragma once

#include "arpeggiator.h"
#include "harmony.h"
#include "pattern_library.h"
#include "performance_factory_routes.h"
#include "performance_lock.h"

namespace smk::p4::scene {

constexpr unsigned kSlots=8;
enum Domain : uint8_t {
    Patch=1<<0, Pattern=1<<1, Fx=1<<2, Harmony=1<<3, Arp=1<<4, Macros=1<<5, Tempo=1<<6
};
constexpr uint8_t kAll=Patch|Pattern|Fx|Harmony|Arp|Macros|Tempo;

// Stable identity is the scene bank index. The pattern is a slot reference.
struct Configuration {
    char name[seq::kPatternNameBytes]{};
    uint8_t domains=0, pattern_slot=seq::kNoSlot;
    uint32_t pattern_sequence=0;
    uint16_t patch_id=0, bpm=0;
    fx::FxConfig fx;
    harmony::Config harmony;
    arp::Config arp;
    performance::Positions macros=performance::kNeutral;
    performance::RouteConfiguration routes;
};

enum class Dirty : uint8_t { Clean, Modified, Unsaved, Invalid, Pending };

inline uint8_t differences(const Configuration& a,const Configuration& b,uint8_t domains) {
    uint8_t changed=0;
    if((domains&Patch) && a.patch_id!=b.patch_id) changed|=Patch;
    if((domains&Pattern) && (a.pattern_slot!=b.pattern_slot ||
        a.pattern_sequence!=b.pattern_sequence)) changed|=Pattern;
    if(domains&Fx) {
        uint8_t left[fx::kFxRecordBytes],right[fx::kFxRecordBytes];
        if(!fx::encodeRecord(a.fx,0,left) || !fx::encodeRecord(b.fx,0,right) ||
           std::memcmp(left,right,sizeof(left))) changed|=Fx;
    }
    if((domains&Harmony) && harmony::pack(a.harmony)!=harmony::pack(b.harmony)) changed|=Harmony;
    if((domains&Arp) && !(a.arp==b.arp)) changed|=Arp;
    if((domains&Macros) && (a.macros!=b.macros ||
        performance::activeRoutes(a.routes)!=performance::activeRoutes(b.routes))) changed|=Macros;
    if((domains&Tempo) && a.bpm!=b.bpm) changed|=Tempo;
    return changed;
}

enum class Reject : uint8_t {
    None, InvalidScene, EmptySelection, PatchUnavailable, PatternUnavailable, InvalidFx,
    InvalidHarmony, InvalidArp, IncompatibleMacros, InvalidTempo, CompareActive
};

struct Selection {
    uint8_t domains=0;
    Reject reason=Reject::None;
};

constexpr uint8_t effectiveDomains(uint8_t stored,uint8_t requested,uint8_t locks,bool apply_tempo) {
    const unsigned excluded=(locks&perf_lock::Fx?Fx:0) |
        (locks&perf_lock::Harmony?Harmony:0) | (locks&perf_lock::Arp?Arp:0) |
        (locks&perf_lock::Macros?Macros:0) | (apply_tempo?0:Tempo);
    return uint8_t(stored & requested & kAll & ~excluded);
}

inline bool validArp(const arp::Config& value) {
    return value.octaves>=1 && value.octaves<=4 && value.gate>=10 && value.gate<=100 &&
        value.swing>=50 && value.swing<=75 && value.mode<=arp::Mode::AsPlayed &&
        value.rate<=arp::Rate::ThirtySecond;
}

// The caller supplies the capabilities of the *resulting* patch and resolves
// references on the control side before proposing any owner mutation.
inline Selection validate(const Configuration& scene,uint8_t requested,uint8_t locks,bool apply_tempo,
                          bool patch_available,const seq::Slot* pattern,uint8_t resulting_family,
                          edit::Mask available) {
    const auto domains=effectiveDomains(scene.domains,requested,locks,apply_tempo);
    Reject reason=Reject::None;
    if(!scene.domains || (scene.domains&~kAll) || !seq::validName(scene.name))
        reason=Reject::InvalidScene;
    else if(!domains) reason=Reject::EmptySelection;
    else if((domains&Patch) && !patch_available) reason=Reject::PatchUnavailable;
    else if((domains&Pattern) && (scene.pattern_slot>=seq::kPatternSlots || !pattern ||
            pattern->state!=seq::SlotState::Valid ||
            (scene.pattern_sequence && scene.pattern_sequence!=pattern->sequence) ||
            !seq::Engine::valid(pattern->settings)))
        reason=Reject::PatternUnavailable;
    else if((domains&Fx) && !fx::valid(scene.fx)) reason=Reject::InvalidFx;
    else if((domains&Harmony) && !harmony::valid(scene.harmony)) reason=Reject::InvalidHarmony;
    else if((domains&Arp) && !validArp(scene.arp)) reason=Reject::InvalidArp;
    else if((domains&Macros) && (!available || !performance::valid(scene.macros) ||
            !performance::valid(scene.routes,resulting_family,available)))
        reason=Reject::IncompatibleMacros;
    else if((domains&Tempo) && (scene.bpm<tempo::kMinBpm || scene.bpm>tempo::kMaxBpm))
        reason=Reject::InvalidTempo;
    return {domains,reason};
}

}
