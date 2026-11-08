#pragma once
#include "user_presets.h"
namespace smk::p4::edit {
// Codec only: storage admission and non-destructive separate keys are pending.
constexpr unsigned kV2RecordBytes=160;
inline bool encodeV2(const Preset& p,uint8_t* b){
    if(!validPreset(p,false))return false;
    std::memset(b,0,kV2RecordBytes);put32(b,0x31314553);put32(b+4,2);
    put32(b+8,p.sequence);put32(b+12,p.patch);put32(b+16,p.family);
    put32(b+20,p.mask);std::memcpy(b+24,p.name,25);
    for(unsigned m=0;m<3;++m)b[49+m]=p.macros[m];
    unsigned pos=52;
    for(unsigned i=0;i<Count;++i)if(p.mask&(1u<<i)){
        b[pos++]=uint8_t(i);uint32_t bits;std::memcpy(&bits,&p.values[i],4);
        put32(b+pos,bits);pos+=4;
    }
    put32(b+152,kRomCompatibility);put32(b+156,crc(b,156));return true;
}
inline bool decodeV2(const uint8_t* b,unsigned n,Preset& out){
    if(n!=kV2RecordBytes || get32(b)!=0x31314553 || get32(b+4)!=2 ||
       get32(b+156)!=crc(b,156) || get32(b+152)!=kRomCompatibility ||
       get32(b+12)>255 || get32(b+16)>1)return false;
    Preset p;p.sequence=get32(b+8);p.patch=uint16_t(get32(b+12));p.family=uint8_t(get32(b+16));
    p.mask=get32(b+20);std::memcpy(p.name,b+24,25);
    for(unsigned m=0;m<3;++m)p.macros[m]=b[49+m];
    if(p.mask>>Count)return false;
    unsigned pos=52;
    for(unsigned i=0;i<Count;++i)if(p.mask&(1u<<i)){
        if(b[pos++]!=i)return false;
        const uint32_t bits=get32(b+pos);pos+=4;std::memcpy(&p.values[i],&bits,4);
    }
    if(!validPreset(p,false))return false;
    out=p;return true;
}
}
