#include "event_bus.h"
#include "diagnostics.h"

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
        // Queue full. Check if incoming event is critical
        if (event.type == EventType::NoteOff || 
            event.type == EventType::AllNotesOff || 
            event.type == EventType::Panic) {
            // Drop oldest message to make room, but avoid dropping another critical message
            SynthEvent discarded_ev;
            if (xQueueReceive(queue_, &discarded_ev, 0) == pdTRUE) {
                if (discarded_ev.type == EventType::NoteOff || 
                    discarded_ev.type == EventType::AllNotesOff || 
                    discarded_ev.type == EventType::Panic) {
                    // Do not discard an already queued critical event; put it back
                    xQueueSendToFront(queue_, &discarded_ev, 0);
                    overflow_count_.fetch_add(1, std::memory_order_relaxed);
                    Diagnostics::instance().counters().event_queue_overflows.fetch_add(1, std::memory_order_relaxed);
                    return false;
                }
            }
            res = xQueueSendToFront(queue_, &event, 0);
            if (res != pdTRUE) {
                res = xQueueSend(queue_, &event, 0);
            }
            overflow_count_.fetch_add(1, std::memory_order_relaxed);
            Diagnostics::instance().counters().event_queue_overflows.fetch_add(1, std::memory_order_relaxed);
            return (res == pdTRUE);
        }
        overflow_count_.fetch_add(1, std::memory_order_relaxed);
        Diagnostics::instance().counters().event_queue_overflows.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    return true;
}

bool EventBus::sendFromISR(const SynthEvent& event, BaseType_t* pxHigherPriorityTaskWoken) {
    if (!queue_) return false;
    BaseType_t res = xQueueSendFromISR(queue_, &event, pxHigherPriorityTaskWoken);
    if (res != pdTRUE) {
        if (event.type == EventType::NoteOff || 
            event.type == EventType::AllNotesOff || 
            event.type == EventType::Panic) {
            SynthEvent discarded_ev;
            if (xQueueReceiveFromISR(queue_, &discarded_ev, pxHigherPriorityTaskWoken) == pdTRUE) {
                if (discarded_ev.type == EventType::NoteOff || 
                    discarded_ev.type == EventType::AllNotesOff || 
                    discarded_ev.type == EventType::Panic) {
                    xQueueSendToFrontFromISR(queue_, &discarded_ev, pxHigherPriorityTaskWoken);
                    overflow_count_.fetch_add(1, std::memory_order_relaxed);
                    return false;
                }
            }
            res = xQueueSendToFrontFromISR(queue_, &event, pxHigherPriorityTaskWoken);
            overflow_count_.fetch_add(1, std::memory_order_relaxed);
            return (res == pdTRUE);
        }
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
