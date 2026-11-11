#pragma once

#include <atomic>
#include <cstdint>

namespace smk::p4::amy {

struct MidiControlEvent {
    uint32_t timestamp_ms = 0;
    uint32_t device_generation = 0;
    uint32_t command_generation = 0;
    uint8_t cable = 0;
    uint8_t channel = 0;
    uint8_t controller = 0;
    uint8_t value = 0;
};

constexpr bool musicalController(uint8_t cc) {
    return cc == 1 || cc == 64 || cc == 120 || cc == 123;
}

constexpr bool observableController(uint8_t cc) {
    return cc < 120 && cc != 64;
}

// Producer: USB client. Consumer: performance UI. Overflow drops newest.
class MidiControlEvents {
public:
    static constexpr unsigned kCapacity = 32;

    void push(const MidiControlEvent& event) {
        const auto w = write_.load(std::memory_order_relaxed);
        if (w - read_.load(std::memory_order_acquire) >= kCapacity) {
            dropped_.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        events_[w % kCapacity] = event;
        write_.store(w + 1, std::memory_order_release);
    }

    bool pop(MidiControlEvent& event) {
        const auto r = read_.load(std::memory_order_relaxed);
        if (r == write_.load(std::memory_order_acquire)) return false;
        event = events_[r % kCapacity];
        read_.store(r + 1, std::memory_order_release);
        return true;
    }

    uint32_t dropped() const { return dropped_.load(std::memory_order_relaxed); }

private:
    static_assert(std::atomic<uint32_t>::is_always_lock_free);
    MidiControlEvent events_[kCapacity]{};
    std::atomic<uint32_t> read_{0}, write_{0}, dropped_{0};
};

}
