#pragma once

#include "performance_scene.h"
#include <atomic>

namespace smk::p4::scene {

enum class RecallPhase : uint8_t { Idle, WaitingBoundary, Confirmed, Failed, Cancelled };
enum class RecallError : uint8_t {
    None, InvalidRequest, UnsupportedDomains, Stale, Busy, Cancelled, Verification
};
enum class RecallMode : uint8_t { Immediate, NextBar };

struct RecallRequest {
    Configuration configuration;
    seq::Slot pattern;
    seq::PatternSettings expected_pattern;
    uint32_t token=0, generation=0;
    uint32_t fx_revision=0, harmony_revision=0, arp_revision=0, tempo_revision=0;
    uint8_t slot=seq::kNoSlot, requested=0, locks=0, expected_pattern_slot=seq::kNoSlot;
    RecallMode mode=RecallMode::Immediate;
    bool apply_tempo=false, discard_pattern=false;
};

struct RecallStatus {
    RecallPhase phase=RecallPhase::Idle;
    RecallError error=RecallError::None;
    Reject rejection=Reject::None;
    uint32_t token=0, generation=0, revision=0;
    uint32_t pattern_sequence=0;
    uint8_t active=seq::kNoSlot, pending=seq::kNoSlot, effective=0, active_domains=0;
    uint64_t due_phase=0, applied_phase=0;
};

// One control producer. The owner retains the slot until its terminal result,
// so a concurrent submission is rejected without overwriting the plan.
class RecallMailbox {
public:
    bool push(const RecallRequest& request) {
        if(!request.token || pending_.load(std::memory_order_acquire)) return false;
        cancel_.store(0,std::memory_order_relaxed);
        request_=request;
        pending_.store(true,std::memory_order_release);
        return true;
    }
    void cancel(uint32_t token) { cancel_.store(token,std::memory_order_release); }
    bool cancelled(uint32_t token) const { return cancel_.load(std::memory_order_acquire)==token; }
    const RecallRequest* front() const {
        return pending_.load(std::memory_order_acquire)?&request_:nullptr;
    }
    void consume() { pending_.store(false,std::memory_order_release); }
private:
    static_assert(std::atomic<bool>::is_always_lock_free && std::atomic<uint32_t>::is_always_lock_free);
    RecallRequest request_;
    std::atomic<bool> pending_{false};
    std::atomic<uint32_t> cancel_{0};
};

}
