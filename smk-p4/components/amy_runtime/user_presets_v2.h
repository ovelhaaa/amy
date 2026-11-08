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
        if(pos+5>152)return false;
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
        if(pos+5>152 || b[pos++]!=i)return false;
        const uint32_t bits=get32(b+pos);pos+=4;std::memcpy(&p.values[i],&bits,4);
    }
    if(!validPreset(p,false))return false;
    out=p;return true;
}
// A storage error must never be confused with an absent or corrupt key.
enum class BankRead : uint8_t { Present, Missing, Error };
class VersionedPresetStore : public PresetStore {
public:
    virtual BankRead readVersioned(unsigned version,unsigned slot,unsigned bank,uint8_t* data,unsigned& length)=0;
    virtual bool writeV2(unsigned slot,unsigned bank,const uint8_t* data,unsigned length)=0;
    bool load(unsigned slot,Preset& out) override {
        Scan scan{};if(!scanSlot(slot,scan) || !scan.found)return false;
        out=scan.latest;return true;
    }
    bool save(unsigned slot,Preset p) override {
        Scan scan{};if(!scanSlot(slot,scan))return false;
        if(scan.found && scan.latest.sequence==UINT32_MAX)return false;
        p.sequence=scan.found?scan.latest.sequence+1:1;
        const unsigned bank=!scan.valid[0]?0:!scan.valid[1]?1:
            scan.modern[0].sequence<=scan.modern[1].sequence?0:1;
        uint8_t bytes[kV2RecordBytes];
        if(!encodeV2(p,bytes) || !writeV2(slot,bank,bytes,sizeof(bytes)))return false;
        uint8_t check[kV2RecordBytes];unsigned length=sizeof(check);
        return readVersioned(2,slot,bank,check,length)==BankRead::Present &&
               length==sizeof(check) && std::memcmp(check,bytes,sizeof(check))==0;
    }
private:
    struct Scan {Preset latest,modern[2];bool found=false,valid[2]{};};
    bool scanSlot(unsigned slot,Scan& scan){
        if(slot>=kSlots)return false;
        for(unsigned version=1;version<=2;++version)for(unsigned bank=0;bank<2;++bank){
            uint8_t bytes[kV2RecordBytes];unsigned length=sizeof(bytes);
            const auto result=readVersioned(version,slot,bank,bytes,length);
            if(result==BankRead::Error)return false;
            if(result==BankRead::Missing)continue;
            // A recognizable future record must survive an older firmware.
            if(length>=8 && get32(bytes)==0x31314553 && get32(bytes+4)>2)return false;
            Preset p;const bool valid=version==1?decode(bytes,length,p):decodeV2(bytes,length,p);
            if(!valid)continue;
            if(version==2){scan.valid[bank]=true;scan.modern[bank]=p;}
            // Equal sequence prefers the first V1 bank, preserving rollback.
            if(!scan.found || p.sequence>scan.latest.sequence){scan.latest=p;scan.found=true;}
        }
        return true;
    }
};
}
