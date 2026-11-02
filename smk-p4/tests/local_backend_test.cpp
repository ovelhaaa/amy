#include "local_performance_backend.h"
#include "touch_note.h"
#include <cassert>
#include <cstdio>
#include <thread>
#include <atomic>
using namespace smk::p4;
int main() {
    amy::Runtime runtime;
    LocalAmyPerformanceBackend backend(runtime.commands());
    assert(!backend.connected() && !backend.noteOn(60,100,0));
    assert(runtime.begin());
    const auto publish=[&] { backend.publish(runtime.snapshot(),true,100,0); };
    publish(); assert(backend.connected() && backend.patchCount()==2);
    assert(!backend.setPatch(999,0) && !backend.setMaster(128,0));
    assert(!backend.noteOn(128,100,0) && !backend.noteOn(60,128,0));
    assert(!backend.setMacro(0,100,0));
    assert(backend.noteOn(60,100,0) && backend.noteOn(60,100,0));
    assert(runtime.commands().depth()==1);
    runtime.apply(0); assert(runtime.snapshot().active_notes==1);
    for(unsigned i=0;i<amy::kCommandCapacity;++i) assert(backend.setMaster(70,0));
    assert(!backend.setMaster(10,0) && runtime.commands().overflows()==1);
    backend.noteOff(60); runtime.apply(1); assert(!runtime.snapshot().active_notes);
    publish(); assert(backend.state().master==70);
    assert(backend.noteOn(60,100,2)); runtime.apply(2);
    for(unsigned i=0;i<amy::kCommandCapacity;++i) assert(backend.setMaster(10,2));
    backend.panic(); runtime.apply(3);
    assert(!runtime.snapshot().active_notes && runtime.snapshot().master==70);
    assert(backend.noteOn(60,100,4));
    // A stale, pre-Panic snapshot must never erase a new release token.
    amy::Snapshot stale; stale.ready=true;
    backend.publish(stale,true,0,0);
    backend.noteOff(60); runtime.apply(5); assert(!runtime.snapshot().active_notes);
    assert(backend.noteOn(62,100,6)); runtime.apply(6);
    assert(backend.setPatch(135,7)); runtime.apply(7); publish();
    assert(runtime.snapshot().patch_id==135 && !runtime.snapshot().active_notes);
    assert(backend.noteOn(62,100,8)); runtime.apply(8);
    // Old generation's Off cannot terminate the new incarnation.
    runtime.commands().release(62,1); runtime.apply(9); assert(runtime.snapshot().active_notes==1);
    assert(backend.noteOn(62,0,10)); runtime.apply(10); assert(!runtime.snapshot().active_notes);
    TouchNote touch;
    touch.press(64,20); touch.service(backend,20,true); runtime.apply(20); assert(runtime.snapshot().active_notes==1);
    touch.release(64); touch.service(backend,21,true); runtime.apply(21); assert(!runtime.snapshot().active_notes);
    touch.press(64,30); touch.service(backend,30,true); runtime.apply(30);
    runtime.apply(10030); assert(!runtime.snapshot().active_notes && runtime.snapshot().expired_notes==1);
    touch.disconnect(); backend.panic(); runtime.apply(10031);
    assert(runtime.commands().push({amy::CommandType::NoteOn,999,100,0,999,10031}));
    runtime.apply(10032); assert(runtime.snapshot().invalid_commands);
    runtime.end(); publish(); assert(!backend.connected() && !backend.noteOn(60,100,0));
    assert(runtime.begin()); publish(); assert(!backend.state().active_notes);
    std::atomic<bool> finished{false};
    std::thread owner([&] {
        while(!finished.load(std::memory_order_acquire) || runtime.commands().depth()) {
            runtime.apply(0); assert(runtime.render());
        }
        runtime.apply(0);
    });
    for(unsigned i=0;i<10000;++i) {
        backend.noteOn(60,100,0); backend.setMaster(i%128,0); backend.noteOff(60);
        if(i%17==0) backend.panic();
    }
    backend.panic(); finished.store(true,std::memory_order_release); owner.join();
    assert(!runtime.snapshot().active_notes);
    std::puts("PASS: applied snapshots, invalid/duplicate commands, saturation, priority Off/Panic, stale snapshots/generations, patch cancellation, lease and restart");
}
