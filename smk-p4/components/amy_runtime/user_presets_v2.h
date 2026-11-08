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
// Separate banks retain every V1 key for rollback.
class VersionedPresetStore : public PresetStore {
public:
    virtual bool readV2(unsigned slot,unsigned bank,uint8_t* data,unsigned& length)=0;
    virtual bool writeV2(unsigned slot,unsigned bank,const uint8_t* data,unsigned length)=0;
    bool load(unsigned slot,Preset& out) override {
        if(slot>=kSlots)return false;
        Preset v1,a,b;const bool old=PresetStore::load(slot,v1);
        const bool av=readBank(slot,0,a),bv=readBank(slot,1,b);
        bool found=old;if(old)out=v1;
        if(av && (!found || a.sequence>out.sequence)){out=a;found=true;}
        if(bv && (!found || b.sequence>out.sequence)){out=b;found=true;}
        return found;
    }
    bool save(unsigned slot,Preset p) override {
        if(slot>=kSlots)return false;
        Preset latest,a,b;const bool found=load(slot,latest);
        if(found && latest.sequence==UINT32_MAX)return false;
        p.sequence=found?latest.sequence+1:1;
        const bool av=readBank(slot,0,a),bv=readBank(slot,1,b);
        const unsigned bank=!av?0:!bv?1:a.sequence<=b.sequence?0:1;
        uint8_t bytes[kV2RecordBytes];
        if(!encodeV2(p,bytes) || !writeV2(slot,bank,bytes,sizeof(bytes)))return false;
        uint8_t check[kV2RecordBytes];unsigned length=sizeof(check);
        return readV2(slot,bank,check,length) && length==sizeof(check) &&
               std::memcmp(check,bytes,sizeof(check))==0;
    }
private:
    bool readBank(unsigned slot,unsigned bank,Preset& p){
        uint8_t bytes[kV2RecordBytes];unsigned length=sizeof(bytes);
        return readV2(slot,bank,bytes,length) && decodeV2(bytes,length,p);
    }
};

}
