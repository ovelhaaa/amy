#include "clock_timing.h"
#include "synth_event.h"

#include <cassert>
#include <cstdint>
#include <atomic>
#include <thread>

int main() {
    using smk::LatestTickMailbox;
    using smk::tickReached;
    static_assert(sizeof(smk::SynthEvent) == 16, "timestamps must retain the bounded event size");

    assert(tickReached(10, 10));
    assert(!tickReached(9, 10));
    assert(tickReached(0, UINT32_MAX));
    assert(!tickReached(UINT32_MAX, 0));

    LatestTickMailbox mailbox;
    uint32_t tick = 0;
    uint32_t skipped = 0;
    uint64_t timestamp = 0;
    assert(!mailbox.consume(tick, skipped, timestamp));

    mailbox.publish(1, 1000);
    assert(mailbox.consume(tick, skipped, timestamp));
    assert(tick == 1 && skipped == 0 && timestamp == 1000);

    // A delayed control task sees the newest position once, never replays a
    // backlog of note triggers.
    mailbox.publish(2, 2000);
    mailbox.publish(3, 3000);
    mailbox.publish(6, 6000);
    assert(mailbox.consume(tick, skipped, timestamp));
    assert(tick == 6 && skipped == 4 && timestamp == 6000);
    assert(!mailbox.consume(tick, skipped, timestamp));

    // Unsigned subtraction and the half-range deadline rule both handle clock
    // rollover without turning a due release into a multi-day wait.
    mailbox.reset(UINT32_MAX - 1);
    mailbox.publish(UINT32_MAX, 10);
    assert(mailbox.consume(tick, skipped, timestamp));
    assert(tick == UINT32_MAX && skipped == 0 && timestamp == 10);
    mailbox.publish(0, 20);
    assert(mailbox.consume(tick, skipped, timestamp));
    assert(tick == 0 && skipped == 0 && timestamp == 20);

    constexpr uint64_t after_32_bit_wrap = (uint64_t{1} << 32) + 1234;
    mailbox.reset();
    mailbox.publish(1, after_32_bit_wrap);
    assert(mailbox.consume(tick, skipped, timestamp));
    assert(timestamp == after_32_bit_wrap);

    mailbox.reset();
    std::atomic<bool> done{false};
    std::thread producer([&] {
        for (uint32_t i = 1; i <= 500000; ++i) mailbox.publish(i, uint64_t{i} * 1000);
        done.store(true);
    });
    do {
        if (mailbox.consume(tick, skipped, timestamp)) assert(timestamp == uint64_t{tick} * 1000);
    } while (!done.load());
    producer.join();
    done = false;
    std::thread resetting([&] {
        for (uint32_t i=0; i<10000; ++i) mailbox.reset();
    });
    std::thread publishing([&] {
        for (uint32_t i=1; i<=100000; ++i) mailbox.publish(i, uint64_t{i} * 1000);
        done = true;
    });
    do {
        if (mailbox.consume(tick, skipped, timestamp)) assert(timestamp == uint64_t{tick} * 1000);
    } while (!done.load());
    publishing.join(); resetting.join();
    return 0;
}
