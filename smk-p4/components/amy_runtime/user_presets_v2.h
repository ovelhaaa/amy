#pragma once
#include "user_presets.h"
#include "performance_factory_routes.h"
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
// V3 wraps an unchanged V2 payload and commits all eight positions with it.
constexpr unsigned kV3RecordBytes = 4 + kV2RecordBytes + performance::kMacroCount + 4;
constexpr uint32_t kV3Magic = 0x33314553;
inline bool encodeV3(const Preset& preset, uint8_t* bytes) {
    if (!performance::valid(preset.performance_macros) || !encodeV2(preset, bytes + 4)) return false;
    put32(bytes, kV3Magic);
    std::memcpy(bytes + 4 + kV2RecordBytes, preset.performance_macros.data(), performance::kMacroCount);
    put32(bytes + kV3RecordBytes - 4, crc(bytes, kV3RecordBytes - 4));
    return true;
}
inline bool decodeV3(const uint8_t* bytes, unsigned length, Preset& preset) {
    if (length != kV3RecordBytes || get32(bytes) != kV3Magic ||
        get32(bytes + length - 4) != crc(bytes, length - 4)) return false;
    Preset candidate;
    if (!decodeV2(bytes + 4, kV2RecordBytes, candidate)) return false;
    std::memcpy(candidate.performance_macros.data(), bytes + 4 + kV2RecordBytes, performance::kMacroCount);
    if (!performance::valid(candidate.performance_macros)) return false;
    preset = candidate;
    return true;
}

// V4 stores sorted overrides of factory slots within the Synth transaction.
// Each override uses slot + destination + flags + three IEEE-754 LE values.
constexpr uint32_t kV4Magic = 0x34314553;
constexpr unsigned kV4HeaderBytes = 5 + kV3RecordBytes;
constexpr unsigned kV4RouteBytes = 15;
constexpr unsigned kV4Slots = performance::kMacroCount * performance::kRoutesPerMacro;
constexpr unsigned kV4MaxBytes = kV4HeaderBytes + kV4Slots * kV4RouteBytes + 4;
static_assert(performance::EqHigh < 256 && kV4Slots < 256);

inline unsigned v4Size(const performance::RouteConfiguration& config) {
    if (!config.custom) return 0;
    unsigned overrides = 0;
    for (unsigned m = 0; m < performance::kMacroCount; ++m)
        for (unsigned i = 0; i < performance::kRoutesPerMacro; ++i)
            overrides += !(config.routes[m][i] == performance::kFactoryRoutes[m][i]);
    return overrides ? kV4HeaderBytes + overrides * kV4RouteBytes + 4 : 0;
}

inline bool encodeV4(const Preset& preset, uint8_t* bytes, unsigned length) {
    if (!length || length != v4Size(preset.performance_routes) ||
        !performance::valid(preset.performance_routes, preset.family, familyParameterMask(preset.family)) ||
        !encodeV3(preset, bytes + 5)) return false;
    put32(bytes, kV4Magic);
    bytes[4] = uint8_t((length - kV4HeaderBytes - 4) / kV4RouteBytes);
    unsigned offset = kV4HeaderBytes;
    for (unsigned slot = 0; slot < kV4Slots; ++slot) {
        const unsigned macro = slot / performance::kRoutesPerMacro;
        const unsigned index = slot % performance::kRoutesPerMacro;
        const auto& route = preset.performance_routes.routes[macro][index];
        if (route == performance::kFactoryRoutes[macro][index]) continue;
        bytes[offset++] = uint8_t(slot);
        bytes[offset++] = uint8_t(route.destination);
        bytes[offset++] = uint8_t(unsigned(route.curve) | (unsigned(route.polarity) << 2) |
            (unsigned(route.families) << 4) | (unsigned(route.enabled) << 6) | (unsigned(route.inverted) << 7));
        for (float value : {route.minimum, route.maximum, route.depth}) {
            uint32_t bits;
            std::memcpy(&bits, &value, sizeof(bits));
            put32(bytes + offset, bits);
            offset += 4;
        }
    }
    put32(bytes + offset, crc(bytes, offset));
    return true;
}

inline bool decodeV4(const uint8_t* bytes, unsigned length, Preset& preset) {
    if (length < kV4HeaderBytes + 4 || length > kV4MaxBytes || get32(bytes) != kV4Magic ||
        !bytes[4] || length != kV4HeaderBytes + bytes[4] * kV4RouteBytes + 4 ||
        get32(bytes + length - 4) != crc(bytes, length - 4)) return false;
    Preset candidate;
    if (!decodeV3(bytes + 5, kV3RecordBytes, candidate)) return false;
    candidate.performance_routes = {performance::kFactoryRoutes, true};
    unsigned offset = kV4HeaderBytes;
    unsigned next = 0;
    for (unsigned i = 0; i < bytes[4]; ++i) {
        const unsigned slot = bytes[offset++];
        if (slot < next || slot >= kV4Slots) return false;
        next = slot + 1;
        auto& route = candidate.performance_routes.routes[slot / performance::kRoutesPerMacro][slot % performance::kRoutesPerMacro];
        route.destination = bytes[offset++];
        const unsigned flags = bytes[offset++];
        route.curve = performance::Curve(flags & 3);
        route.polarity = performance::Polarity((flags >> 2) & 3);
        route.families = (flags >> 4) & 3;
        route.enabled = (flags & 64) != 0;
        route.inverted = (flags & 128) != 0;
        for (float* value : {&route.minimum, &route.maximum, &route.depth}) {
            const uint32_t bits = get32(bytes + offset);
            std::memcpy(value, &bits, sizeof(bits));
            offset += 4;
        }
    }
    if (v4Size(candidate.performance_routes) != length ||
        !performance::valid(candidate.performance_routes, candidate.family, familyParameterMask(candidate.family))) return false;
    preset = candidate;
    return true;
}

enum class BankRead : uint8_t { Present, Missing, Error };
class VersionedPresetStore : public PresetStore {
public:
    virtual BankRead readVersioned(unsigned version,unsigned slot,unsigned bank,uint8_t* data,unsigned& length)=0;
    virtual bool writeV2(unsigned slot,unsigned bank,const uint8_t* data,unsigned length)=0;
    virtual bool writeV3(unsigned,unsigned,const uint8_t*,unsigned) { return false; }
    virtual bool writeV4(unsigned,unsigned,const uint8_t*,unsigned) { return false; }
    bool accepts(const Preset& p) const override {return validPreset(p,false);}
    bool load(unsigned slot,Preset& out) override {
        Scan scan{};if(!scanSlot(slot,scan) || !scan.found)return false;
        out=scan.latest;return true;
    }
    bool save(unsigned slot,Preset p) override {
        if (p.performance_routes.custom) return false;
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
