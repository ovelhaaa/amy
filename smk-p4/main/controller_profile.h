#pragma once

#include "performance_midi.h"
#include "user_presets.h"
#include <array>

namespace smk::p4::performance {

struct ControllerProfile {
    uint8_t bank = 0;
    std::array<MidiSource,8> knobs{};

    ControllerProfile() {
        for(auto& knob:knobs) knob.channel=16;
    }
};

constexpr unsigned kProfileBytes = 36;
inline bool validProfile(const ControllerProfile& profile) {
    if(profile.bank>=4) return false;
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
    bytes[4]=1; // Version
    bytes[5]=profile.bank;
    bytes[6]=0; // Pickup
    bytes[7]=0;
    for(unsigned i=0;i<profile.knobs.size();++i){
        const auto& knob=profile.knobs[i];
        bytes[8+3*i]=knob.cable;
        bytes[9+3*i]=knob.channel;
        bytes[10+3*i]=knob.controller;
    }
    edit::put32(bytes+32,edit::crc(bytes,32));
    return true;
}

inline bool decodeProfile(const uint8_t* bytes,unsigned length,ControllerProfile& profile) {
    if(length!=kProfileBytes || edit::get32(bytes)!=0x34314d53 ||
       bytes[4]!=1 || bytes[6] || bytes[7] || edit::get32(bytes+32)!=edit::crc(bytes,32)) return false;
    ControllerProfile candidate;
    candidate.bank=bytes[5];
    for(unsigned i=0;i<candidate.knobs.size();++i)
        candidate.knobs[i]={bytes[8+3*i],bytes[9+3*i],bytes[10+3*i]};
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
