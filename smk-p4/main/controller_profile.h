#pragma once

#include "performance_midi.h"
#include "user_presets.h"
#include <array>

namespace smk::p4::performance {

// V2 wire IDs. Keep this list append-only. Zero selects the factory mapping.
inline constexpr const char* kMidiTargets[]{
    nullptr,
    "perf.macro.1","perf.macro.2","perf.macro.3","perf.macro.4","perf.macro.5","perf.macro.6","perf.macro.7","perf.macro.8",
    "osc.1.level","osc.sub_level","osc.noise_level","osc.1.pulse_width","osc.1.detune_ct",
    "amp.level","amp.pan","amp.vel_sens","fm.depth","fm.feedback","fm.op.level","fm.op.ratio",
    "filter.cutoff_hz","filter.resonance","filter.env_amount","env.amp.attack_ms","env.amp.decay_ms",
    "env.amp.sustain","env.amp.release_ms","filter.keytrack","fm.op.attack","fm.op.decay","fm.op.vel_sens",
    "lfo.1.rate_hz","lfo.1.depth","fx.chorus.depth","fx.delay.time_ms","fx.delay.feedback",
    "fx.delay.mix","fx.reverb.size","fx.reverb.mix","fx.master_tone","mod.slot1.amount","fx.eq.low_db"
};
inline constexpr unsigned kMidiTargetCount = sizeof(kMidiTargets)/sizeof(*kMidiTargets);
static_assert(kMidiTargetCount < 256);
using MidiDestinations = std::array<std::array<uint8_t,8>,4>;

struct ControllerProfile {
    uint8_t bank = 0;
    std::array<MidiSource,16> knobs{};
    bool legacy = false;
    std::array<uint8_t,8> physical_b_destinations{};
    MidiDestinations destinations{};

    ControllerProfile() {
        for(auto& knob:knobs) knob.channel=16;
    }
};

constexpr unsigned kProfileV1Bytes = 36;
constexpr unsigned kProfileV2Bytes = 68;
constexpr unsigned kProfileBytes = 100;

inline ControllerProfile smk25Defaults() {
    ControllerProfile profile;
    profile.legacy = true;
    for (unsigned i = 0; i < profile.knobs.size(); ++i)
        profile.knobs[i] = {0, 0, uint8_t(21 + i)};
    return profile;
}
inline bool validProfile(const ControllerProfile& profile) {
    if(profile.bank>=4 || (profile.legacy && profile.bank!=0)) return false;
    for (auto target : profile.physical_b_destinations)
        if (target >= kMidiTargetCount || (!profile.legacy && target)) return false;
    for (const auto& bank : profile.destinations)
        for (const auto target : bank)
            if (target >= kMidiTargetCount) return false;
    for(unsigned i=0;i<profile.knobs.size();++i){
        const auto& knob=profile.knobs[i];
        if(knob.cable || knob.channel>16 || !amy::observableController(knob.controller)) return false;
        if(knob.channel==16) continue;
        if (!profile.legacy && i >= 8) return false;
        for(unsigned previous=0;previous<i;++previous) if(knob==profile.knobs[previous]) return false;
    }
    return true;
}

inline bool encodeProfile(const ControllerProfile& profile,uint8_t* bytes) {
    if(!validProfile(profile)) return false;
    edit::put32(bytes,0x34314d53);
    bytes[4]=3;
    bytes[5]=profile.bank;
    bytes[6]=0; // Pickup
    bytes[7]=profile.legacy ? 1 : 0;
    for(unsigned i=0;i<profile.knobs.size();++i){
        const auto& knob=profile.knobs[i];
        bytes[(i<8 ? 8 : 40)+3*i]=knob.cable;
        bytes[(i<8 ? 9 : 41)+3*i]=knob.channel;
        bytes[(i<8 ? 10 : 42)+3*i]=knob.controller;
    }
    for (unsigned bank = 0; bank < 4; ++bank)
        for (unsigned knob = 0; knob < 8; ++knob)
            bytes[32 + bank * 8 + knob] = profile.destinations[bank][knob];
    for (unsigned i=0;i<8;++i) bytes[88+i]=profile.physical_b_destinations[i];
    edit::put32(bytes+kProfileBytes-4,edit::crc(bytes,kProfileBytes-4));
    return true;
}

inline bool decodeProfile(const uint8_t* bytes,unsigned length,ControllerProfile& profile) {
    const unsigned version = length == kProfileV1Bytes ? 1 :
        length == kProfileV2Bytes ? 2 : length == kProfileBytes ? 3 : 0;
    if (!version || edit::get32(bytes) != 0x34314d53 ||
        bytes[4] != version || bytes[6] || bytes[7] > (version==3 ? 1 : 0) ||
        edit::get32(bytes + length - 4) != edit::crc(bytes, length - 4)) return false;
    ControllerProfile candidate;
    candidate.bank=bytes[5];
    candidate.legacy=bytes[7]!=0;
    for(unsigned i=0;i<(version==3 ? 16u : 8u);++i)
        candidate.knobs[i]={bytes[(i<8 ? 8 : 40)+3*i],bytes[(i<8 ? 9 : 41)+3*i],bytes[(i<8 ? 10 : 42)+3*i]};
    if (version >= 2)
        for (unsigned bank = 0; bank < 4; ++bank)
            for (unsigned knob = 0; knob < 8; ++knob)
                candidate.destinations[bank][knob] = bytes[32 + bank * 8 + knob];
    if (version==3)
        for (unsigned i=0;i<8;++i) candidate.physical_b_destinations[i]=bytes[88+i];
    if(!validProfile(candidate)) return false;
    profile=candidate;
    return true;
}

class ControllerProfileStore {
public:
    virtual ~ControllerProfileStore() = default;
    virtual bool load(ControllerProfile&) = 0;
    virtual bool save(const ControllerProfile&) = 0;
};

}
