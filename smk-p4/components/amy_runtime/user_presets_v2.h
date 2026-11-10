#pragma once
#include "user_presets.h"
namespace smk::p4::edit {
// V2 sparse codec; V1 decoding and keys remain unchanged.
constexpr unsigned kV2RecordBytes=160;
inline unsigned maskPopcount(Mask m){
    unsigned c=0;for(;m;m&=m-1)++c;return c;
}
inline bool isFloatParamV2(unsigned i){
    return i==Depth || (i>=OpRatio0 && i<=OpRatio5) || i==LfoRate;
}
inline bool isByteParamV2(unsigned i){
    return (i>=LfoWave && i<=ModDest) || i==ModPolarity || i==FmAlgorithm;
}
inline bool isTimeParamV2(unsigned i){
    return i>=OpAttack0 && i<=OpDecay5;
}
inline bool encodeV2(const Preset& p,uint8_t* b){
    if(!validPreset(p,false))return false;
    const unsigned active=maskPopcount(p.mask);
    const uint8_t mode=active<=20?0:(active<=25?1:2);
    std::memset(b,0,kV2RecordBytes);put32(b,0x31314553);put32(b+4,2);
    put32(b+8,p.sequence);put32(b+12,p.patch);put32(b+16,p.family);
    const uint32_t hi=uint32_t(p.mask>>32);
    b[14]=uint8_t(hi>>24);b[15]=mode;
    b[17]=uint8_t(hi&0xffu);b[18]=uint8_t((hi>>8)&0xffu);b[19]=uint8_t((hi>>16)&0xffu);
    put32(b+20,uint32_t(p.mask));std::memcpy(b+24,p.name,25);
    for(unsigned m=0;m<3;++m)b[49+m]=p.macros[m];
    unsigned pos=52;
    for(unsigned i=0;i<Count;++i)if(p.mask&bit(i)){
        if(mode==0){
            if(pos+5>152)return false;
            b[pos++]=uint8_t(i);uint32_t bits;std::memcpy(&bits,&p.values[i],4);
            put32(b+pos,bits);pos+=4;
        }else if(mode==1){
            if(pos+4>152)return false;
            uint32_t bits;std::memcpy(&bits,&p.values[i],4);
            put32(b+pos,bits);pos+=4;
        }else{
            if(isFloatParamV2(i)){
                if(pos+4>152)return false;
                uint32_t bits;std::memcpy(&bits,&p.values[i],4);
                put32(b+pos,bits);pos+=4;
            }else if(isByteParamV2(i)){
                if(pos+1>152)return false;
                const float b_val=p.values[i];
                if(!std::isfinite(b_val) || b_val<0.0f || b_val>255.0f) return false;
                b[pos++]=uint8_t(std::lround(b_val));
            }else if(isTimeParamV2(i)){
                if(pos+2>152)return false;
                const float t_val=p.values[i]*16.0f;
                if(!std::isfinite(t_val) || t_val<0.0f || t_val>65535.0f) return false;
                const uint16_t u=uint16_t(std::lround(t_val));
                b[pos++]=uint8_t(u&0xffu);b[pos++]=uint8_t(u>>8);
            }else{
                if(pos+2>152)return false;
                const float s_val=p.values[i]*100.0f;
                if(!std::isfinite(s_val) || s_val<-32768.0f || s_val>32767.0f) return false;
                const uint16_t u=uint16_t(int16_t(std::lround(s_val)));
                b[pos++]=uint8_t(u&0xffu);b[pos++]=uint8_t(u>>8);
            }
        }
    }
    put32(b+152,kRomCompatibility);put32(b+156,crc(b,156));return true;
}
inline bool decodeV2(const uint8_t* b,unsigned n,Preset& out){
    if(n!=kV2RecordBytes || get32(b)!=0x31314553 || get32(b+4)!=2 ||
       get32(b+156)!=crc(b,156) || get32(b+152)!=kRomCompatibility)return false;
    const uint16_t patch_id=uint16_t(b[12]|(uint16_t(b[13])<<8));
    const uint8_t mode=b[15],family=b[16];
    if(patch_id>255 || family>1 || mode>2)return false;
    const uint32_t hi=uint32_t(b[17])|(uint32_t(b[18])<<8)|(uint32_t(b[19])<<16)|(uint32_t(b[14])<<24);
    const Mask mask=Mask(get32(b+20))|(Mask(hi)<<32);
    if(mask>>Count)return false;
    const unsigned active=maskPopcount(mask);
    if((mode==0 && active>20) || (mode==1 && (active<21 || active>25)) || (mode==2 && active<=25))return false;
    Preset p;p.sequence=get32(b+8);p.patch=patch_id;p.family=family;
    p.mask=mask;std::memcpy(p.name,b+24,25);
    for(unsigned m=0;m<3;++m)p.macros[m]=b[49+m];
    unsigned pos=52;
    for(unsigned i=0;i<Count;++i)if(p.mask&bit(i)){
        if(mode==0){
            if(pos+5>152 || b[pos++]!=i)return false;
            const uint32_t bits=get32(b+pos);pos+=4;std::memcpy(&p.values[i],&bits,4);
        }else if(mode==1){
            if(pos+4>152)return false;
            const uint32_t bits=get32(b+pos);pos+=4;std::memcpy(&p.values[i],&bits,4);
        }else{
            if(isFloatParamV2(i)){
                if(pos+4>152)return false;
                const uint32_t bits=get32(b+pos);pos+=4;std::memcpy(&p.values[i],&bits,4);
            }else if(isByteParamV2(i)){
                if(pos+1>152)return false;
                p.values[i]=float(b[pos++]);
            }else if(isTimeParamV2(i)){
                if(pos+2>152)return false;
                const uint16_t u=uint16_t(b[pos]|(uint16_t(b[pos+1])<<8));pos+=2;
                p.values[i]=float(u)/16.0f;
            }else{
                if(pos+2>152)return false;
                const int16_t s=int16_t(uint16_t(b[pos]|(uint16_t(b[pos+1])<<8)));pos+=2;
                p.values[i]=float(s)/100.0f;
            }
        }
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
    bool accepts(const Preset& p) const override {return validPreset(p,false);}
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
