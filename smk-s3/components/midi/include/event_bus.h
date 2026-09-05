#pragma once
#include <cstdint>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "synth_event.h"

namespace smk {

class EventBus {
public:
    explicit EventBus(size_t capacity = 256);
    ~EventBus();

    bool ready() const { return entries_ != nullptr && wake_ != nullptr; }
    
    // Send event (non-blocking, from task context)
    bool send(const SynthEvent& event);
    
    // Send event from ISR context
    bool sendFromISR(const SynthEvent& event, BaseType_t* pxHigherPriorityTaskWoken = nullptr);
    
    // Receive event (blocking with timeout)
    bool receive(SynthEvent& event, uint32_t timeout_ms = 0);
    
    // Receive event (non-blocking)
    bool tryReceive(SynthEvent& event);

    // Single consumer: call only after clearing engine and producer note state.
    // Musical input is suppressed until this acknowledgement.
    void acknowledgePanic();
    
    // Diagnostics
    uint32_t overflowCount() const;
    size_t pendingCount() const;
    
    // Non-copyable
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    
private:
    struct Entry {
        SynthEvent event;
        uint32_t generation;
    };
    bool enqueueLocked(const SynthEvent& event);
    void panicLocked(const SynthEvent& event);
    void countDropLocked(bool overflow);

    Entry* entries_ = nullptr;
    SemaphoreHandle_t wake_ = nullptr;
    size_t capacity_ = 0;
    size_t release_reserve_ = 0;
    size_t head_ = 0;
    size_t count_ = 0;
    uint32_t generation_ = 0;
    uint32_t delivered_generation_ = 0;
    bool panic_pending_ = false;
    bool panic_delivered_ = false;
    SynthEvent panic_event_{};
    mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
    std::atomic<uint32_t> overflow_count_{0};
};

} // namespace smk
