#pragma once
#include "sound_edit.h"
#include "patch_library.h"
#include <cstring>
#include <limits>
namespace smk::p4::edit {
constexpr unsigned kRecordBytes=128;
// V1 layout reserves exactly 17 floats; new RAM-only IDs are rejected.
constexpr unsigned kV1Count=17;
constexpr uint32_t kRomCompatibility=0x3e76efb6; // Qualified ROM SHA256 prefix.
static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559);
struct Preset { uint32_t sequence=0;uint16_t patch=0;uint8_t family=0;char name[25]{};uint8_t macros[3]{64,64,64};uint32_t mask=0;float values[Count]{}; };
inline uint32_t crc(const uint8_t* data,unsigned n){uint32_t c=~0u;for(unsigned i=0;i<n;++i){c^=data[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}return ~c;}
inline void put32(uint8_t* p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=uint8_t(v>>(i*8));}
inline uint32_t get32(const uint8_t* p){uint32_t v=0;for(unsigned i=0;i<4;++i)v|=uint32_t(p[i])<<(i*8);return v;}
inline bool validPreset(const Preset& p,bool v1){
    const auto* patch=library::find(p.patch);
    if(!p.sequence || !patch || !(patch->qualification&library::SUPPORTED) || p.family!=uint8_t(patch->family) ||
       (v1 ? (p.mask>>kV1Count) : (p.mask>>Count)) || !p.name[0] || p.name[24])return false;

    for(unsigned i=0;i<24 && p.name[i];++i)if(uint8_t(p.name[i])<32 || uint8_t(p.name[i])>126)return false;

    for(unsigned m=0;m<3;++m)if(p.macros[m]>127)return false;
    if(p.family==0 && (p.macros[1]!=64 || p.macros[2]!=64))return false;
    const uint32_t allowed=p.family==1?(15u | (v1?0u:((1u<<Decay)|(1u<<Sustain)))):(((1u<<kV1Count)-1)&~15u);
    if(p.mask&~allowed)return false;

    for(unsigned i=0;i<Count;++i)if((p.mask&(1u<<i)) && !valid(i,p.values[i]))return false;

    return true;
}
inline bool valid(const Preset& p){return validPreset(p,true);}
inline bool encode(const Preset& p,uint8_t* b){
    if(!valid(p))return false;

    std::memset(b,0,kRecordBytes);put32(b,0x31314553);put32(b+4,1);put32(b+8,p.sequence);
    put32(b+12,p.patch);put32(b+16,p.family);put32(b+20,p.mask);std::memcpy(b+24,p.name,25);
    for(unsigned m=0;m<3;++m)b[49+m]=p.macros[m];
    for(unsigned i=0;i<kV1Count;++i){uint32_t bits=0;const float v=p.mask&(1u<<i)?p.values[i]:0;std::memcpy(&bits,&v,4);put32(b+52+4*i,bits);}
    // ROM compatibility; future FX require a new format version.
    put32(b+120,kRomCompatibility);
    put32(b+124,crc(b,124));return true;
}
inline bool decode(const uint8_t* b,unsigned n,Preset& p){
    if(n!=kRecordBytes || get32(b)!=0x31314553 || get32(b+4)!=1 || get32(b+124)!=crc(b,124) ||
        get32(b+12)>255 || get32(b+16)>1 || get32(b+120)!=kRomCompatibility)return false;

    Preset candidate;candidate.sequence=get32(b+8);candidate.patch=uint16_t(get32(b+12));candidate.family=uint8_t(get32(b+16));
    for(unsigned m=0;m<3;++m)candidate.macros[m]=b[49+m];
    candidate.mask=get32(b+20);std::memcpy(candidate.name,b+24,25);
    for(unsigned i=0;i<kV1Count;++i){const uint32_t bits=get32(b+52+4*i);std::memcpy(&candidate.values[i],&bits,4);}
    if(!valid(candidate))return false;
    p=candidate;return true;
}
// Storage owned exclusively by the UI/control task. No audio calls this API.
class PresetStore {
public:
    virtual ~PresetStore()=default;
    virtual bool read(unsigned slot,unsigned bank,uint8_t* data,unsigned& length)=0;
    virtual bool write(unsigned slot,unsigned bank,const uint8_t* data,unsigned length)=0;
    virtual bool load(unsigned slot,Preset& out){
        if(slot>=kSlots)return false;
        Preset a,b;const bool av=readPreset(slot,0,a),bv=readPreset(slot,1,b);
        if(!av && !bv)return false;
        out=(!bv || (av && a.sequence>=b.sequence))?a:b;return true;
    }
    virtual bool save(unsigned slot,Preset p){
        if(slot>=kSlots)return false;
        Preset a,b;
        const bool av=readPreset(slot,0,a),bv=readPreset(slot,1,b);
        const uint32_t seq=av && (!bv || a.sequence>=b.sequence)?a.sequence:bv?b.sequence:0;
        if(seq==UINT32_MAX)return false;
        p.sequence=seq+1;
        const unsigned bank=!av?0:!bv?1:a.sequence<=b.sequence?0:1;
        uint8_t bytes[kRecordBytes];if(!encode(p,bytes) || !write(slot,bank,bytes,sizeof(bytes)))return false;

        Preset check;return readPreset(slot,bank,check) && check.sequence==p.sequence;
    }
private:
    bool readPreset(unsigned slot,unsigned bank,Preset& p){uint8_t b[kRecordBytes];unsigned n=sizeof(b);return read(slot,bank,b,n) && decode(b,n,p);}
};
}
