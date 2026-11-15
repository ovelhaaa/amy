#include "amy_runtime.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

extern "C" void probe_realtime(bool);

namespace {
using namespace smk::p4;

static amy::Runtime runtime;
static uint32_t now = 0;

static void midi(uint8_t status, uint8_t id, uint8_t value) {
    const uint8_t packet[]{uint8_t(status >> 4), status, id, value};
    assert(runtime.usbInput().packet(packet, now));
}

static void arpRequest(arp::Action action, uint8_t value) {
    auto& commands = runtime.commands();
    amy::Command c{amy::CommandType::Arp, uint16_t(action), value, 0, commands.nextToken(), now};
    c.version = arp::kProtocolVersion;
    assert(commands.push(c));
}

static void setFx(const fx::FxConfig& cfg) {
    assert(runtime.commands().fx.push(fx::FxRequest{cfg, 0, 1, 0, false}));
}

void testDelayTransitions() {
    assert(runtime.begin());

    fx::FxConfig cfg{};
    cfg.delay_mode = fx::DelayMode::Sync;
    cfg.delay_division = tempo::Division::Eighth;
    cfg.delay_feedback = 0.40f;
    cfg.delay_mix = 0.50f;
    setFx(cfg);
    arpRequest(arp::Action::Bpm, 120);

    runtime.apply(now);
    runtime.loadPendingPatch();

    // Trigger short note
    midi(0x90, 60, 110);
    now += 5;
    runtime.apply(now);
    runtime.render();

    now += 20;
    midi(0x80, 60, 0);
    runtime.apply(now);
    runtime.render();

    // 1. Transition: 120 -> 90 BPM with Feedback 40%
    arpRequest(arp::Action::Bpm, 90);
    for (int i = 0; i < 40; ++i) {
        now += 5;
        runtime.apply(now);
        const int16_t* pcm = runtime.render();
        assert(pcm != nullptr);
    }
    assert(runtime.snapshot().fx.delay_dsp_ms > 0);

    // 2. Transition: 90 -> 140 BPM with Feedback 80%
    cfg.delay_feedback = 0.80f;
    setFx(cfg);
    arpRequest(arp::Action::Bpm, 140);
    for (int i = 0; i < 40; ++i) {
        now += 5;
        runtime.apply(now);
        const int16_t* pcm = runtime.render();
        assert(pcm != nullptr);
    }

    // 3. Transition: 140 -> 60 BPM with Feedback 95%
    cfg.delay_feedback = 0.95f;
    setFx(cfg);
    arpRequest(arp::Action::Bpm, 60);
    for (int i = 0; i < 50; ++i) {
        now += 5;
        runtime.apply(now);
        const int16_t* pcm = runtime.render();
        assert(pcm != nullptr);
    }

    // 4. Division shift: Eighth -> Dotted Eighth
    cfg.delay_division = tempo::Division::DottedEighth;
    setFx(cfg);
    for (int i = 0; i < 40; ++i) {
        now += 5;
        runtime.apply(now);
        const int16_t* pcm = runtime.render();
        assert(pcm != nullptr);
    }

    // 5. FREE -> SYNC -> FREE
    cfg.delay_mode = fx::DelayMode::Free;
    cfg.delay_time_ms = 450.0f;
    setFx(cfg);
    for (int i = 0; i < 20; ++i) {
        now += 5;
        runtime.apply(now);
        runtime.render();
    }
    assert(std::fabs(runtime.snapshot().fx.applied.delay_time_ms - 450.0f) < 1.0f);

    cfg.delay_mode = fx::DelayMode::Sync;
    setFx(cfg);
    for (int i = 0; i < 30; ++i) {
        now += 5;
        runtime.apply(now);
        runtime.render();
    }
    assert(runtime.snapshot().fx.applied.delay_mode == fx::DelayMode::Sync);

    // 6. Bypass during transition
    cfg.bypass_all = true;
    setFx(cfg);
    for (int i = 0; i < 20; ++i) {
        now += 5;
        runtime.apply(now);
        runtime.render();
    }

    // 7. Panic during transition
    cfg.bypass_all = false;
    setFx(cfg);
    arpRequest(arp::Action::Bpm, 130);
    now += 5;
    runtime.apply(now);
    runtime.commands().panic();
    runtime.apply(now);
    runtime.render();

    assert(runtime.snapshot().active_notes == 0);
    assert(runtime.snapshot().fx.errors == 0);

    runtime.end();
    std::printf("PASS delay_transition_0152_test: 120->90->140->60 BPM, divisions, FREE/SYNC, bypass, panic\n");
}
} // namespace

int main() {
    testDelayTransitions();
    return 0;
}
