#include "event_bus.h"

namespace smk {

EventBus::EventBus(size_t capacity) {
    queue_ = xQueueCreate(capacity, sizeof(SynthEvent));
}

EventBus::~EventBus() {
    if (queue_) {
        vQueueDelete(queue_);
    }
}

bool EventBus::send(const SynthEvent& event) {
    if (!queue_) return false;
    
    BaseType_t res = xQueueSend(queue_, &event, 0);
    if (res != pdTRUE) {
        // Queue full. Check if critical event
        if (event.type == EventType::NoteOff || 
            event.type == EventType::AllNotesOff || 
            event.type == EventType::Panic) {
            // Try to force to front for critical events
            res = xQueueSendToFront(queue_, &event, 0);
            if (res != pdTRUE) {
                overflow_count_.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
            return true;
        }
        overflow_count_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    return true;
}

bool EventBus::sendFromISR(const SynthEvent& event, BaseType_t* pxHigherPriorityTaskWoken) {
    if (!queue_) return false;
    BaseType_t res = xQueueSendFromISR(queue_, &event, pxHigherPriorityTaskWoken);
    if (res != pdTRUE) {
        overflow_count_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    return true;
}

bool EventBus::receive(SynthEvent& event, uint32_t timeout_ms) {
    if (!queue_) return false;
    TickType_t ticks = timeout_ms == 0 ? 0 : pdMS_TO_TICKS(timeout_ms);
    return xQueueReceive(queue_, &event, ticks) == pdTRUE;
}

bool EventBus::tryReceive(SynthEvent& event) {
    if (!queue_) return false;
    return xQueueReceive(queue_, &event, 0) == pdTRUE;
}

uint32_t EventBus::overflowCount() const {
    return overflow_count_.load(std::memory_order_relaxed);
}

size_t EventBus::pendingCount() const {
    if (!queue_) return 0;
    return uxQueueMessagesWaiting(queue_);
}

} // namespace smk
