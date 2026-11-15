#include "usb_input.h"
#include <cassert>

using namespace smk::p4::amy;

int main() {
    UsbInput usb;
    usb.enableControls();
    usb.setLegacyProfile(true);
    uint32_t now=0;
    const auto send=[&](uint8_t status,uint8_t number,uint8_t value) {
        const uint8_t packet[]{uint8_t(status>>4),status,number,value};
        return usb.transfer(packet,sizeof(packet),++now);
    };
    Command musical;
    MidiControlEvent control;
    usb.identifyDevice(0x4353,0x1234);
    assert(send(0x99,36,127) && usb.commands.pop(musical) && musical.type==CommandType::NoteOn);
    assert(send(0x89,36,0) && usb.commands.pop(musical) && musical.type==CommandType::NoteOff);
    assert(!usb.popControl(control));
    usb.identifyDevice(0x4353,0x4b4d);
    for(uint8_t i=0;i<16;++i){
        assert(send(0xb0,uint8_t(21+i),uint8_t(63+i)));
        assert(usb.popControl(control) && control.kind==MidiControlKind::CC &&
               control.controller==21+i && usb.currentControl(control));
        assert(send(0x99,uint8_t(36+i),100));
        assert(send(i%2 ? 0x89 : 0x99,uint8_t(36+i),0));
        assert(usb.popControl(control) && control.kind==MidiControlKind::PadOn && control.value==100);
        assert(usb.popControl(control) && control.kind==MidiControlKind::PadOff && control.controller==36+i);
    }
    assert(!usb.commands.pop(musical));
    assert(send(0x90,36,127) && usb.commands.pop(musical) && musical.type==CommandType::NoteOn);
    assert(send(0x80,36,0) && usb.commands.pop(musical) && musical.type==CommandType::NoteOff);
    assert(!send(0xb0,21,128) && !usb.popControl(control));
    assert(send(0xb1,115,127) && usb.popControl(control) && control.kind==MidiControlKind::CC);
    const auto start=usb.commands.generation();
    for(uint8_t cc : {114,115,117}){
        for(uint8_t value : {0,127,127,0}){
            assert(send(0xb0,cc,value) && usb.popControl(control));
            assert(control.kind==MidiControlKind::Transport && control.value==value);
        }
    }
    assert(usb.commands.generation()==start+1);
    assert(send(0x99,51,127));
    for(unsigned i=1;i<MidiControlEvents::kCapacity;++i) assert(send(0xb0,21,127));
    assert(send(0x89,51,0) && usb.controlDrops()==1);
    assert(send(0x80,60,0) && usb.commands.pop(musical) && musical.type==CommandType::NoteOff);
    usb.disconnect();
    while(usb.popControl(control)) assert(!usb.currentControl(control));

    usb.identifyDevice(0x4353,0x4b4d);
    usb.setLegacyProfile(false);
    assert(send(0x99,51,100) && usb.commands.pop(musical));
    usb.setLegacyProfile(true);
    assert(send(0x89,51,0) && usb.commands.pop(musical) && musical.type==CommandType::NoteOff);
    assert(send(0x99,51,100) && usb.popControl(control) && control.kind==MidiControlKind::PadOn);
    usb.setLegacyProfile(false);
    assert(send(0x89,51,0) && usb.popControl(control) && control.kind==MidiControlKind::PadOff);
    assert(!usb.commands.pop(musical));
}
