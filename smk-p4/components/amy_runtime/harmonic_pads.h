#pragma once
#include "harmony.h"
#include <cstdio>
namespace smk::p4::pads {
constexpr unsigned kCount=8;
constexpr uint32_t kLeaseMs=2000;
struct Config { uint8_t root=0,scale=1,octave=4,inversion=0,open=0,spread=0; bool seventh=false; };
struct Intent { harmony::Config configuration; uint8_t input=0; bool valid=false; };
inline Intent resolve(unsigned pad,Config c) {
    Intent r;
    if(pad>=kCount || c.root>=12 || c.scale>=11 || c.octave>9 || harmony::degrees(c.scale)!=7) return r;
    unsigned offset=12;
    if(pad<7) {
        unsigned degree=0;
        for(unsigned i=0;i<12;++i) if(harmony::kScales[c.scale]&(1u<<i)) {
            if(degree++==pad) {offset=i;break;}
        }
    }
    const unsigned note=(unsigned(c.octave)+1)*12+c.root+offset;
    if(note>127) return r;
    auto& h=r.configuration;
    h.scale_on=h.chord_on=true;h.root=c.root;h.scale=c.scale;
    h.chord=c.seventh?13:12;h.inversion=c.inversion;h.open=c.open;h.spread=c.spread;
    r.input=uint8_t(note);r.valid=harmony::valid(h);return r;
}
inline constexpr const char* kPitchNames[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
inline const char* quality(const harmony::Notes& notes) {
    if(notes.count<3)return "?";
    // Pitch-class intervals identify the quality independently of voicing.
    unsigned mask=0;
    for(unsigned i=0;i<notes.count;++i) mask|=1u<<((notes.notes[i]+12-notes.quantized%12)%12);
    if((mask&((1u<<3)|(1u<<6)))==((1u<<3)|(1u<<6)))return "dim";
    if((mask&((1u<<4)|(1u<<8)))==((1u<<4)|(1u<<8)))return "aug";
    if(mask&(1u<<3))return "m";
    return "";
}
inline void name(const Intent& intent,char* text,unsigned capacity) {
    if(!intent.valid) {std::snprintf(text,capacity,"INDISPONIVEL");return;}
    const auto notes=harmony::transform(intent.input,intent.configuration);
    if(notes.error!=harmony::Error::None){std::snprintf(text,capacity,"FORA DA FAIXA");return;}
    std::snprintf(text,capacity,"%s%s",kPitchNames[intent.input%12],quality(notes));
}
enum class Result : uint8_t { None, Admitted, Capacity, Invalid, Released, Expired };
struct Status {
    uint32_t token=0,generation=0,active_token=0;
    Result result=Result::None;
    uint8_t notes[4]{},count=0;
};
struct State { Status pads[kCount]{}; uint8_t voices=0; bool available=false; };
}
