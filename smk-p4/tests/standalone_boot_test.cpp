#include "../main/standalone_boot.h"
#include <cassert>
#include <cstdio>
using namespace smk::p4::boot;
namespace {
struct Snapshot {
    struct { bool ready=false; } engine;
    bool running=false, owner_initialized=false;
    int error=0;
};
Snapshot ready() { return {{true},true,true,0}; }
}
int main() {
    Snapshot initial=ready();
    unsigned peeks=0, delays=0;
    auto missing=[&](Snapshot&) { ++peeks; return false; };
    auto delay=[&] { ++delays; };
    // Missing mailbox/task must not invoke their APIs, nor retain old success.
    assert(!awaitSnapshot(false,true,initial,3,missing,delay));
    assert(!peeks && !delays && !initial.running && !initial.engine.ready);
    assert(usbStart(true,false,true,false,initial)==UsbStart::MailboxUnavailable);
    assert(!awaitSnapshot(true,false,initial,3,missing,delay));
    assert(!peeks && !delays);
    assert(usbStart(true,true,false,false,initial)==UsbStart::OwnerNotStarted);
    assert(!awaitSnapshot(true,true,initial,3,missing,delay));
    assert(peeks==3 && delays==3);
    // Even a plausible/stale payload cannot authorize USB on query timeout.
    initial=ready();
    assert(usbStart(true,true,true,false,initial)==UsbStart::SnapshotTimeout);
    assert(usbStart(false,true,true,true,initial)==UsbStart::Disabled);
    assert(usbStart(false,false,false,false,initial)==UsbStart::Disabled);
    peeks=delays=0;
    const bool received=awaitSnapshot(true,true,initial,3,[&](Snapshot& value) {
        if(++peeks<3) return false;
        value=ready(); return true;
    },delay);
    assert(received && peeks==3 && delays==2); // Last allowed peek counts.
    assert(usbStart(true,true,true,received,initial)==UsbStart::Ready);
    initial.owner_initialized=false;
    assert(usbStart(true,true,true,true,initial)==UsbStart::OwnerNotInitialized);
    initial=ready(); initial.error=-1;
    assert(usbStart(true,true,true,true,initial)==UsbStart::OwnerFailed);
    initial=ready(); initial.running=false;
    assert(usbStart(true,true,true,true,initial)==UsbStart::OwnerStopped);
    initial=ready(); initial.engine.ready=false;
    assert(usbStart(true,true,true,true,initial)==UsbStart::EngineNotReady);
    // A received failure is not a timeout and does not consume the poll budget.
    peeks=delays=0;
    assert(awaitSnapshot(true,true,initial,3,[&](Snapshot& value) {
        ++peeks; value=Snapshot{}; value.owner_initialized=true; value.error=-1; return true;
    },delay));
    assert(peeks==1 && !delays);
    assert(usbStart(true,true,true,true,initial)==UsbStart::OwnerFailed);
    std::puts("PASS: boot snapshot success/timeout/task/mailbox/failure/readiness/qualification gates and bounded polling");
}
