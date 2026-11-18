#pragma once

#include "performance_scene.h"

namespace smk::p4::scene {

constexpr uint32_t kMagic=0x314e4353; // SCN1
constexpr unsigned kHeaderBytes=16, kCrcBytes=4, kLegacyPayloadBytes=72, kPayloadBytes=76;
constexpr unsigned kRouteBytes=15, kRouteSlots=performance::kMacroCount*performance::kRoutesPerMacro;
constexpr unsigned kMaxBytes=kHeaderBytes+kPayloadBytes+kRouteBytes*kRouteSlots+kCrcBytes;
static_assert(sizeof(float)==4 && performance::EqHigh<256);

struct Slot {
    Configuration config;
    uint32_t sequence=0;
    seq::SlotState state=seq::SlotState::Empty;
};

inline unsigned encode(const Slot& slot,uint8_t* bytes) {
    using edit::put32;
    if(!slot.sequence || (slot.state!=seq::SlotState::Valid && slot.state!=seq::SlotState::Empty)) return 0;
    const auto& c=slot.config;
    unsigned size=kHeaderBytes;
    std::memset(bytes,0,kHeaderBytes);
    put32(bytes,kMagic);
    put32(bytes+4,2);
    put32(bytes+8,slot.sequence);
    if(slot.state==seq::SlotState::Valid) {
        if(!c.domains || (c.domains&~kAll) || !seq::validName(c.name) ||
            ((c.domains&Pattern) && c.pattern_slot>=seq::kPatternSlots) ||
            ((c.domains&Harmony) && !harmony::valid(c.harmony)) ||
            ((c.domains&Arp) && !validArp(c.arp)) ||
            ((c.domains&Macros) && !performance::valid(c.macros)) ||
            ((c.domains&Tempo) && (c.bpm<tempo::kMinBpm || c.bpm>tempo::kMaxBpm))) return 0;
        bytes[12]=c.domains;
        auto* p=bytes+kHeaderBytes;
        std::memset(p,0,kPayloadBytes);
        std::memcpy(p,c.name,sizeof(c.name));
        if(c.domains&Patch) { p[17]=uint8_t(c.patch_id); p[18]=uint8_t(c.patch_id>>8); }
        if(c.domains&Pattern) {
            p[19]=c.pattern_slot;
            put32(p+kLegacyPayloadBytes,c.pattern_sequence);
        }
        if(c.domains&Tempo) { p[20]=uint8_t(c.bpm); p[21]=uint8_t(c.bpm>>8); }
        if((c.domains&Fx) && !fx::encodeRecord(c.fx,0,p+22)) return 0;
        if(c.domains&Harmony) put32(p+54,harmony::pack(c.harmony));
        if(c.domains&Arp) {
            p[58]=c.arp.octaves; p[59]=c.arp.gate; p[60]=c.arp.swing;
            p[61]=uint8_t(c.arp.mode); p[62]=uint8_t(c.arp.rate);
            p[63]=uint8_t(unsigned(c.arp.enabled) | (unsigned(c.arp.latch)<<1) | (unsigned(c.arp.restart)<<2));
        }
        size+=kPayloadBytes;
        if(c.domains&Macros) {
            std::copy(c.macros.begin(),c.macros.end(),p+64);
            for(unsigned i=0;c.routes.custom && i<kRouteSlots;++i) {
                const auto macro=i/performance::kRoutesPerMacro;
                const auto index=i%performance::kRoutesPerMacro;
                const auto& r=c.routes.routes[macro][index];
                if(r==performance::kFactoryRoutes[macro][index]) continue;
                if(r.destination>performance::EqHigh || r.curve>performance::Curve::SCurve ||
                    r.polarity>performance::Polarity::Negative || !r.families || r.families>performance::Both ||
                    !std::isfinite(r.minimum) || !std::isfinite(r.maximum) || !std::isfinite(r.depth)) return 0;
                ++bytes[13];
                bytes[size++]=uint8_t(i);
                bytes[size++]=uint8_t(r.destination);
                bytes[size++]=uint8_t(unsigned(r.curve) | (unsigned(r.polarity)<<2) |
                    (unsigned(r.families)<<4) | (unsigned(r.enabled)<<6) | (unsigned(r.inverted)<<7));
                for(const float value : {r.minimum,r.maximum,r.depth}) {
                    uint32_t bits;
                    std::memcpy(&bits,&value,sizeof(bits));
                    put32(bytes+size,bits);
                    size+=4;
                }
            }
        }
    }
    put32(bytes+size,edit::crc(bytes,size));
    return size+kCrcBytes;
}

inline Slot decode(const uint8_t* bytes,unsigned size) {
    using edit::get32;
    Slot slot;
    slot.state=seq::SlotState::Corrupted;
    if(size<8 || get32(bytes)!=kMagic) return slot;
    const auto version=get32(bytes+4);
    if(version!=1 && version!=2) { slot.state=seq::SlotState::Incompatible; return slot; }
    const auto payload=version==1?kLegacyPayloadBytes:kPayloadBytes;
    if(size<kHeaderBytes+kCrcBytes || size>kMaxBytes ||
        get32(bytes+size-kCrcBytes)!=edit::crc(bytes,size-kCrcBytes) ||
        !get32(bytes+8) || bytes[14] || bytes[15] || (bytes[12]&~kAll)) return slot;
    const auto domains=bytes[12];
    const unsigned count=bytes[13];
    if(count>kRouteSlots || (count && !(domains&Macros)) ||
        size!=kHeaderBytes+(domains?payload:0)+count*kRouteBytes+kCrcBytes) return slot;
    if(!domains) {
        slot.state=seq::SlotState::Empty;
        slot.sequence=get32(bytes+8);
        return slot;
    }

    auto& c=slot.config;
    c.domains=domains;
    const auto* p=bytes+kHeaderBytes;
    std::memcpy(c.name,p,sizeof(c.name));
    if(!seq::validName(c.name)) return slot;
    if(domains&Patch) c.patch_id=uint16_t(p[17] | (unsigned(p[18])<<8));
    if(domains&Pattern) {
        c.pattern_slot=p[19];
        if(version==2) c.pattern_sequence=get32(p+kLegacyPayloadBytes);
        if(c.pattern_slot>=seq::kPatternSlots) return slot;
    }
    if(domains&Tempo) {
        c.bpm=uint16_t(p[20] | (unsigned(p[21])<<8));
        if(c.bpm<tempo::kMinBpm || c.bpm>tempo::kMaxBpm) return slot;
    }
    if(domains&Fx) {
        uint32_t sequence=0;
        if(!fx::decodeRecord(p+22,fx::kFxRecordBytes,c.fx,sequence) || sequence) return slot;
    }
    if(domains&Harmony) {
        const auto packed=get32(p+54);
        c.harmony=harmony::unpack(packed);
        if(packed>>27 || !harmony::valid(c.harmony)) return slot;
    }
    if(domains&Arp) {
        c.arp={p[58],p[59],p[60],arp::Mode(p[61]),arp::Rate(p[62]),bool(p[63]&1),bool(p[63]&2),bool(p[63]&4)};
        if(p[63]>7 || !validArp(c.arp)) return slot;
    }
    if(domains&Macros) {
        std::copy_n(p+64,c.macros.size(),c.macros.begin());
        if(!performance::valid(c.macros)) return slot;
        if(count) c.routes={performance::kFactoryRoutes,true};
        unsigned next=0;
        p+=payload;
        for(unsigned i=0;i<count;++i,p+=kRouteBytes) {
            const unsigned index=p[0];
            if(index<next || index>=kRouteSlots || p[1]>performance::EqHigh) return slot;
            next=index+1;
            auto& r=c.routes.routes[index/performance::kRoutesPerMacro][index%performance::kRoutesPerMacro];
            r.destination=p[1]; r.curve=performance::Curve(p[2]&3);
            r.polarity=performance::Polarity((p[2]>>2)&3); r.families=(p[2]>>4)&3;
            r.enabled=(p[2]&64)!=0; r.inverted=(p[2]&128)!=0;
            if(r.polarity>performance::Polarity::Negative || !r.families) return slot;
            unsigned offset=3;
            for(float* value : {&r.minimum,&r.maximum,&r.depth}) {
                const auto bits=get32(p+offset);
                std::memcpy(value,&bits,sizeof(bits));
                if(!std::isfinite(*value)) return slot;
                offset+=4;
            }
        }
    }
    slot.sequence=get32(bytes+8);
    slot.state=seq::SlotState::Valid;
    return slot;
}

// Control-side store. Serialize access with other NVS clients through Storage Quiet.
class Store {
public:
    virtual ~Store()=default;
    Slot load(unsigned index) { return scan(index).slot; }

    bool commit(unsigned index,const Slot& value) {
        const auto current=scan(index);
        if(current.blocked || current.slot.sequence==UINT32_MAX) return false;
        auto next=value;
        next.sequence=current.slot.sequence+1;
        uint8_t bytes[kMaxBytes],check[kMaxBytes];
        const auto size=encode(next,bytes);
        const auto bank=1-current.bank;
        if(!size || !write(index,bank,bytes,size)) return false;
        unsigned found=sizeof(check);
        return read(index,bank,check,found)==seq::ReadResult::Present && found==size &&
            std::memcmp(bytes,check,size)==0;
    }

protected:
    virtual seq::ReadResult read(unsigned index,unsigned bank,uint8_t* bytes,unsigned& size)=0;
    virtual bool write(unsigned index,unsigned bank,const uint8_t* bytes,unsigned size)=0;

private:
    struct Scan { Slot slot; unsigned bank=1; bool blocked=false; };
    Scan scan(unsigned index) {
        Scan current;
        if(index>=kSlots) {
            current.slot.state=seq::SlotState::Corrupted;
            current.blocked=true;
            return current;
        }
        bool unknown=false;
        for(unsigned bank=0;bank<2;++bank) {
            uint8_t bytes[kMaxBytes];
            unsigned size=sizeof(bytes);
            const auto status=read(index,bank,bytes,size);
            if(status==seq::ReadResult::Missing) continue;
            const auto candidate=status==seq::ReadResult::Present?decode(bytes,size):Slot{};
            unknown|=status==seq::ReadResult::Incompatible || candidate.state==seq::SlotState::Incompatible;
            current.blocked|=status==seq::ReadResult::Error;
            if(candidate.sequence>current.slot.sequence) {
                current.slot=candidate;
                current.bank=bank;
            } else if(!current.slot.sequence) current.slot.state=seq::SlotState::Corrupted;
        }
        if(current.blocked) current.slot.state=seq::SlotState::Corrupted;
        if(unknown) current.slot.state=seq::SlotState::Incompatible;
        current.blocked|=unknown;
        return current;
    }
};

}
