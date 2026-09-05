// Real AMY, real command/PCM queues. Only task scheduling is mocked.
#include "amy_adapter.h"
#include "synth_config.h"
#include "diagnostics.h"
#include "freertos/task.h"
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>

namespace smk {
Diagnostics& Diagnostics::instance() { static Diagnostics instance; return instance; }
DiagnosticCounters& Diagnostics::counters() { return counters_; }
struct AmyAdapterTestAccess {
    static bool service(AmyAdapter& adapter) { return adapter.serviceBlock(); }
};
}
thread_local bool owner = false;
std::atomic<bool> pause_render{false}, render_entered{false}, resume_render{false};
struct OwnerScope {
    OwnerScope() { assert(!owner); owner = true; }
    ~OwnerScope() { owner = false; }
};
extern "C" {
#include "amy.h"
void delay_ms(uint32_t) {}
void amy_update_tasks() {}
int16_t* amy_render_audio() { return amy_simple_fill_buffer(); }
size_t amy_i2s_write(const uint8_t*, size_t nbytes) { return nbytes; }
void __real_amy_add_event(amy_event*);
void __real_amy_render(uint16_t, uint16_t, uint8_t);
void __real_amy_play_message(char*);
void __real_amy_execute_deltas();
void __real_amy_deltas_reset();
void __real_amy_stop();
void __wrap_amy_add_event(amy_event* e) { assert(owner); __real_amy_add_event(e); }
void __wrap_amy_render(uint16_t a, uint16_t b, uint8_t c) {
    assert(owner);
    if (pause_render.load()) {
        render_entered.store(true);
        while (!resume_render.load()) std::this_thread::yield();
    }
    __real_amy_render(a, b, c);
}
void __wrap_amy_play_message(char* s) { assert(owner); __real_amy_play_message(s); }
void __wrap_amy_execute_deltas() { assert(owner); __real_amy_execute_deltas(); }
void __wrap_amy_deltas_reset() { assert(owner); __real_amy_deltas_reset(); }
void __wrap_amy_stop() { assert(owner); __real_amy_stop(); }
}
using namespace smk;

static bool service(AmyAdapter& adapter) {
    OwnerScope scope;
    return AmyAdapterTestAccess::service(adapter);
}
static void consume(AmyAdapter& adapter, bool silent = false) {
    assert(!owner);
    const int16_t* samples = adapter.render();
    assert(samples);
    if (silent) for (int i = 0; i < AMY_BLOCK_SIZE * 2; ++i) assert(samples[i] == 0);
}
static void pump(AmyAdapter& adapter, int count = 8) {
    for (int i = 0; i < count; ++i) { assert(service(adapter)); consume(adapter); }
}
static void testBoundaries(AmyAdapter& adapter) {
    auto& diag = Diagnostics::instance().counters();
    const auto before = amy_global.total_blocks;
    consume(adapter, true); // Empty queue is silence, not an AMY call.
    assert(amy_global.total_blocks == before && diag.synth_pcm_starvations == 1);
    assert(service(adapter) && service(adapter));
    assert(!service(adapter)); // Producer cannot overwrite a consumer-owned slot.
    consume(adapter); consume(adapter);

    adapter.noteOn(0, 60, 100);
    adapter.noteOff(0, 60);
    assert(adapter.activeVoices() == 0); // Enqueue never touches the engine.
    pump(adapter);
    assert(adapter.activeVoices() == 0);

    for (int i = 0; i < config::kSynthCommandsPerBlock + 1; ++i) adapter.setMonoMode((i % 2) != 0);
    assert(service(adapter)); consume(adapter);
    assert(adapter.isMonoMode()); // Exactly first 16 applied.
    assert(service(adapter)); consume(adapter);
    assert(!adapter.isMonoMode());

    // Wire command must own its bytes after the console reuses its buffer.
    char wire[] = "V0.25";
    adapter.sendAmyMessage(wire);
    std::memset(wire, '!', sizeof(wire));
    pump(adapter);
    assert(std::fabs(amy_global.volume[0] - 0.25f) < 0.001f);
    const uint32_t drops = diag.synth_commands_dropped;
    std::array<char, config::kSynthMessageBytes + 1> oversized;
    oversized.fill('!'); oversized.back() = 0;
    adapter.sendAmyMessage(oversized.data());
    assert(diag.synth_commands_dropped == drops + 1);

    // Panic also cancels future events already expanded into AMY deltas.
    {
        OwnerScope scope;
        amy_event future = amy_default_event();
        future.time = amy_sysclock() + 500;
        future.osc = 119;
        future.wave = SINE;
        future.freq_coefs[COEF_CONST] = 440;
        future.amp_coefs[COEF_CONST] = 0.2f;
        future.velocity = 1;
        amy_add_event(&future);
    }
    pump(adapter, 2);
    adapter.panic();
    // Ignore prior release/filter tails; inspect after the scheduled onset.
    for (int i = 0; i < 160; ++i) { assert(service(adapter)); consume(adapter, i >= 100); }

    // Prepared sound and queued notes from before Panic must not escape.
    adapter.noteOn(0, 60, 100);
    assert(service(adapter) && service(adapter));
    adapter.noteOn(0, 64, 100);
    adapter.panic();
    const uint32_t panics = diag.synth_panics;
    consume(adapter, true); // Discards both old-generation PCM blocks.
    pump(adapter);
    assert(diag.synth_panics == panics + 1 && adapter.activeVoices() == 0);

    // Commands submitted after Panic still run after the reset.
    adapter.panic();
    adapter.noteOn(0, 67, 100);
    pump(adapter, 2);
    assert(adapter.activeVoices() == 1);
    adapter.noteOff(0, 67);
    pump(adapter);
    assert(adapter.activeVoices() == 0);

    // Ordinary commands fill only their budget; releases retain reserve.
    const uint32_t prior_drops = diag.synth_commands_dropped;
    for (int i = 0; i < config::kSynthCommandCapacity - config::kSynthReleaseReserve; ++i)
        adapter.noteOn(0, 60, 100);
    adapter.pitchBend(0, 8191);
    assert(diag.synth_commands_dropped == prior_drops + 1);
    for (int i = 0; i < config::kSynthReleaseReserve; ++i) adapter.noteOff(0, 60);
    assert(diag.synth_commands_dropped == prior_drops + 1);
    adapter.noteOff(0, 60); // Full release queue escalates to out-of-band Panic.
    assert(diag.synth_commands_dropped == prior_drops + 2);
    assert(adapter.takeRecoveryRequest()); // Application must reset generators too.
    assert(!adapter.takeRecoveryRequest());
    const uint32_t prior_panics = diag.synth_panics;
    pump(adapter, 8);
    assert(diag.synth_panics == prior_panics + 1 && adapter.activeVoices() == 0);
    assert(diag.synth_queue_high_water == config::kSynthCommandCapacity);

    // A stalled owner must not block PCM delivery. Panic arriving in the
    // middle of render invalidates the result even after it is published.
    adapter.noteOn(0, 60, 100);
    pause_render.store(true);
    std::thread stalled([&] { assert(service(adapter)); });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!render_entered.load()) {
        assert(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
    }
    adapter.panic();
    consume(adapter, true);
    const uint32_t gaps = diag.synth_pcm_starvations;
    resume_render.store(true);
    stalled.join();
    pause_render.store(false);
    consume(adapter, true);
    assert(diag.synth_pcm_starvations == gaps + 1);
    pump(adapter);
    assert(adapter.activeVoices() == 0);
}

static void testConcurrent(AmyAdapter& adapter) {
    assert(adapter.startWorker(1, 23, 16384));
    std::thread worker([] { OwnerScope scope; mock_task::run(); });
    std::atomic<int> producers{2};
    std::thread keyboard([&] {
        for (int i = 0; i < 200; ++i) {
            adapter.noteOn(0, 60 + i % 12, 100);
            adapter.pitchBend(0, i * 32);
            adapter.noteOff(0, 60 + i % 12);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        --producers;
    });
    std::thread console([&] {
        for (int i = 0; i < 100; ++i) {
            if (i % 10 == 0) adapter.loadPreset(1, i, 8);
            adapter.setFilter(1, 1000 + i * 10, 1.0f);
            if (i % 15 == 0) adapter.panic();
            int16_t scope[128]; size_t count;
            adapter.getScopeSamples(scope, 128, &count);
            assert(count == 128);
            adapter.renderLoad(); adapter.isMonoMode();
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
        --producers;
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (producers.load()) {
        consume(adapter);
        assert(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    keyboard.join(); console.join();
    const uint32_t previous_panics = Diagnostics::instance().counters().synth_panics;
    adapter.panic();
    while (Diagnostics::instance().counters().synth_panics == previous_panics) {
        consume(adapter);
        assert(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    adapter.onAudioStopped();
    worker.join();
    assert(adapter.activeVoices() == 0);
}

int main() {
    auto* adapter = new AmyAdapter;
    { OwnerScope scope; assert(!adapter->begin(44100)); assert(adapter->begin(AMY_SAMPLE_RATE)); }
    testBoundaries(*adapter);
    testConcurrent(*adapter);
    { OwnerScope scope; delete adapter; }
    // Task allocation failure leaves the engine available for orderly teardown.
    auto* failed = new AmyAdapter;
    { OwnerScope scope; assert(failed->begin(AMY_SAMPLE_RATE)); }
    mock_task::create_result = pdFALSE;
    assert(!failed->startWorker(1, 23, 16384));
    assert(failed->render() == nullptr);
    { OwnerScope scope; delete failed; }
    std::puts("PASS: exclusive AMY owner, bounded FIFO/PCM, copied wire, Panic generations, concurrent producers, shutdown and startup failure");
}
