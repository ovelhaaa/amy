#include "amy_runtime.h"
#include <cassert>
#include <cstdio>
#include <cstring>

extern "C" {
#include "amy.h"
void config_eq(uint8_t bus, SAMPLE eq_l, SAMPLE eq_m, SAMPLE eq_h);
void probe_realtime(bool enabled);
uint32_t probe_live();
size_t probe_bytes();
uint32_t probe_calls();
}

using namespace smk::p4;

int main() {
    std::printf("=== GATE B: AMY FX Resource Preallocation & Safety Test ===\n");

    const size_t initial_bytes = probe_bytes();
    const uint32_t initial_calls = probe_calls();

    amy::Runtime r;
    assert(r.begin());
    std::printf("PASS Runtime::begin() with FX features enabled\n");

    // 1. Verify all preallocated buffers are non-null and correctly dimensioned
    assert(amy_global.config.features.chorus == 1);
    assert(amy_global.config.features.echo == 1);
    assert(amy_global.config.features.reverb == 1);

    // Special chorus osc
    assert(synth[CHORUS_MOD_SOURCE] != nullptr);

    // Chorus delay lines and mod buffer
    assert(amy_global.bus[0]->chorus.chorus_delay_lines[0] != nullptr);
    assert(amy_global.bus[0]->chorus.chorus_delay_lines[1] != nullptr);
    assert(amy_global.bus[0]->chorus.delay_mod != nullptr);

    // Echo delay lines (capacity 65536 samples)
    assert(amy_global.bus[0]->echo.echo_delay_lines[0] != nullptr);
    assert(amy_global.bus[0]->echo.echo_delay_lines[1] != nullptr);
    assert(amy_global.bus[0]->echo.max_delay_samples == 65536);

    // Reverb comb & early reflection delay lines
    assert(amy_global.bus[0]->reverb.rev != nullptr);
    assert(amy_global.bus[0]->reverb.rev->delay_1 != nullptr);
    assert(amy_global.bus[0]->reverb.rev->delay_2 != nullptr);
    assert(amy_global.bus[0]->reverb.rev->delay_3 != nullptr);
    assert(amy_global.bus[0]->reverb.rev->delay_4 != nullptr);
    assert(amy_global.bus[0]->reverb.rev->ref_1 != nullptr);
    assert(amy_global.bus[0]->reverb.rev->ref_2 != nullptr);
    assert(amy_global.bus[0]->reverb.rev->ref_3 != nullptr);
    assert(amy_global.bus[0]->reverb.rev->ref_4 != nullptr);
    assert(amy_global.bus[0]->reverb.rev->ref_5 != nullptr);
    assert(amy_global.bus[0]->reverb.rev->ref_6 != nullptr);
    std::printf("PASS all FX delay lines and buffers confirmed non-null\n");

    // 2. Verify initial neutral / bypassed state
    assert(amy_global.bus[0]->chorus.level == 0);
    assert(amy_global.bus[0]->echo.level == 0);
    assert(amy_global.bus[0]->reverb.level == 0);
    assert(amy_global.bus[0]->eq.eq[0] == F2S(1.0f));
    assert(amy_global.bus[0]->eq.eq[1] == F2S(1.0f));
    assert(amy_global.bus[0]->eq.eq[2] == F2S(1.0f));
    std::printf("PASS initial state is 100%% neutral and bypassed\n");

    const size_t booted_bytes = probe_bytes();
    const uint32_t booted_calls = probe_calls();
    std::printf("MEMORY FOOTPRINT: total preallocated FX heap = %zu bytes across %u calls\n",
                booted_bytes - initial_bytes, booted_calls - initial_calls);

    // 3. HARD REAL-TIME SAFETY: zero allocations when tweaking parameters or rendering
    probe_realtime(true);

    // Tweaking Chorus
    config_chorus(0, 0.5f, 320, 0.5f, 0.5f);
    config_chorus(0, 0.8f, 400, 1.2f, 0.7f);
    config_chorus(0, 0.0f, 320, 0.5f, 0.5f);

    // Tweaking Echo (various delay times within preallocated 65536 capacity)
    config_echo(0, 0.25f, 10.0f, 1000.0f, 0.2f, 0.0f);
    config_echo(0, 0.50f, 350.0f, 1000.0f, 0.5f, 0.1f);
    config_echo(0, 0.75f, 999.0f, 1000.0f, 0.8f, -0.2f);
    config_echo(0, 0.00f, 350.0f, 1000.0f, 0.0f, 0.0f);

    // Tweaking Reverb
    config_reverb(0, 0.30f, 0.70f, 0.4f, 2500.0f);
    config_reverb(0, 0.60f, 0.95f, 0.6f, 4000.0f);
    config_reverb(0, 0.00f, 0.85f, 0.5f, 3000.0f);

    // Tweaking EQ
    config_eq(0, F2S(1.2f), F2S(0.8f), F2S(1.4f));
    config_eq(0, F2S(0.7f), F2S(1.1f), F2S(0.9f));
    config_eq(0, F2S(1.0f), F2S(1.0f), F2S(1.0f));

    // Render audio blocks under realtime guard
    for (int b = 0; b < 64; ++b) {
        const int16_t* pcm = r.render();
        assert(pcm != nullptr);
    }

    probe_realtime(false);
    assert(probe_bytes() == booted_bytes);
    assert(probe_calls() == booted_calls);
    std::printf("PASS zero heap allocations during live FX adjustments and audio rendering\n");

    // 4. Shutdown and resource deallocation integrity
    r.end();
    assert(probe_bytes() == initial_bytes);
    std::printf("PASS r.end() cleanly deallocated all FX delay lines with zero heap leak\n");

    std::printf("=== GATE B VERIFICATION COMPLETED: PASS ===\n");
    return 0;
}
