#include "performance_midi.h"
#include <cassert>
#include <initializer_list>

using namespace smk::p4::performance;

int main() {
    MidiLearn learn;
    assert(learn.state() == LearnState::Idle);
    learn.arm(10, 5, 3, false);
    assert(!learn.capture({9, 5, 1, 0, 3, 74, 70}, 11, 5));
    assert(!learn.capture({11, 4, 1, 0, 3, 74, 70}, 11, 5));
    assert(!learn.capture({11, 5, 1, 0, 2, 74, 70}, 11, 5));
    for (const uint8_t cc : {1, 64, 120, 123})
        assert(!learn.capture({11, 5, 1, 0, 3, cc, 70}, 11, 5));
    assert(learn.capture({11, 5, 1, 0, 3, 74, 70}, 11, 5));
    assert((learn.candidate() == MidiSource{0, 3, 74}));
    assert(learn.confirm(12, 5));
    learn.resolve(false, 12, 5);
    assert(learn.state() == LearnState::Captured);
    assert(learn.confirm(13, 5));
    learn.resolve(true, 13, 5);
    assert(learn.state() == LearnState::Active);
    learn.arm(20, 5, 16, true);
    assert(learn.capture({21, 5, 1, 0, 8, 1, 70}, 21, 5));
    learn.service(22, 6);
    assert(learn.state() == LearnState::Cancelled);
    learn.arm(23, 6, 16, false);
    learn.cancel();
    assert(learn.state() == LearnState::Cancelled);
    learn.arm(0xfffffff0, 6, 16, false);
    learn.service(0xfffffff0 + MidiLearn::kTimeoutMs, 6);
    assert(learn.state() == LearnState::Timeout);

    Pickup knob;
    uint8_t output = 0;
    assert(!knob.update(81, 84, output));
    assert(knob.state() == PickupState::ApproachingFromBelow);
    for (const uint8_t physical : {81, 82, 81, 83, 82})
        assert(!knob.update(physical, 84, output));
    assert(knob.update(88, 84, output) && output == 84);
    assert(!knob.update(88, 84, output));
    assert(knob.update(99, 88, output) && output == 99);
    knob.reset();
    assert(!knob.update(100, 84, output));
    assert(knob.state() == PickupState::ApproachingFromAbove);
    assert(!knob.update(90, 84, output));
    assert(knob.update(87, 84, output) && output == 84);
    knob.reset();
    assert(knob.update(84, 84, output) && output == 84);
    knob.reset();
    assert(!knob.update(100, 30, output));
    assert(!knob.update(128, 30, output));
    assert(knob.update(27, 30, output) && output == 30);
}
