#include "local_performance_backend.h"
#include "touch_note.h"
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace smk::p4;

static void readModel() {
    amy::Commands commands;
    LocalAmyPerformanceBackend backend(commands);
    assert(!backend.connected() && !backend.busy() && backend.local());
    assert(!backend.takeNotice() && !backend.midiError());
    PatchInfo info;
    assert(backend.patchCount()==amy::kPatchCount && backend.patchAt(0)==128 && backend.patchAt(1)==135);
    assert(backend.patchAt(amy::kPatchCount)==UINT16_MAX && !backend.patchInfo(amy::kPatchCount,info,0));
    assert(backend.patchInfo(1,info,0) && info.id==135 && !info.available);
    assert(!backend.setPatch(128,0) && !backend.setMaster(10,0));
    amy::Snapshot owner;
    owner.ready=true; owner.patch_id=135; owner.master=71; owner.active_notes=3;
    owner.polyphony_rejected=9; owner.oom_count=2;
    const auto revision=backend.stateRevision();
    backend.publish(owner,false,321,4);
    assert(backend.connected() && backend.stateRevision()==revision+1);
    assert(backend.patchInfo(1,info,0) && info.available);
    const auto& state=backend.state();
    assert(state.patch_id==135 && state.engine_patch==135 && state.master==71 && state.active_notes==3);
    assert(std::strcmp(state.patch_name,amy::kPatches[1].name)==0);
    assert(state.sample_rate_hz==48000 && state.block_frames==256);
    assert(state.flags==0 && state.max_render_us==321 && state.deadline_misses==4);
    // These DTO members do not provide local heap, underrun or OOM telemetry.
    assert(!state.free_internal && !state.free_psram && !state.pcm_starvations && !state.event_overflows);
    assert(!state.bpm_centi && !state.commands_dropped);
    for(const auto& macro:state.macros) assert(!macro.supported && !macro.value && !macro.name[0]);
    assert(!backend.setMacro(0,100,0) && !commands.depth());
    backend.publishMidi(true,false,17);
    assert(backend.stateRevision()==revision+1); // No revision increment for MIDI-only publication.
    assert(state.midi_events==17 && state.flags==smk::link::kUsbConnected);
    backend.publish(owner,true,322,5);
    assert(state.flags==(smk::link::kAudioLive|smk::link::kUsbConnected));
    backend.publishMidi(false,true,18);
    assert(backend.midiError() && backend.connected() && state.flags==smk::link::kAudioLive);
    owner.ready=false; backend.publish(owner,false,322,5);
    assert(!backend.connected() && std::strcmp(backend.status(),"AMY INIT/ERROR")==0);
}

static void asynchronousCommandsAndLease() {
    amy::Runtime runtime; assert(runtime.begin());
    LocalAmyPerformanceBackend backend(runtime.commands());
    const auto publish=[&] { backend.publish(runtime.snapshot(),true,0,0); };
    publish();
    assert(backend.setMaster(90,0));
    assert(backend.state().master==64 && runtime.snapshot().master==64);
    runtime.apply(0); assert(runtime.snapshot().master==90 && backend.state().master==64);
    publish(); assert(backend.state().master==90);
    assert(backend.noteOn(60,100,0) && backend.noteOn(60,20,0));
    assert(runtime.commands().depth()==1 && !runtime.snapshot().active_notes);
    runtime.apply(0); assert(runtime.snapshot().active_notes==1);
    backend.heldNote(60); runtime.apply(amy::kLeaseMs-1);
    assert(runtime.snapshot().active_notes==1);
    backend.heldNote(60); runtime.apply(amy::kLeaseMs);
    assert(!runtime.snapshot().active_notes && runtime.snapshot().expired_notes==1);
    publish();
    // There is no per-note lease acknowledgement to clear the backend token.
    assert(backend.noteOn(60,100,amy::kLeaseMs+1) && !runtime.commands().depth());
    runtime.apply(amy::kLeaseMs+1); assert(!runtime.snapshot().active_notes);
    backend.noteOff(60);
    assert(backend.noteOn(60,100,amy::kLeaseMs+2));
    runtime.apply(amy::kLeaseMs+2); assert(runtime.snapshot().active_notes==1);
    assert(backend.noteOn(60,0,amy::kLeaseMs+3)); // velocity zero routes to the release latch
    runtime.apply(amy::kLeaseMs+3); assert(!runtime.snapshot().active_notes);
    // Two same-pitch callers are not two owned notes: one release ends the gate.
    assert(backend.noteOn(62,100,11000) && backend.noteOn(62,100,11000));
    runtime.apply(11000); backend.noteOff(62); runtime.apply(11001);
    assert(!runtime.snapshot().active_notes);
    assert(backend.setPatch(135,11002) && backend.state().patch_id==128);
    runtime.apply(11002); assert(runtime.snapshot().patch_id==135 && backend.state().patch_id==128);
    publish(); assert(backend.state().patch_id==135);
}

static void queueAndGestureCancellation() {
    amy::Runtime runtime; assert(runtime.begin());
    LocalAmyPerformanceBackend backend(runtime.commands());
    const auto publish=[&] { backend.publish(runtime.snapshot(),true,0,0); };
    publish(); TouchNote touch;
    for(unsigned i=0;i<amy::kCommandCapacity;++i) assert(backend.setMaster(70,0));
    touch.press(60,0); touch.service(backend,0,true); // queue full, On not accepted
    touch.releaseAll(); runtime.apply(0); touch.service(backend,1,true); runtime.apply(1);
    assert(!runtime.snapshot().active_notes); // canceled unsent gate cannot replay
    publish(); assert(backend.state().commands_dropped==1);
    touch.press(60,2); touch.service(backend,2,true); runtime.apply(2);
    assert(runtime.snapshot().active_notes==1);
    touch.releaseAll(); // navigation/focus loss must still service the pending Off
    touch.service(backend,3,true); runtime.apply(3); assert(!runtime.snapshot().active_notes);
    touch.press(62,4); touch.service(backend,4,true); runtime.apply(4);
    touch.service(backend,5,false); runtime.apply(5); assert(!runtime.snapshot().active_notes);
    assert(backend.noteOn(64,100,6)); runtime.apply(6);
    for(unsigned i=0;i<amy::kCommandCapacity;++i) assert(backend.setMaster(10,6));
    const auto generation=runtime.commands().generation();
    assert(!backend.setPatch(135,7)); // queue rejection still has a Panic side effect
    assert(runtime.commands().generation()!=generation);
    runtime.apply(7); publish();
    assert(!runtime.snapshot().active_notes && runtime.snapshot().patch_id==128 && runtime.snapshot().master==70);
    assert(backend.state().commands_dropped==2);
    for(uint8_t pitch=60;pitch<60+amy::kVoices+1;++pitch) assert(backend.noteOn(pitch,100,8));
    runtime.apply(8); publish();
    assert(runtime.snapshot().active_notes==amy::kVoices && runtime.snapshot().polyphony_rejected==1);
    assert(backend.state().commands_dropped==2); // accepted On can still be rejected by the owner
    backend.panic(); runtime.apply(9); assert(!runtime.snapshot().active_notes);
}

int main() {
    readModel(); asynchronousCommandsAndLease(); queueAndGestureCancellation();
    std::puts("PASS: local backend contract: DTO sources/unavailable fields, async application, MIDI revision, fixed lease, pitch ownership, queue rejection and gesture cancellation");
}
