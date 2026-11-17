#pragma once

#include "performance_scene.h"
#include "factory_edit_capabilities.h"
#include "patch_library.h"
#include <atomic>
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#endif

namespace smk::p4::scene {

enum class RecallPhase : uint8_t { Idle, WaitingBoundary, Confirmed, Failed, Cancelled, PartialApply, WaitingPatch };
enum class RecallError : uint8_t {
    None, InvalidRequest, UnsupportedDomains, Stale, Busy, Cancelled, Verification, FxConflict,
    PatchFailed, PatchTimeout
};
enum class RecallMode : uint8_t { Immediate, NextBar };

struct RecallRequest {
    Configuration configuration;
    seq::Slot pattern;
    seq::PatternSettings expected_pattern;
    uint32_t token=0, generation=0;
    uint32_t fx_revision=0, harmony_revision=0, arp_revision=0, tempo_revision=0, edit_revision=0;
    uint8_t slot=seq::kNoSlot, requested=0, locks=0, expected_pattern_slot=seq::kNoSlot;
    uint8_t resolved_pattern_slot=seq::kNoSlot;
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
    uint8_t applied_domains=0;
    int patch_error=0;
    uint64_t due_phase=0, applied_phase=0;
#ifdef CONFIG_SMKP4_HARDWARE_017B_TELEMETRY
    uint32_t bench_units_per_frame=0, bench_request_ms=0, bench_start_ms=0;
    uint32_t bench_start_us=0, bench_elapsed_us=0;
    bool bench_started=false;
#endif
};

inline Selection validateRecall(const RecallRequest& request,const edit::State& resident,
                                bool patch_available,const seq::Slot* pattern) {
    const auto domains=effectiveDomains(request.configuration.domains,request.requested,
                                        request.locks,request.apply_tempo);
    const auto patch_id=request.configuration.patch_id;
    const bool swap=(domains&Patch) && patch_available && patch_id!=resident.patch;
    const auto* patch=swap?library::find(patch_id):nullptr;
    const auto family=patch?uint8_t(patch->family):resident.family;
    const auto available=patch?edit::factoryPatchCapabilities(patch_id):resident.available;
    if(request.configuration.pattern_sequence && request.resolved_pattern_slot!=request.configuration.pattern_slot)
        pattern=nullptr;
    auto selection=validate(request.configuration,request.requested,request.locks,request.apply_tempo,
                            patch_available,pattern,family,available);
    if(selection.reason==Reject::None && (domains&Macros) && resident.compare)
        selection.reason=Reject::CompareActive;
    if(selection.reason==Reject::None && swap && !(domains&Macros) &&
       !performance::valid(resident.performance_routes,family,available))
        selection.reason=Reject::IncompatibleMacros;
    return selection;
}

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
