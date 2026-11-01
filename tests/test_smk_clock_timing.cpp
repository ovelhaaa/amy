#include "clock_timing.h"

#include <cassert>
#include <cstdint>

int main() {
    using smk::LatestTickMailbox;
    using smk::tickReached;

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

    return 0;
}
