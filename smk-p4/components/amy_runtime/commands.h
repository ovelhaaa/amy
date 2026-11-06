#pragma once
#include "runtime_config.h"
#include <atomic>
namespace smk::p4::amy {
enum class CommandType : uint8_t { NoteOn, Patch, Master, NoteOff, Control, Bend };
struct Command {
    CommandType type{};
    uint16_t id=0;
    uint8_t value=0;
    uint32_t generation=0, token=0, timestamp_ms=0;
    uint8_t channel=0;
};
// Exactly one producer per queue (UI or USB client), and one AMY owner.
// Touch Off and every Panic use independent atomic latches.
template<unsigned Capacity>
class CommandQueue {
public:
    bool push(Command c) {
        const uint32_t w=write_.load(std::memory_order_relaxed);
        if(w-read_.load(std::memory_order_acquire)>=Capacity) { ++overflows_; return false; }
        c.generation=generation(); slots_[w%Capacity]=c;
        write_.store(w+1,std::memory_order_release); return true;
    }
    bool pop(Command& c) {
        const uint32_t r=read_.load(std::memory_order_relaxed);
        if(r==write_.load(std::memory_order_acquire)) return false;
        c=slots_[r%Capacity]; read_.store(r+1,std::memory_order_release); return true;
    }
    void release(uint8_t note,uint32_t token) {
        if(note<128 && token && token>released_[note].load(std::memory_order_relaxed))
            released_[note].store(token,std::memory_order_release);
    }
    uint32_t nextToken() { return next_token_<0x7ffffffe?++next_token_:0; }
    uint32_t released(uint8_t note) const { return released_[note].load(std::memory_order_acquire); }
    // Owner publishes reason + generation in one lock-free word. Mismatch is unknown.
    std::atomic<uint32_t> output_context{0};
    void panic() { generation_.fetch_add(1,std::memory_order_acq_rel); }
    uint32_t generation() const { return generation_.load(std::memory_order_acquire); }
    uint32_t overflows() const { return overflows_.load(std::memory_order_relaxed); }
    uint32_t depth() const {
        const uint32_t r=read_.load(std::memory_order_acquire);
        const uint32_t depth=write_.load(std::memory_order_acquire)-r;
        return depth>Capacity?Capacity:depth;
    }
private:
    static_assert(std::atomic<uint32_t>::is_always_lock_free,"32-bit atomics required");
    Command slots_[Capacity]{};
    uint32_t next_token_=0; // Producer lifetime, survives UI/backend/engine restart.
    std::atomic<uint32_t> read_{0},write_{0},generation_{1},overflows_{0},released_[128]{};
};
using Commands = CommandQueue<kCommandCapacity>;
inline bool atOrAfter(uint32_t a,uint32_t b) { return static_cast<int32_t>(a-b)>=0; }
// The owner samples its clock before draining. A concurrent producer can
// timestamp a command just after that sample; it is fresh, not UINT32_MAX ms
// old. Modular ordering also preserves normal clock wrap (intervals <2^31 ms).
inline uint32_t eventAgeMs(uint32_t now_ms,uint32_t timestamp_ms) {
    return atOrAfter(now_ms,timestamp_ms)?now_ms-timestamp_ms:0;
}
}
