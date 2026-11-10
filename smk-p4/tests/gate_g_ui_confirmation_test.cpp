#include "ui_blueprint_performance_adapter.h"
#include "local_performance_backend.h"
#include "amy_runtime.h"
#include "fake_nvs.h"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace smk::p4;
using namespace smk::p4::ui;

// Minimal Mock Backend for fast deterministic unit testing of adapter logic
struct MockFxBackend final : PerformanceBackend {
    PerformanceState s{};
    fx::FxState fx_state{};
    uint32_t fx_token = 0;
    uint32_t rev = 1;
    bool is_busy = false;
    bool ready = true;

    void service(uint32_t) override {}
    bool connected() const override { return ready; }
    bool busy() const override { return is_busy; }
    const PerformanceState& state() const override { return s; }
    uint32_t stateRevision() const override { return rev; }
    const char* status() const override { return ready ? "READY" : "OFFLINE"; }
    bool midiError() const override { return false; }
    uint16_t patchCount() const override { return 1; }
    uint16_t patchAt(uint16_t) const override { return 128; }
    bool patchInfo(uint16_t, PatchInfo& p, uint32_t) override { p.id = 128; p.available = true; return true; }
    bool setPatch(uint16_t, uint32_t) override { return true; }
    bool setMaster(uint8_t, uint32_t) override { return true; }
    bool setMacro(uint8_t, uint8_t, uint32_t) override { return false; }
    bool noteOn(uint8_t, uint8_t, uint32_t) override { return true; }
    void noteOff(uint8_t) override {}
    void heldNote(uint8_t) override {}
    void panic() override {}

    const char* last_param_id = nullptr;
    float last_param_value = 0.0f;

    const fx::FxState* fxState() const override { return &fx_state; }
    uint32_t fxToken() const override { return fx_token; }

    bool setFxParameter(const char* id, float value, uint32_t) override {
        if (!ready || is_busy) return false;
        ++fx_token;
        last_param_id = id;
        last_param_value = value;
        return true;
    }

    void simulateOwnerAck() {
        fx_state.request = fx_token;
        if (last_param_id && std::strcmp(last_param_id, "fx.chorus.mode") == 0) {
            fx_state.config.chorus_mode = static_cast<uint8_t>(last_param_value);
        } else if (last_param_id && std::strcmp(last_param_id, "fx.chorus.depth") == 0) {
            fx_state.config.chorus_depth = last_param_value / 100.0f;
        } else if (last_param_id && std::strcmp(last_param_id, "fx.delay.time_ms") == 0) {
            fx_state.config.delay_time_ms = last_param_value;
        }
    }

    MockFxBackend() {
        s.patch_id = 128;
        s.master = 64;
    }
};

static void test_mock_confirmation_lifecycle() {
    MockFxBackend backend;
    UiBlueprintPerformanceAdapter adapter(backend);
    adapter.service(100);

    // Initial state: chorus mode 0
    assert(adapter.model().getInt("fx.chorus.mode") == 0);

    // 1. Request chorus mode 2
    auto res = adapter.requestParameterInt("fx.chorus.mode", 2);
    assert(res.state == UiCommandState::Requested);
    assert(adapter.lastCommandState() == UiCommandState::Requested);
    assert(backend.fxToken() == 1);

    // Service before owner ACK: still Requested
    adapter.service(110);
    assert(adapter.lastCommandState() == UiCommandState::Requested);

    // Owner simulates ACK: fx_state.request advances to token 1, new revision published
    backend.simulateOwnerAck();
    ++backend.rev;
    adapter.service(120);

    assert(adapter.lastCommandState() == UiCommandState::Confirmed);
    assert(adapter.model().getInt("fx.chorus.mode") == 2);
    std::puts("  PASS: mock confirmation lifecycle");
}

static void test_mock_rejection_and_rollback() {
    MockFxBackend backend;
    backend.fx_state.config.chorus_mode = 1;
    UiBlueprintPerformanceAdapter adapter(backend);
    adapter.service(100);
    assert(adapter.model().getInt("fx.chorus.mode") == 1);

    // Request chorus mode 3
    auto res = adapter.requestParameterInt("fx.chorus.mode", 3);
    assert(res.state == UiCommandState::Requested);

    // Owner simulates rejection: error count increments, config remains mode 1
    backend.fx_state.errors = 1;
    backend.fx_state.config.chorus_mode = 1;
    ++backend.rev;
    adapter.service(120);

    assert(adapter.lastCommandState() == UiCommandState::Rejected);
    // UI model must restore the owner's confirmed value (mode 1)
    assert(adapter.model().getInt("fx.chorus.mode") == 1);
    std::puts("  PASS: mock rejection restores confirmed value");
}

static void test_mock_timeout() {
    MockFxBackend backend;
    backend.fx_state.config.delay_time_ms = 250.0f;
    UiBlueprintPerformanceAdapter adapter(backend);
    adapter.service(100);
    assert(adapter.model().getFloat("fx.delay.time_ms") == 250.0f);

    // Request delay 500 ms
    auto res = adapter.requestParameterFloat("fx.delay.time_ms", 500.0f);
    assert(res.state == UiCommandState::Requested);

    // Advance time beyond kPatchConfirmationTimeoutMs (2000 ms) without owner ACK
    adapter.service(100 + UiBlueprintPerformanceAdapter::kPatchConfirmationTimeoutMs + 50);

    assert(adapter.lastCommandState() == UiCommandState::Rejected);
    assert(std::strstr(res.notice ? res.notice : "", "WAIT") != nullptr ||
           adapter.lastCommandParameterId() != nullptr);
    // UI model retains confirmed value
    assert(adapter.model().getFloat("fx.delay.time_ms") == 250.0f);
    std::puts("  PASS: mock timeout handling");
}

static void test_real_backend_end_to_end() {
    amy::Runtime runtime;
    assert(runtime.begin());

    LocalAmyPerformanceBackend backend(runtime.commands());

    fx::NvsFxPresets fx_store;
    assert(fx_store.begin());
    backend.fxPresetStore(&fx_store);

    UiBlueprintPerformanceAdapter adapter(backend);

    // Sync initial state
    backend.service(10);
    runtime.apply(10);
    assert(runtime.render());
    backend.publish(runtime.snapshot(), false, 0, 0);
    adapter.service(10);

    assert(adapter.model().getInt("fx.chorus.mode") == 0);

    // Request Chorus mode 1 via UI adapter
    auto res = adapter.requestParameterInt("fx.chorus.mode", 1);
    assert(res.state == UiCommandState::Requested);
    assert(backend.fxToken() > 0);

    // Run backend service to dispatch FX request to runtime
    backend.service(20);
    runtime.apply(20);

    // Runtime renders audio and applies FX queue
    assert(runtime.render());
    auto snap = runtime.snapshot();
    assert(snap.fx.config.chorus_mode == 1);
    assert(snap.fx.request >= backend.fxToken());

    // Publish snapshot to backend
    backend.publish(snap, false, 0, 0);

    // Service UI adapter: should confirm
    adapter.service(30);
    assert(adapter.lastCommandState() == UiCommandState::Confirmed);
    assert(adapter.model().getInt("fx.chorus.mode") == 1);

    // Test float parameter: Delay feedback 45%
    res = adapter.requestParameterFloat("fx.delay.feedback", 45.0f);
    assert(res.state == UiCommandState::Requested);

    backend.service(40);
    runtime.apply(40);
    assert(runtime.render());
    snap = runtime.snapshot();
    assert(snap.fx.config.delay_feedback == 0.45f);

    backend.publish(snap, false, 0, 0);
    adapter.service(50);

    assert(adapter.lastCommandState() == UiCommandState::Confirmed);
    assert(adapter.model().getFloat("fx.delay.feedback") == 45.0f);

    std::puts("  PASS: real backend end-to-end confirmation");
}

int main() {
    std::puts("=== GATE G: UI FX Confirmation Tests ===");
    test_mock_confirmation_lifecycle();
    test_mock_rejection_and_rollback();
    test_mock_timeout();
    test_real_backend_end_to_end();
    std::puts("ALL GATE G TESTS PASSED.");
    return 0;
}
