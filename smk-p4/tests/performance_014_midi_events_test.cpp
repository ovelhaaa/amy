#include "usb_input.h"
#include <cassert>

using namespace smk::p4::amy;

int main() {
    UsbInput input;
    input.enableControls();
    const uint8_t cc[]{0x0b, 0xb3, 74, 100};
    const uint8_t cc_other_cable[]{0x1b, 0xb3, 74, 100};
    const uint8_t bad_cc[]{0x0b, 0xb3, 74, 128};
    const uint8_t off[]{0x08, 0x83, 60, 0};
    const uint8_t on[]{0x09, 0x93, 60, 100};
    const uint8_t bend[]{0x0e, 0xe3, 2, 64};
    const uint8_t clock[]{0x0f, 0xf8, 0, 0};
    for (unsigned i = 0; i < MidiControlEvents::kCapacity + 1; ++i)
        assert(input.transfer(cc, sizeof(cc), 100 + i));
    assert(input.controlDrops() == 1);
    assert(input.commands.depth() == 0);
    assert(input.transfer(on, sizeof(on), 200));
    assert(input.transfer(off, sizeof(off), 201));
    assert(input.transfer(bend, sizeof(bend), 202));
    assert(input.transfer(clock, sizeof(clock), 203));
    Command command;
    assert(input.commands.pop(command) && command.type == CommandType::NoteOn);
    assert(input.commands.pop(command) && command.type == CommandType::NoteOff);
    assert(input.commands.pop(command) && command.type == CommandType::Bend && command.id == 8194);
    assert(!input.commands.pop(command) && input.dropped == 0);
    MidiControlEvent event;
    for (unsigned i = 0; i < MidiControlEvents::kCapacity; ++i) {
        assert(input.popControl(event) && input.currentControl(event));
        assert(event.timestamp_ms == 100 + i && event.channel == 3 && event.cable == 0);
        assert(event.controller == 74 && event.value == 100);
    }
    assert(!input.popControl(event));
    for (const uint8_t controller : {1, 64, 120, 123}) {
        const uint8_t reserved[]{0x0b, 0xb3, controller, 0};
        assert(input.transfer(reserved, sizeof(reserved), 204));
        assert(input.commands.pop(command) && command.type == CommandType::Control && command.id == controller);
    }
    assert(input.popControl(event) && event.controller == 1);
    assert(!input.popControl(event));
    assert(input.transfer(cc_other_cable, sizeof(cc_other_cable), 205));
    assert(!input.popControl(event));
    assert(input.transfer(cc, sizeof(cc), 206));
    input.disconnect();
    assert(input.popControl(event) && !input.currentControl(event));
    assert(input.transfer(cc, sizeof(cc), 207));
    assert(input.popControl(event) && input.currentControl(event));
    assert(!input.transfer(bad_cc, sizeof(bad_cc), 208));
    assert(!input.currentControl(event));
    assert(!input.popControl(event));
}
