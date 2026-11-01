#include "storage_worker.h"
#include "diagnostics.h"
#include "esp_timer.h"
#include <algorithm>
#include <cstdio>
namespace smk {
StorageWorker::~StorageWorker() {
    running_.store(false);
    if (wake_) xSemaphoreGive(wake_);
    while (active_.load()) vTaskDelay(1); // Lifecycle only, never control/audio.
    if (wake_) vSemaphoreDelete(wake_);
}
bool StorageWorker::start(uint8_t core, uint8_t priority, uint32_t stack_bytes) {
    if (wake_ || !stack_bytes) return false;
    wake_ = xSemaphoreCreateBinary();
    if (!wake_) return false;
    running_ = active_ = true;
    if (xTaskCreatePinnedToCore(routine,"storage_worker",stack_bytes,this,priority,&task_,core) != pdPASS) {
        running_ = active_ = false;
        vSemaphoreDelete(wake_); wake_ = nullptr;
        return false;
    }
    return true;
}
StorageWorker::Job* StorageWorker::reserve() {
    State expected = State::Idle;
    if (!wake_ || !state_.compare_exchange_strong(expected,State::Filling,std::memory_order_acquire)) {
        Diagnostics::instance().counters().storage_requests_rejected.fetch_add(1);
        return nullptr;
    }
    job_ = Job{};
    return &job_;
}
void StorageWorker::submit() {
    state_.store(State::Pending,std::memory_order_release);
    xSemaphoreGive(wake_);
}
const StorageWorker::Job* StorageWorker::completion() const {
    return state_.load(std::memory_order_acquire)==State::Done ? &job_ : nullptr;
}
void StorageWorker::releaseCompletion() { state_.store(State::Idle,std::memory_order_release); }
bool StorageWorker::processOne() {
    State expected=State::Pending;
    if (!state_.compare_exchange_strong(expected,State::Working,std::memory_order_acquire)) return false;
    const uint64_t start_us=static_cast<uint64_t>(esp_timer_get_time());
    bool ok=false;
    switch(job_.kind) {
        case Kind::SaveProfile: ok=storage_.saveProfile(job_.name,job_.profile); break;
        case Kind::SaveScene: ok=storage_.saveScene(job_.name,job_.scenes[0]); break;
        case Kind::LoadProfile:
            ok=storage_.loadProfile(job_.name,job_.profile);
            if (!ok) ok=storage_.loadProfile("smk25_custom",job_.profile);
            break;
        case Kind::SaveBundle:
            ok=storage_.savePatch(job_.slot,job_.patch);
            if (ok) {
                ok=storage_.saveProfile("smk25_custom",job_.profile);
                for (unsigned i=0;i<8;++i) {
                    char name[16]; snprintf(name,sizeof(name),"slot_%u",i+1);
                    const bool saved=storage_.saveScene(name,job_.scenes[i]);
                    ok=saved && ok;
                }
            }
            break;
    }
    job_.success=ok;
    auto& d=Diagnostics::instance().counters();
    const uint32_t elapsed=static_cast<uint32_t>(std::min<uint64_t>(static_cast<uint64_t>(esp_timer_get_time())-start_us,UINT32_MAX));
    if (elapsed>d.storage_max_job_us.load()) d.storage_max_job_us=elapsed;
    if (!ok) d.storage_jobs_failed.fetch_add(1);
    state_.store(State::Done,std::memory_order_release);
    return true;
}
void StorageWorker::routine(void* arg) {
    auto& self=*static_cast<StorageWorker*>(arg);
    while(self.running_.load()) {
        if (!self.processOne()) xSemaphoreTake(self.wake_,portMAX_DELAY);
    }
    self.active_=false;
    vTaskDelete(nullptr);
}
} // namespace smk
