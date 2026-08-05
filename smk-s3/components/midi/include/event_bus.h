#pragma once
#include <cstdint>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "synth_event.h"

namespace smk {

class EventBus {
public:
    explicit EventBus(size_t capacity = 256);
    ~EventBus();
    
    // Send event (non-blocking, from task context)
    bool send(const SynthEvent& event);
    
    // Send event from ISR context
    bool sendFromISR(const SynthEvent& event, BaseType_t* pxHigherPriorityTaskWoken = nullptr);
    
    // Receive event (blocking with timeout)
    bool receive(SynthEvent& event, uint32_t timeout_ms = 0);
    
    // Receive event (non-blocking)
    bool tryReceive(SynthEvent& event);
    
    // Diagnostics
    uint32_t overflowCount() const;
    size_t pendingCount() const;
    
    // Non-copyable
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    
private:
    QueueHandle_t queue_;
    std::atomic<uint32_t> overflow_count_{0};
};

} // namespace smk
