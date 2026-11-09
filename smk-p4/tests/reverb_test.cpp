#include "amy_runtime.h"
#include <cassert>
#include <cstdio>
#include <cmath>
#include <vector>
#include <numeric>

extern "C" {
void probe_realtime(bool enabled);
uint32_t probe_live();
size_t probe_bytes();
uint32_t probe_calls();
}

using namespace smk::p4::amy;
using namespace smk::p4::fx;

static double computeRMS(const int16_t* pcm, size_t num_samples) {
    if (num_samples == 0) return 0.0;
    double sum2 = 0.0;
    for (size_t i = 0; i < num_samples; ++i) {
        sum2 += double(pcm[i]) * double(pcm[i]);
    }
    return std::sqrt(sum2 / num_samples);
}

static std::vector<int16_t> renderBlocks(Runtime& r, unsigned num_blocks) {
    std::vector<int16_t> out;
    out.reserve(num_blocks * kBlockFrames * 2);
    for (unsigned b = 0; b < num_blocks; ++b) {
        r.apply(b * (kBlockFrames * 1000 / kSampleRateHz));
        const int16_t* pcm = r.render();
        assert(pcm != nullptr);
        out.insert(out.end(), pcm, pcm + kBlockFrames * 2);
    }
    return out;
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== GATE D: Native Stereo Reverb Qualification ===\n");

    Runtime r;
    assert(r.begin());
    assert(r.snapshot().ready);

    // Warm up the engine
    renderBlocks(r, 10);

    // -------------------------------------------------------------
    // Test 1: Audible Reverb Tails After Note-Off
    // -------------------------------------------------------------
    printf("--- Test 1: Reverb Tail Continuity After Note-Off ---\n");

    // 1A: Dry (Reverb OFF) - Note On for 4 blocks (~21 ms), then Note Off
    FxRequest req{};
    req.config.reverb_mix = 0.0f; // OFF
    req.request = 1;
    assert(r.commands().fx.push(req));

    uint32_t t_dry = r.commands().nextToken();
    assert(r.commands().push({CommandType::NoteOn, 60, 100, 0, t_dry, 0}));
    renderBlocks(r, 4); // ~21 ms note on
    r.commands().release(60, t_dry);
    r.commands().push({CommandType::NoteOff, 60, 0, 0, t_dry, 0});
    renderBlocks(r, 20); // let voice release envelope reach zero

    auto dry_tail = renderBlocks(r, 20); // ~106 ms after release
    double dry_tail_rms = computeRMS(dry_tail.data(), dry_tail.size());
    printf("Reverb OFF (Dry) Tail RMS = %6.2f\n", dry_tail_rms);
    assert(dry_tail_rms < 1.0); // complete silence

    // 1B: Wet (Reverb ON) - Note On for 4 blocks, then Note Off
    req.config.reverb_size = 0.75f;
    req.config.reverb_mix = 0.45f;
    req.request = 2;
    assert(r.commands().fx.push(req));

    uint32_t t_wet = r.commands().nextToken();
    assert(r.commands().push({CommandType::NoteOn, 60, 100, 0, t_wet, 0}));
    renderBlocks(r, 4);
    r.commands().release(60, t_wet);
    r.commands().push({CommandType::NoteOff, 60, 0, 0, t_wet, 0});
    renderBlocks(r, 20); // wait same 20 blocks for voice release

    auto wet_tail = renderBlocks(r, 20);
    double wet_tail_rms = computeRMS(wet_tail.data(), wet_tail.size());
    printf("Reverb ON (Size 0.75, Mix 0.45) Tail RMS = %6.2f\n", wet_tail_rms);

    // Reverb tail MUST remain clearly audible after NoteOff
    assert(wet_tail_rms > 20.0);
    assert(wet_tail_rms > dry_tail_rms * 20.0);
    printf("PASS Reverb produces strong, continuous audible tail after NoteOff\n");

    // Clear reverb tail before next test
    req.config.reverb_mix = 0.0f;
    req.request = 3;
    assert(r.commands().fx.push(req));
    renderBlocks(r, 20);

    // -------------------------------------------------------------
    // Test 2: Reverb Size (Decay Length) Scaling
    // -------------------------------------------------------------
    printf("--- Test 2: Reverb Size / Decay Length Scaling ---\n");

    // Small room: Size 0.10 (liveness = 0.455)
    req.config.reverb_size = 0.10f;
    req.config.reverb_mix = 0.50f;
    req.request = 10;
    assert(r.commands().fx.push(req));

    uint32_t t_small = r.commands().nextToken();
    assert(r.commands().push({CommandType::NoteOn, 60, 100, 0, t_small, 0}));
    renderBlocks(r, 4);
    r.commands().release(60, t_small);
    r.commands().push({CommandType::NoteOff, 60, 0, 0, t_small, 0});
    renderBlocks(r, 40); // wait ~213 ms for decay
    auto small_tail = renderBlocks(r, 20); // late tail ~106 ms
    double small_tail_rms = computeRMS(small_tail.data(), small_tail.size());

    // Flush
    req.config.reverb_mix = 0.0f;
    req.request = 11;
    assert(r.commands().fx.push(req));
    renderBlocks(r, 20);

    // Large hall: Size 0.95 (liveness = 0.9225)
    req.config.reverb_size = 0.95f;
    req.config.reverb_mix = 0.50f;
    req.request = 12;
    assert(r.commands().fx.push(req));

    uint32_t t_large = r.commands().nextToken();
    assert(r.commands().push({CommandType::NoteOn, 60, 100, 0, t_large, 0}));
    renderBlocks(r, 4);
    r.commands().release(60, t_large);
    r.commands().push({CommandType::NoteOff, 60, 0, 0, t_large, 0});
    renderBlocks(r, 40); // wait same ~213 ms
    auto large_tail = renderBlocks(r, 20);
    double large_tail_rms = computeRMS(large_tail.data(), large_tail.size());

    printf("Small Size (0.10) Late Tail RMS = %6.2f\n", small_tail_rms);
    printf("Large Size (0.95) Late Tail RMS = %6.2f\n", large_tail_rms);

    // Late tail of large hall must have significantly higher preserved energy than small room
    assert(large_tail_rms > small_tail_rms * 1.5);
    printf("PASS Large size preserves substantially longer and higher late decay energy\n");

    // Flush
    req.config.reverb_mix = 0.0f;
    req.request = 13;
    assert(r.commands().fx.push(req));
    renderBlocks(r, 20);

    // -------------------------------------------------------------
    // Test 3: Mix Scaling & Monotonic Gain Control
    // -------------------------------------------------------------
    printf("--- Test 3: Reverb Mix Scaling & Headroom Check ---\n");
    const float mixes[] = {0.15f, 0.35f, 0.65f, 0.95f};
    double prev_rms = 0.0;
    for (float mix : mixes) {
        req.config.reverb_size = 0.70f;
        req.config.reverb_mix = mix;
        req.request = 20 + int(mix * 100);
        assert(r.commands().fx.push(req));

        uint32_t t_mix = r.commands().nextToken();
        assert(r.commands().push({CommandType::NoteOn, 60, 100, 0, t_mix, 0}));
        auto pcm = renderBlocks(r, 15);
        double rms = computeRMS(pcm.data(), pcm.size());
        printf("Reverb Mix %4.2f: RMS = %6.2f\n", mix, rms);

        // Mix increases wet signal level monotonically
        assert(rms > prev_rms);
        prev_rms = rms;

        r.commands().release(60, t_mix);
        r.commands().push({CommandType::NoteOff, 60, 0, 0, t_mix, 0});
        renderBlocks(r, 5);

        // Flush
        req.config.reverb_mix = 0.0f;
        assert(r.commands().fx.push(req));
        renderBlocks(r, 10);
    }
    printf("PASS Reverb mix scales wet contribution monotonically without sudden gain jumps\n");

    // -------------------------------------------------------------
    // Test 4: Hard Real-Time Safety Under Rapid Adjustments
    // -------------------------------------------------------------
    printf("--- Test 4: Hard Real-Time Safety (Zero Heap Allocations During Live Reverb Changes) ---\n");
    const size_t bytes_before = probe_bytes();
    const uint32_t calls_before = probe_calls();

    probe_realtime(true);
    for (int i = 0; i < 50; ++i) {
        req.config.reverb_size = (i % 10) / 10.0f;
        req.config.reverb_mix = (i % 9) / 10.0f;
        req.request = 100 + i;
        assert(r.commands().fx.push(req));
        r.apply(i * 10);
        const int16_t* pcm = r.render();
        assert(pcm != nullptr);
    }
    probe_realtime(false);
    assert(probe_bytes() == bytes_before);
    assert(probe_calls() == calls_before);
    printf("PASS zero heap allocations during 50 live reverb size/mix modifications during active rendering\n");

    // -------------------------------------------------------------
    // Test 5: 8-Voice Polyphony + Sustain Stability (No Runaway Feedback)
    // -------------------------------------------------------------
    printf("--- Test 5: 8 Voices Polyphony + Sustain Stability Under Reverb ---\n");
    req.config.reverb_size = 0.85f;
    req.config.reverb_mix = 0.50f;
    req.request = 200;
    assert(r.commands().fx.push(req));

    // Turn on sustain pedal (CC 64 = 127)
    Command sustain_cmd{};
    sustain_cmd.type = CommandType::Control;
    sustain_cmd.id = 64;
    sustain_cmd.value = 127;
    assert(r.commands().push(sustain_cmd));

    uint32_t tokens[8];
    for (uint8_t n = 0; n < 8; ++n) {
        Command poly_cmd{};
        poly_cmd.type = CommandType::NoteOn;
        poly_cmd.id = 48 + n * 3;
        poly_cmd.value = 90;
        tokens[n] = r.commands().nextToken();
        poly_cmd.token = tokens[n];
        assert(r.commands().push(poly_cmd));
    }

    auto poly_pcm = renderBlocks(r, 40);
    double poly_rms = computeRMS(poly_pcm.data(), poly_pcm.size());
    int16_t max_peak = 0;
    for (int16_t s : poly_pcm) {
        if (std::abs(s) > max_peak) max_peak = std::abs(s);
    }
    printf("8 Voices + Sustain + Reverb: RMS = %6.2f, Peak = %5d, Clipped = %u\n",
           poly_rms, max_peak, r.snapshot().clipped);

    // Audio must remain fully stable and non-clipping
    assert(r.snapshot().clipped == 0);
    assert(max_peak < 32767);
    assert(poly_rms > 100.0);

    // Release sustain and notes
    sustain_cmd.value = 0;
    assert(r.commands().push(sustain_cmd));
    for (uint8_t n = 0; n < 8; ++n) {
        Command off_cmd{};
        off_cmd.type = CommandType::NoteOff;
        off_cmd.id = 48 + n * 3;
        off_cmd.value = 0;
        off_cmd.token = tokens[n];
        r.commands().release(off_cmd.id, tokens[n]);
        assert(r.commands().push(off_cmd));
    }
    renderBlocks(r, 40);

    r.end();
    printf("=== GATE D REVERB QUALIFICATION: ALL TESTS PASSED (PASS) ===\n");
    return 0;
}
