#include "storage_worker.h"
#include "event_bus.h"
#include "diagnostics.h"
#include <cassert>
#include <filesystem>
#include <thread>
#include <chrono>
#include <cstring>
namespace smk {
Diagnostics& Diagnostics::instance() { static Diagnostics d; return d; }
DiagnosticCounters& Diagnostics::counters() { return counters_; }
struct StorageWorkerTestAccess {
    static void initialize(StorageWorker& w) { w.wake_=xSemaphoreCreateBinary(); }
    static bool working(StorageWorker& w) { return w.state_.load()==StorageWorker::State::Working; }
    static std::recursive_mutex& io(StorageManager& s) { return s.io_mutex_; }
};
}
using namespace smk;
int main() {
    std::filesystem::create_directories("build/smk_storage_worker/data");
    StorageManager store; assert(store.begin("build/smk_storage_worker/data"));
    StorageWorker w(store); StorageWorkerTestAccess::initialize(w);
    auto* j=w.reserve(); assert(j && !w.reserve());
    j->kind=StorageWorker::Kind::SaveBundle; j->slot=17;
    j->patch.id=135; strcpy(j->patch.name,"STABLE SNAPSHOT");
    strcpy(j->profile.name,"PROFILE SNAPSHOT");
    w.submit(); assert(!w.reserve());
    // Simulate arbitrarily slow I/O behind the storage mutex. Routing must
    // continue without waiting on it; no claimed physical duration measurement.
    std::unique_lock<std::recursive_mutex> paused(StorageWorkerTestAccess::io(store));
    std::thread worker([&]{ assert(w.processOne()); });
    auto timeout=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(!StorageWorkerTestAccess::working(w)) {
        assert(std::chrono::steady_clock::now()<timeout); std::this_thread::yield();
    }
    assert(!w.reserve() && !w.completion());
    EventBus bus(16);
    SynthEvent e{EventType::NoteOn,EventSource::UsbMidi,0,60,100,1};
    for (int i=0;i<12;++i) bus.send(e);
    e.type=EventType::NoteOff; assert(bus.send(e));
    e.type=EventType::Panic; assert(bus.send(e));
    assert(bus.receive(e,0) && e.type==EventType::Panic);
    bus.acknowledgePanic(); assert(!bus.receive(e,0));
    paused.unlock(); worker.join();
    const auto* result=w.completion(); assert(result && result->success && !w.reserve());
    SynthPatch loaded{}; assert(store.loadPatch(17,loaded));
    assert(loaded.id==135 && !strcmp(loaded.name,"STABLE SNAPSHOT"));
    assert(loaded.crc32==calculatePatchCrc32(loaded));
    w.releaseCompletion();
    j=w.reserve(); assert(j); j->kind=StorageWorker::Kind::LoadProfile;
    j->token=42; strcpy(j->name,"missing_device"); w.submit();
    assert(w.processOne()); result=w.completion();
    assert(result && result->success && result->token==42);
    assert(!strcmp(result->profile.name,"PROFILE SNAPSHOT"));
    w.releaseCompletion();
    // Real write failure survives to completion; rejected request never wins.
    store.setBasePath("build/smk_storage_worker/no_such_directory");
    j=w.reserve(); assert(j); j->kind=StorageWorker::Kind::SaveProfile;
    strcpy(j->name,"failure"); w.submit(); assert(w.processOne());
    assert(w.completion() && !w.completion()->success);
    assert(Diagnostics::instance().counters().storage_jobs_failed==1);
    w.releaseCompletion();
    mock_task::create_result=pdFALSE;
    StorageWorker failed(store); assert(!failed.start(0,2,8192) && !failed.reserve());
    mock_task::create_result=pdPASS;
    printf("PASS: storage snapshot, bounded ownership, failure/fallback and MIDI/Panic during blocked I/O; job=%zu bytes\n",sizeof(StorageWorker::Job));
}
