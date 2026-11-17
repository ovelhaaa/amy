#include "fake_nvs.h"
#include "pattern_nvs.h"
#include "amy_runtime.h"
#include <cassert>
#include <cstdio>

using namespace smk::p4;

static seq::PatternSettings pattern(uint8_t pitch) {
    seq::PatternSettings settings;
    for(auto& step:settings.pattern.steps) step={pitch,98,100,true};
    return settings;
}

static void persistence() {
    NvsPatternStore store;
    assert(store.begin());
    seq::Slot slot;
    slot.state=seq::SlotState::Valid;
    slot.settings=pattern(60);
    std::strcpy(slot.name,"Original");
    for(unsigned i=0;i<seq::kPatternSlots;++i) {
        assert(store.load(i).state==seq::SlotState::Empty);
        assert(store.commit(i,slot));
        assert(store.load(i).settings==slot.settings);
    }
    std::strcpy(slot.name,"Renamed");
    fake_nvs::torn=true;
    assert(!store.commit(0,slot));
    fake_nvs::torn=false;
    NvsPatternStore reboot;
    assert(reboot.begin());
    assert(std::strcmp(reboot.load(0).name,"Original")==0);
    assert(reboot.commit(0,slot));
    assert(std::strcmp(reboot.load(0).name,"Renamed")==0);
    auto& latest=fake_nvs::blobs[{4,"p00b"}];
    latest.back()^=0xff;
    assert(std::strcmp(reboot.load(0).name,"Original")==0);
    assert(reboot.commit(0,{}));
    assert(reboot.load(0).state==seq::SlotState::Empty);
    assert(reboot.load(1).state==seq::SlotState::Valid);

    auto& invalid=fake_nvs::blobs[{4,"p01a"}];
    invalid.back()^=1;
    assert(reboot.load(1).state==seq::SlotState::Corrupted);
    assert(reboot.commit(2,slot));
    auto& future=fake_nvs::blobs[{4,"p02a"}];
    edit::put32(future.data()+4,2);
    edit::put32(future.data()+100,edit::crc(future.data(),100));
    const auto before=fake_nvs::blobs;
    assert(seq::decodeSlot(before.at({4,"p02b"}).data(),seq::kPatternRecordBytes).state==
           seq::SlotState::Valid);
    assert(reboot.load(2).state==seq::SlotState::Incompatible);
    assert(!reboot.commit(2,slot));
    assert(!reboot.commit(2,{}));
    assert(fake_nvs::blobs==before);
    fake_nvs::free_entries=126;
    assert(!reboot.commit(3,slot));
    assert(std::strcmp(reboot.load(3).name,"Original")==0);
    assert(!reboot.commit(seq::kPatternSlots,slot));
    assert(fake_nvs::blobs.count({2,"x00a"})==0);
    puts("PASS all 16 NVS slots, rename/delete, CRC, incompatible version, low space and torn-write recovery");
}

static void timing(unsigned length,tempo::Division division,unsigned swing) {
    seq::Engine engine;
    tempo::Clock clock;
    auto original=pattern(60);
    original.pattern.length=uint8_t(length);
    original.division=division;
    original.swing=uint8_t(swing);
    auto replacement=pattern(72);
    replacement.division=tempo::Division::EighthTriplet;
    replacement.transpose=1;
    bool release=false;
    assert(engine.load(0,original,seq::SwitchMode::Immediate,false,1,clock.position(),release));
    assert(engine.play(clock.position()) && engine.update(clock.position()).note==60);
    clock.advance(256);
    assert(engine.load(1,replacement,seq::SwitchMode::NextBar,false,2,clock.position(),release));
    assert(engine.load(2,replacement,seq::SwitchMode::NextBar,false,3,clock.position(),release));
    constexpr auto boundary=uint64_t(4)*tempo::kPpqn*tempo::kUnitsPerTick;
    bool bpm_changed=false;
    for(unsigned block=0;block<500;++block) {
        const auto pos=clock.position();
        const auto events=engine.update(pos);
        if(pos.phase>=boundary) {
            assert(engine.status().active_slot==2 && engine.status().pending_slot==seq::kNoSlot);
            assert(engine.status().current_step==0 && events.note==73 && events.release);
            assert(engine.status().bpm==177);
            return;
        }
        assert(engine.status().active_slot==0 && engine.status().pending_slot==2);
        assert(!events.velocity || events.note==60);
        if(block==63) bpm_changed=clock.setBpm(177);
        clock.advance(256);
        assert(block<63 || bpm_changed);
    }
    assert(false);
}

static void dirty() {
    seq::Engine engine;
    tempo::Position pos;
    const auto first=pattern(60), second=pattern(65);
    bool release=false;
    assert(engine.load(0,first,seq::SwitchMode::Immediate,false,1,pos,release));
    assert(engine.setSwing(57));
    engine.updateLibraryStatus();
    assert(engine.status().modified);
    assert(!engine.load(1,second,seq::SwitchMode::Immediate,false,2,pos,release));
    assert(engine.status().modified && engine.status().active_slot==0);
    assert(engine.setSwing(50));
    engine.updateLibraryStatus();
    assert(!engine.status().modified);
    assert(engine.play(pos) && engine.update(pos).note==60);
    assert(engine.load(1,second,seq::SwitchMode::NextBar,false,3,pos,release));
    assert(engine.setTranspose(5));
    engine.update(pos);
    assert(engine.status().switch_cancelled && engine.status().active_slot==0);
    assert(engine.load(1,second,seq::SwitchMode::Immediate,true,4,pos,release) && release);
    engine.updateLibraryStatus();
    assert(!engine.status().modified && engine.update(pos).note==65);
    assert(engine.load(0,first,seq::SwitchMode::NextBar,false,5,pos,release));
    assert(engine.stop() && engine.status().pending_slot==seq::kNoSlot);
    assert(engine.status().switch_cancelled);
}

static void owner() {
    amy::Runtime runtime;
    assert(runtime.begin());
    auto& commands=runtime.commands();
    seq::Request request;
    unsigned token=0;
    const auto run=[&] {
        request.token=++token;
        assert(commands.pushSequencer(request));
        runtime.apply(token);
        assert(runtime.render());
        assert(!runtime.snapshot().sequencer.rejected);
    };
    request.action=seq::Action::LoadSlot;
    request.setSettings(pattern(60));
    run();
    request.action=seq::Action::Play;
    run();
    request.action=seq::Action::LoadSlot;
    request.value=int16_t(seq::SwitchMode::NextBar);
    request.index=1;
    request.setSettings(pattern(65));
    run();
    assert(runtime.snapshot().sequencer.pending_slot==1);
    commands.panic();
    runtime.apply(++token);
    assert(runtime.snapshot().sequencer.pending_slot==seq::kNoSlot);
    assert(runtime.snapshot().active_notes==0);
    assert(runtime.snapshot().sequencer.active_slot==0);
    assert(runtime.snapshot().sequencer_pattern.steps[0].note==60);
    const auto patch=runtime.snapshot().patch_id;
    const auto fx=runtime.snapshot().fx.config;
    const auto bpm=runtime.snapshot().sequencer.bpm;
    request.value=int16_t(seq::SwitchMode::Immediate);
    run();
    assert(runtime.snapshot().sequencer.active_slot==1);
    assert(runtime.snapshot().patch_id==patch && runtime.snapshot().sequencer.bpm==bpm);
    assert(runtime.snapshot().fx.config.chorus_mode==fx.chorus_mode);
    assert(runtime.snapshot().fx.config.delay_mix==fx.delay_mix);

    request.action=seq::Action::Play;
    run();
    request.action=seq::Action::LoadSlot;
    request.index=0;
    request.value=int16_t(seq::SwitchMode::NextBar);
    run();
    assert(runtime.snapshot().sequencer.pending_slot==0);
    runtime.usbInput().identifyDevice(0x4353,0x4b4d);
    runtime.usbInput().disconnect();
    runtime.apply(++token);
    assert(runtime.snapshot().sequencer.pending_slot==seq::kNoSlot);
    assert(!runtime.snapshot().active_notes);

    request.action=seq::Action::Play;
    run();
    request.action=seq::Action::LoadSlot;
    run();
    assert(runtime.snapshot().sequencer.pending_slot==0);
    assert(commands.push({amy::CommandType::Patch,129,0,0,commands.nextToken(),token}));
    for(unsigned i=0;i<30 && runtime.snapshot().patch_id!=129;++i) {
        runtime.apply(++token);
        runtime.loadPendingPatch();
        assert(runtime.render());
    }
    assert(runtime.snapshot().patch_id==129);
    assert(runtime.snapshot().sequencer.pending_slot==seq::kNoSlot);
    assert(runtime.snapshot().sequencer.active_slot==1 && !runtime.snapshot().active_notes);

    request.value=int16_t(seq::SwitchMode::Immediate);
    request.token=++token;
    assert(commands.pushSequencer(request));
    commands.panic();
    runtime.apply(++token);
    assert(runtime.snapshot().sequencer.command.rejected);
    assert(runtime.snapshot().sequencer.active_slot==1);
    runtime.end();
}

int main() {
    persistence();
    for(unsigned length:{16u,8u,7u})
        for(unsigned swing:{50u,73u}) timing(length,tempo::Division::Sixteenth,swing);
    timing(12,tempo::Division::EighthTriplet,73);
    dirty();
    owner();
    puts("PASS dirty/revert/discard, global 4/4 boundaries with BPM and swing, replacement and stop/panic");
}
