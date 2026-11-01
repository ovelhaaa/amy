#pragma once
#include "storage_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <atomic>

namespace smk {
// Capacity one: ownership moves by release/acquire, never by shared references.
// Exactly one control producer and completion consumer; worker owns I/O only.
class StorageWorker {
public:
    enum class Kind : uint8_t { SaveProfile, SaveBundle, SaveScene, LoadProfile };
    struct Job {
        Kind kind = Kind::SaveProfile;
        uint32_t token = 0;
        uint8_t slot = 0;
        char name[32]{};
        SynthPatch patch{};
        ControllerProfile profile{};
        Scene scenes[8]{};
        bool success = false;
    };
    explicit StorageWorker(StorageManager& storage) : storage_(storage) {}
    ~StorageWorker();
    bool start(uint8_t core, uint8_t priority, uint32_t stack_bytes);
    Job* reserve(); // Nonblocking; nullptr means busy/unavailable.
    void submit(); // Caller has filled the reserved stable snapshot.
    const Job* completion() const;
    void releaseCompletion();
    bool processOne(); // Worker only; deterministic host driver.
private:
    friend struct StorageWorkerTestAccess;
    enum class State : uint8_t { Idle, Filling, Pending, Working, Done };
    static void routine(void* arg);
    StorageManager& storage_; // Application lifetime exceeds worker lifetime.
    Job job_{};
    std::atomic<State> state_{State::Idle};
    std::atomic<bool> running_{false}, active_{false};
    SemaphoreHandle_t wake_ = nullptr;
    TaskHandle_t task_ = nullptr;
};
} // namespace smk
