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
    "fx.delay.mix","fx.reverb.size","fx.reverb.mix","fx.master_tone","mod.slot1.amount"
};
inline constexpr unsigned kMidiTargetCount = sizeof(kMidiTargets)/sizeof(*kMidiTargets);
static_assert(kMidiTargetCount < 256);
using MidiDestinations = std::array<std::array<uint8_t,8>,4>;

struct ControllerProfile {
    uint8_t bank = 0;
    std::array<MidiSource,8> knobs{};
    MidiDestinations destinations{};

    ControllerProfile() {
        for(auto& knob:knobs) knob.channel=16;
    }
};

constexpr unsigned kProfileV1Bytes = 36;
constexpr unsigned kProfileBytes = 68;
inline bool validProfile(const ControllerProfile& profile) {
    if(profile.bank>=4) return false;
    for (const auto& bank : profile.destinations)
        for (const auto target : bank)
            if (target >= kMidiTargetCount) return false;
    for(unsigned i=0;i<profile.knobs.size();++i){
        const auto& knob=profile.knobs[i];
        if(knob.cable || knob.channel>16 || !amy::observableController(knob.controller)) return false;
        if(knob.channel==16) continue;
        for(unsigned previous=0;previous<i;++previous) if(knob==profile.knobs[previous]) return false;
    }
    return true;
}

inline bool encodeProfile(const ControllerProfile& profile,uint8_t* bytes) {
    if(!validProfile(profile)) return false;
    edit::put32(bytes,0x34314d53);
    bytes[4]=2;
    bytes[5]=profile.bank;
    bytes[6]=0; // Pickup
    bytes[7]=0;
    for(unsigned i=0;i<profile.knobs.size();++i){
        const auto& knob=profile.knobs[i];
        bytes[8+3*i]=knob.cable;
        bytes[9+3*i]=knob.channel;
        bytes[10+3*i]=knob.controller;
    }
    for (unsigned bank = 0; bank < 4; ++bank)
        for (unsigned knob = 0; knob < 8; ++knob)
            bytes[32 + bank * 8 + knob] = profile.destinations[bank][knob];
    edit::put32(bytes+kProfileBytes-4,edit::crc(bytes,kProfileBytes-4));
    return true;
}

inline bool decodeProfile(const uint8_t* bytes,unsigned length,ControllerProfile& profile) {
    if ((length != kProfileV1Bytes && length != kProfileBytes) || edit::get32(bytes) != 0x34314d53 ||
        bytes[4] != (length == kProfileV1Bytes ? 1 : 2) || bytes[6] || bytes[7] ||
        edit::get32(bytes + length - 4) != edit::crc(bytes, length - 4)) return false;
    ControllerProfile candidate;
    candidate.bank=bytes[5];
    for(unsigned i=0;i<candidate.knobs.size();++i)
        candidate.knobs[i]={bytes[8+3*i],bytes[9+3*i],bytes[10+3*i]};
    if (length == kProfileBytes)
        for (unsigned bank = 0; bank < 4; ++bank)
            for (unsigned knob = 0; knob < 8; ++knob)
                candidate.destinations[bank][knob] = bytes[32 + bank * 8 + knob];
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
