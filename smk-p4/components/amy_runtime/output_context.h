#pragma once
#include <cstdint>
namespace smk::p4::amy {
enum class OutputReason : uint8_t { Boot, Ready, Patch, Panic, Failed, Unknown };
constexpr uint32_t outputContext(uint32_t generation,OutputReason reason,bool active=false) {
    return ((generation&0x0fffffff)<<4)|(active?8u:0u)|uint32_t(reason);
}
struct GapCounts {
    uint32_t boot=0,patch=0,panic=0,stale=0,unexpected_active=0,other=0;
    void add(uint32_t context,uint32_t generation,bool discarded) {
        if((context>>4)!=(generation&0x0fffffff)){++other;return;}
        const auto reason=OutputReason(context&7);
        if(reason==OutputReason::Boot)++boot;
        else if(reason==OutputReason::Patch)++patch;
        else if(reason==OutputReason::Panic)++panic;
        else if(discarded)++stale;
        else if(reason==OutputReason::Ready && (context&8))++unexpected_active;
        else ++other;
    }
};
}
