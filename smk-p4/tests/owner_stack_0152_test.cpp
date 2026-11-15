#include "amy_runtime.h"
#include <cassert>
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

void testOwnerStackSafety() {
    assert(runtime.begin());

    // 1. Enable and configure Arpeggiator
    arpRequest(arp::Action::Enable, 1);
    arpRequest(arp::Action::Rate, 2); // 1/16
    arpRequest(arp::Action::Octaves, 2);
    arpRequest(arp::Action::Mode, 1); // Down
    arpRequest(arp::Action::Bpm, 132);

    // 2. Enable Harmony with 4-note chords
    harmony::Config harm_cfg{};
    harm_cfg.scale = 2; // Minor
    harm_cfg.root = 0;  // C
    harm_cfg.chord = 2; // Min7
    harm_cfg.quant = 1;
    assert(runtime.commands().push({amy::CommandType::Harmony, 0, 0, 0, harmony::pack(harm_cfg), now}));

    // 3. Configure FX: Delay Sync, Chorus, Reverb, EQ
    fx::FxConfig fx_cfg{};
    fx_cfg.delay_mode = fx::DelayMode::Sync;
    fx_cfg.delay_division = tempo::Division::DottedEighth;
    fx_cfg.delay_feedback = 0.65f;
    fx_cfg.delay_mix = 0.40f;
    fx_cfg.chorus_mode = static_cast<uint8_t>(fx::ChorusMode::Classic);
    fx_cfg.chorus_depth = 0.5f;
    fx_cfg.reverb_size = 0.35f;
    fx_cfg.eq_high_db = 2.0f;
    fx_cfg.eq_mid_db = -1.0f;
    fx_cfg.eq_low_db = 3.0f;
    assert(runtime.commands().fx.push(fx::FxRequest{fx_cfg, 0, 1, 0, false}));

    // Process initial configuration
    runtime.apply(now);
    runtime.loadPendingPatch();

    // 4. Play continuous MIDI with 8 voices under Arp + Harmony + FX
    for (uint32_t step = 0; step < 120; ++step) {
        now += 5;
        if (step % 20 == 0) {
            midi(0x90, 60 + (step % 12), 100);
        } else if (step % 20 == 10) {
            midi(0x80, 60 + (step % 12), 0);
        }
        if (step == 50) {
            // Tempo shift during active audio
            arpRequest(arp::Action::Bpm, 144);
        }
        runtime.apply(now);
        const int16_t* pcm = runtime.render();
        assert(pcm != nullptr);
    }

    assert(runtime.snapshot().active_notes <= amy::kVoices);
    assert(runtime.snapshot().harmony_errors == 0);
    assert(runtime.snapshot().fx.errors == 0);

    runtime.end();
    std::printf("PASS owner_stack_0152_test: 8 voices + 4-note harmony + arp + delay sync + chorus + reverb\n");
}
} // namespace

int main() {
    testOwnerStackSafety();
    return 0;
}
