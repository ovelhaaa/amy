#include "event_bus.h"
#include "diagnostics.h"
#include <algorithm>
#include <new>
#include <cstdlib>
#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#endif

namespace smk {

EventBus::EventBus(size_t capacity) : capacity_(capacity) {
    if (capacity_ == 0 || capacity_ > SIZE_MAX / sizeof(Entry)) return;
#ifdef ESP_PLATFORM
    // ISR-accessed queue storage must not spill into PSRAM.
    entries_ = static_cast<Entry*>(heap_caps_malloc(capacity_ * sizeof(Entry), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
#else
    entries_ = static_cast<Entry*>(std::malloc(capacity_ * sizeof(Entry)));
#endif
    if (!entries_) return;
    for (size_t i = 0; i < capacity_; ++i) new (&entries_[i]) Entry{};
    (void)Diagnostics::instance(); // Initialize before any ISR can update counters.
    wake_ = xSemaphoreCreateBinary();
    release_reserve_ = std::min(size_t(32), std::max(size_t(1), capacity_ / 4));
}

EventBus::~EventBus() {
    if (wake_) vSemaphoreDelete(wake_);
    std::free(entries_);
}

void EventBus::countDropLocked(bool overflow) {
    auto& counters = Diagnostics::instance().counters();
    counters.events_dropped.fetch_add(1, std::memory_order_relaxed);
    if (overflow) {
        overflow_count_.fetch_add(1, std::memory_order_relaxed);
        counters.event_queue_overflows.fetch_add(1, std::memory_order_relaxed);
    }
}

void EventBus::panicLocked(const SynthEvent& event) {
    ++generation_;
    panic_event_ = event;
    panic_event_.type = EventType::Panic;
    panic_pending_ = true;
    panic_delivered_ = false;
}

bool EventBus::enqueueLocked(const SynthEvent& event) {
    if (event.type == EventType::Panic) {
        panicLocked(event);
        return true;
    }
    if (event.type == EventType::UsbDisconnect) panicLocked(event);

    if (panic_pending_ && !isUsbLifecycleEvent(event)) {
        countDropLocked(false);
        return false;
    }

    const bool critical = isReleaseEvent(event) || isUsbLifecycleEvent(event);
    const size_t limit = critical ? capacity_ : capacity_ - release_reserve_;
    if (count_ >= limit) {
        countDropLocked(true);
        // A lost release becomes an explicit fail-safe reset. Never move a
        // Note Off ahead of its queued Note On, or evict another release.
        if (isReleaseEvent(event)) panicLocked(event);
        return false;
    }
    entries_[(head_ + count_) % capacity_] = {event, generation_};
    ++count_;
    return true;
}

bool EventBus::send(const SynthEvent& event) {
    if (!ready()) return false;
    portENTER_CRITICAL(&mux_);
    const bool accepted = enqueueLocked(event);
    portEXIT_CRITICAL(&mux_);
    xSemaphoreGive(wake_);
    return accepted;
}

bool EventBus::sendFromISR(const SynthEvent& event, BaseType_t* woken) {
    if (!ready()) return false;
    portENTER_CRITICAL_ISR(&mux_);
    const bool accepted = enqueueLocked(event);
    portEXIT_CRITICAL_ISR(&mux_);
    xSemaphoreGiveFromISR(wake_, woken);
    return accepted;
}

bool EventBus::tryReceive(SynthEvent& event) {
    if (!ready()) return false;
    // Each critical section copies at most one entry. USB lifecycle events
    // survive resets; stale musical events cannot restart a silenced voice.
    for (size_t i = 0; i <= capacity_; ++i) {
        portENTER_CRITICAL(&mux_);
        if (panic_pending_ && !panic_delivered_) {
            event = panic_event_;
            panic_delivered_ = true;
            delivered_generation_ = generation_;
            portEXIT_CRITICAL(&mux_);
            return true;
        }
        if (count_ == 0) {
            portEXIT_CRITICAL(&mux_);
            return false;
        }
        const Entry entry = entries_[head_];
        head_ = (head_ + 1) % capacity_;
        --count_;
        const bool valid = isUsbLifecycleEvent(entry.event) ||
                           (!panic_pending_ && entry.generation == generation_);
        if (!valid) countDropLocked(false);
        portEXIT_CRITICAL(&mux_);
        if (valid) {
            event = entry.event;
            return true;
        }
    }
    return false;
}

bool EventBus::receive(SynthEvent& event, uint32_t timeout_ms) {
    if (tryReceive(event)) return true;
    if (!ready() || timeout_ms == 0) return false;
    // Semaphore is only a wake hint; entries and Panic live under mux_.
    xSemaphoreTake(wake_, pdMS_TO_TICKS(timeout_ms));
    return tryReceive(event);
}

void EventBus::acknowledgePanic() {
    portENTER_CRITICAL(&mux_);
    if (panic_delivered_ && delivered_generation_ == generation_) {
        panic_pending_ = false;
        panic_delivered_ = false;
    }
    portEXIT_CRITICAL(&mux_);
}

uint32_t EventBus::overflowCount() const {
    return overflow_count_.load(std::memory_order_relaxed);
}

size_t EventBus::pendingCount() const {
    portENTER_CRITICAL(&mux_);
    const size_t result = count_ + (panic_pending_ && !panic_delivered_ ? 1 : 0);
    portEXIT_CRITICAL(&mux_);
    return result;
}

} // namespace smk
