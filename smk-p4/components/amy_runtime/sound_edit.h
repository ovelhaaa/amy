#pragma once
#include <atomic>
#include <cmath>
#include <cstdint>
namespace smk::p4::edit {
// Stable persisted IDs. Operator IDs follow materialized algo_source order.
enum Param : uint8_t { Cutoff, Resonance, Attack, Release, OpLevel0, OpLevel5=9,
    OpRatio0=10, OpRatio5=15, Depth=16, Decay=17, Sustain=18, Count=19 };
constexpr unsigned kSlots=16;
struct State {
    uint16_t patch=128;
    uint8_t family=0;
    uint32_t available=0,mask=0,revision=0,request=0,errors=0,epoch=0,apply_max_us=0;
    uint8_t macros[3]{64,64,64};
    float original[Count]{},manual[Count]{},applied[Count]{};
    bool compare=false,dirty=false;
    int8_t user_slot=-1;
};
inline bool valid(unsigned p,float v) {
    if(!std::isfinite(v))return false;
    if(p==Cutoff)return v>=20 && v<=18000;
    if(p==Resonance)return v>=0.1f && v<=2;
    if(p==Attack || p==Release)return v>=1 && v<=8000;
    if(p==Decay)return v>=1 && v<=30000;
    if(p>=OpLevel0 && p<=OpLevel5)return v>=0 && v<=100;
    if(p>=OpRatio0 && p<=OpRatio5)return v>=0.5f && v<=16;
    if(p==Sustain)return v>=0 && v<=100;
    return p==Depth && v>=0 && v<=1;
}
struct Request {
    uint16_t patch=0;
    uint32_t generation=0,request=0,mask=0;
    uint8_t macros[3]{64,64,64};
    float values[Count]{};
    bool compare=false;
    int8_t slot=-1;
};
// Single UI producer, AMY owner consumer. One request in flight; producer keeps
// latest desired complete state locally until space opens. Independent of notes.
class Mailbox {
public:
    bool push(const Request& r) {
        if(full_.load(std::memory_order_acquire))return false;
        request_=r;full_.store(true,std::memory_order_release);return true;
    }
    bool pop(Request& r) {
        if(!full_.load(std::memory_order_acquire))return false;
        r=request_;full_.store(false,std::memory_order_release);return true;
    }
private:
    Request request_{};
    std::atomic<bool> full_{false};
};
}
