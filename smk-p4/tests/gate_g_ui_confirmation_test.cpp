#include "ui_blueprint_performance_adapter.h"
#include "local_performance_backend.h"
#include "amy_runtime.h"
#include "fake_nvs.h"
#include <cassert>
#include <cmath>
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

static void test_chorus_off_depth_editability_and_preservation() {
    MockFxBackend backend;
    backend.fx_state.config.chorus_mode = 0;
    backend.fx_state.config.chorus_depth = 0.65f;
    UiBlueprintPerformanceAdapter adapter(backend);
    uint32_t t = 100;
    adapter.service(t);

    assert(adapter.model().getInt("fx.chorus.mode") == 0);
    assert(std::fabs(adapter.model().getFloat("fx.chorus.depth") - 65.0f) < 0.1f);
    assert(!adapter.isParameterEditable("fx.chorus.depth"));

    // Pending transition OFF -> ON must NOT prematurely unlock Depth before owner ACK
    auto res = adapter.requestParameterInt("fx.chorus.mode", 1);
    assert(res.state == UiCommandState::Requested);
    adapter.service(t += 10);
    assert(!adapter.isParameterEditable("fx.chorus.depth"));

    // Rejected transition OFF -> ON keeps Depth locked and preserved
    ++backend.fx_state.errors;
    ++backend.rev;
    adapter.service(t += 10);
    assert(adapter.lastCommandState() == UiCommandState::Rejected);
    assert(adapter.model().getInt("fx.chorus.mode") == 0);
    assert(!adapter.isParameterEditable("fx.chorus.depth"));
    assert(std::fabs(adapter.model().getFloat("fx.chorus.depth") - 65.0f) < 0.1f);

    // Confirmed transition OFF -> ON unlocks Depth and keeps preserved value
    res = adapter.requestParameterInt("fx.chorus.mode", 2);
    assert(res.state == UiCommandState::Requested);
    backend.simulateOwnerAck();
    ++backend.rev;
    adapter.service(t += 10);
    assert(adapter.lastCommandState() == UiCommandState::Confirmed);
    assert(adapter.model().getInt("fx.chorus.mode") == 2);
    assert(adapter.isParameterEditable("fx.chorus.depth"));
    assert(std::fabs(adapter.model().getFloat("fx.chorus.depth") - 65.0f) < 0.1f);

    // Edit Depth while ON -> Confirmed 72%
    res = adapter.requestParameterFloat("fx.chorus.depth", 72.0f);
    assert(res.state == UiCommandState::Requested);
    backend.simulateOwnerAck();
    ++backend.rev;
    adapter.service(t += 10);
    assert(adapter.lastCommandState() == UiCommandState::Confirmed);
    assert(std::fabs(adapter.model().getFloat("fx.chorus.depth") - 72.0f) < 0.1f);

    // Pending ON -> OFF keeps Depth editable until OFF is confirmed
    res = adapter.requestParameterInt("fx.chorus.mode", 0);
    assert(res.state == UiCommandState::Requested);
    adapter.service(t += 10);
    assert(adapter.isParameterEditable("fx.chorus.depth"));

    // Rejected ON -> OFF stays ON and editable
    ++backend.fx_state.errors;
    ++backend.rev;
    adapter.service(t += 10);
    assert(adapter.lastCommandState() == UiCommandState::Rejected);
    assert(adapter.isParameterEditable("fx.chorus.depth"));

    // Timeout ON -> OFF stays ON and editable
    res = adapter.requestParameterInt("fx.chorus.mode", 0);
    assert(res.state == UiCommandState::Requested);
    t += UiBlueprintPerformanceAdapter::kPatchConfirmationTimeoutMs + 25;
    adapter.service(t);
    assert(adapter.lastCommandState() == UiCommandState::Rejected);
    assert(adapter.isParameterEditable("fx.chorus.depth"));

    // Confirmed ON -> OFF locks Depth while preserving 72%
    res = adapter.requestParameterInt("fx.chorus.mode", 0);
    assert(res.state == UiCommandState::Requested);
    backend.simulateOwnerAck();
    ++backend.rev;
    adapter.service(t += 10);
    assert(adapter.lastCommandState() == UiCommandState::Confirmed);
    assert(adapter.model().getInt("fx.chorus.mode") == 0);
    assert(!adapter.isParameterEditable("fx.chorus.depth"));
    assert(std::fabs(adapter.model().getFloat("fx.chorus.depth") - 72.0f) < 0.1f);

    // External owner update back to Juno II unlocks Depth with 72% preserved
    backend.fx_state.config.chorus_mode = 3;
    ++backend.rev;
    adapter.service(t += 10);
    assert(adapter.isParameterEditable("fx.chorus.depth"));
    assert(std::fabs(adapter.model().getFloat("fx.chorus.depth") - 72.0f) < 0.1f);

    // End-to-end check with real AMY runtime audio owner
    amy::Runtime runtime;
    assert(runtime.begin());
    LocalAmyPerformanceBackend real_backend(runtime.commands());
    fx::NvsFxPresets fx_store;
    assert(fx_store.begin());
    real_backend.fxPresetStore(&fx_store);
    UiBlueprintPerformanceAdapter real_adapter(real_backend);
    auto pump = [&](uint32_t dt) {
        t += dt;
        real_adapter.service(t);
        real_backend.service(t);
        runtime.apply(t);
        assert(runtime.render());
        real_backend.publish(runtime.snapshot(), false, 0, 0);
        real_adapter.service(t);
    };
    pump(10);
    assert(!real_adapter.isParameterEditable("fx.chorus.depth"));
    assert(real_adapter.requestParameterInt("fx.chorus.mode", 2).state == UiCommandState::Requested);
    pump(10);
    assert(real_adapter.isParameterEditable("fx.chorus.depth"));
    assert(real_adapter.requestParameterFloat("fx.chorus.depth", 72.0f).state == UiCommandState::Requested);
    pump(10);
    assert(std::fabs(real_adapter.model().getFloat("fx.chorus.depth") - 72.0f) < 0.1f);
    assert(runtime.snapshot().fx.applied.chorus_mode == 2);
    assert(real_adapter.requestParameterInt("fx.chorus.mode", 0).state == UiCommandState::Requested);
    pump(10);
    assert(!real_adapter.isParameterEditable("fx.chorus.depth"));
    assert(std::fabs(real_adapter.model().getFloat("fx.chorus.depth") - 72.0f) < 0.1f);
    assert(runtime.snapshot().fx.applied.chorus_mode == 0);
    assert(real_adapter.requestParameterInt("fx.chorus.mode", 3).state == UiCommandState::Requested);
    pump(10);
    assert(real_adapter.isParameterEditable("fx.chorus.depth"));
    assert(std::fabs(real_adapter.model().getFloat("fx.chorus.depth") - 72.0f) < 0.1f);
    runtime.end();

    std::puts("  PASS: Chorus OFF -> ON -> OFF Depth editability, pending/rejected & value preservation");
}

static void test_idle_service_zero_revision_bumps() {
    amy::Runtime runtime;
    assert(runtime.begin());
    LocalAmyPerformanceBackend backend(runtime.commands());
    fx::NvsFxPresets fx_store;
    assert(fx_store.begin());
    backend.fxPresetStore(&fx_store);
    UiBlueprintPerformanceAdapter adapter(backend);

    uint32_t t = 100;
    backend.service(t);
    runtime.apply(t);
    assert(runtime.render());
    backend.publish(runtime.snapshot(), false, 0, 0);
    adapter.service(t);
    // One extra cycle ensures first-snapshot initialization caches have settled.
    backend.publish(runtime.snapshot(), false, 0, 0);
    adapter.service(t += 20);

    const uint32_t rev0 = adapter.model().revision();
    for (int i = 0; i < 100; ++i) {
        backend.publish(runtime.snapshot(), false, 0, 0);
        adapter.service(t += 20);
    }
    assert(adapter.model().revision() == rev0 && "Idle publish/service must not bump UI revision");

    assert(adapter.requestParameterFloat("fx.reverb.mix", 42.0f).state == UiCommandState::Requested);
    t += 20;
    adapter.service(t);
    backend.service(t);
    runtime.apply(t);
    assert(runtime.render());
    backend.publish(runtime.snapshot(), false, 0, 0);
    adapter.service(t);
    assert(adapter.model().revision() > rev0);

    const uint32_t rev1 = adapter.model().revision();
    for (int i = 0; i < 100; ++i) {
        backend.publish(runtime.snapshot(), false, 0, 0);
        adapter.service(t += 20);
    }
    assert(adapter.model().revision() == rev1 && "Post-edit idle publish/service must not bump UI revision");
    runtime.end();

    std::puts("  PASS: 100 idle publish/service cycles produce 0 revision bumps");
}

int main() {
    std::puts("=== GATE G: UI FX Confirmation Tests ===");
    test_mock_confirmation_lifecycle();
    test_mock_rejection_and_rollback();
    test_mock_timeout();
    test_real_backend_end_to_end();
    test_chorus_off_depth_editability_and_preservation();
    test_idle_service_zero_revision_bumps();
    std::puts("ALL GATE G TESTS PASSED.");
    return 0;
}
