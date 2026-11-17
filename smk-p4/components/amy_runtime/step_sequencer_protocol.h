#pragma once

#include "step_sequencer.h"
#include <atomic>

namespace smk::p4::seq {

constexpr uint8_t kProtocolVersion=3;
constexpr uint32_t kConfirmationTimeoutMs=2000;
constexpr unsigned kQueueCapacity=4;
enum class Action : uint8_t {
    Play, Stop, Restart, SetStep, SetLength, SetDivision, SetPattern,
    SetEnabled, SetSwing, SetTranspose, SetQuantize,
    SelectStep, SetInputMode, SetInputChannel, SetLiveChannel, SetRecordGate,
    SetReference, SetLive, SetThru, SetSmkOnly, Rest, SetStepNote, SetStepVelocity, SetStepGate,
    SetStepEnabled, ShiftStepNote, MoveStep, ToggleStepEnabled,
    LoadSlot, MarkSlotSaved, DetachSlot, CancelSwitch
};

struct Request {
    Pattern pattern;
    Step step;
    uint32_t token=0, generation=0;
    uint32_t submitted_ms=0;
    int16_t value=0;
    uint8_t index=0, version=kProtocolVersion;
    tempo::Division division=tempo::Division::Sixteenth;
    uint8_t swing=50;
    int8_t transpose=0;
    bool quantize=false, discard=false;
    Action action=Action::Play;
    bool select=false, timed=false;

    PatternSettings settings() const { return {pattern,division,swing,transpose,quantize}; }
    void setSettings(const PatternSettings& s) {
        pattern=s.pattern; division=s.division; swing=s.swing; transpose=s.transpose; quantize=s.quantize;
    }
};

// SPSC, with an independent Stop slot. The returned entry remains owned by
// the consumer until consume(), so the owner need not copy a full pattern.
class Mailbox {
public:
    bool push(const Request& request,uint32_t generation) {
        if(request.action==Action::Stop) {
            if(stop_pending_.load(std::memory_order_acquire)) { ++overflows_; return false; }
            stop_=request;
            stop_.generation=generation;
            stop_pending_.store(true,std::memory_order_release);
            return true;
        }
        const auto write=write_.load(std::memory_order_relaxed);
        if(write-read_.load(std::memory_order_acquire)>=kQueueCapacity) { ++overflows_; return false; }
        auto& slot=entries_[write%kQueueCapacity];
        slot=request;
        slot.generation=generation;
        write_.store(write+1,std::memory_order_release);
        return true;
    }

    const Request* front() const {
        const auto read=read_.load(std::memory_order_relaxed);
        return read!=write_.load(std::memory_order_acquire)?&entries_[read%kQueueCapacity]:nullptr;
    }
    void consume() {
        read_.store(read_.load(std::memory_order_relaxed)+1,std::memory_order_release);
    }

    const Request* stop() const {
        return stop_pending_.load(std::memory_order_acquire)?&stop_:nullptr;
    }
    void consumeStop() { stop_pending_.store(false,std::memory_order_release); }

    uint32_t overflows() const { return overflows_.load(std::memory_order_relaxed); }

private:
    std::atomic<uint32_t> overflows_{0};
    static_assert(std::atomic<uint32_t>::is_always_lock_free && std::atomic<bool>::is_always_lock_free);
    Request entries_[kQueueCapacity]{}, stop_{};
    std::atomic<uint32_t> write_{0},read_{0};
    std::atomic<bool> stop_pending_{false};
};

}
