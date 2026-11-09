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
    printf("=== GATE E: Native Echo / Delay Qualification ===\n");

    Runtime r;
    assert(r.begin());
    assert(r.snapshot().ready);

    // Warm up the engine
    renderBlocks(r, 10);

    // -------------------------------------------------------------
    // Test 1: Echo Timing & Discrete Repetitions
    // -------------------------------------------------------------
    printf("--- Test 1: Echo Timing & Discrete Repetitions ---\n");
    // Configure delay: time = 100 ms (at 48 kHz, 100 ms = 4800 samples = ~18.75 blocks)
    // For exact block alignment: 20 blocks * 256 samples / 48 = 106.67 ms
    const float delay_ms = 106.67f; // exactly 5120 samples = 20 blocks
    FxRequest req{};
    req.config.delay_time_ms = delay_ms;
    req.config.delay_feedback = 0.50f;
    req.config.delay_mix = 0.60f;
    req.request = 1;
    assert(r.commands().fx.push(req));

    // Play a short 2-block note impulse
    uint32_t t_imp = r.commands().nextToken();
    assert(r.commands().push({CommandType::NoteOn, 60, 100, 0, t_imp, 0}));
    renderBlocks(r, 2);
    r.commands().release(60, t_imp);
    r.commands().push({CommandType::NoteOff, 60, 0, 0, t_imp, 0});
    renderBlocks(r, 2); // note release

    // Blocks 4..18: silence before the first echo arrives
    auto silence_gap = renderBlocks(r, 14);
    double gap_rms = computeRMS(silence_gap.data(), silence_gap.size());

    // Blocks 19..23: the first echo repetition arrives
    auto echo1 = renderBlocks(r, 5);
    double echo1_rms = computeRMS(echo1.data(), echo1.size());

    printf("Gap RMS (before echo) = %6.2f, Echo 1 RMS = %6.2f\n", gap_rms, echo1_rms);

    // Echo 1 MUST be substantially stronger than the gap between repetitions
    assert(echo1_rms > 15.0);
    assert(echo1_rms > gap_rms * 2.0);
    printf("PASS Delay produces discrete echo repetition at the configured time\n");

    // Flush delay line
    req.config.delay_mix = 0.0f;
    req.request = 2;
    assert(r.commands().fx.push(req));
    renderBlocks(r, 30);

    // -------------------------------------------------------------
    // Test 2: Feedback Decay & Runaway Prevention
    // -------------------------------------------------------------
    printf("--- Test 2: Feedback Stability & Runaway Prevention ---\n");
    // Maximum safe feedback = 0.95
    req.config.delay_time_ms = 50.0f;
    req.config.delay_feedback = 0.95f;
    req.config.delay_mix = 0.50f;
    req.request = 10;
    assert(r.commands().fx.push(req));

    uint32_t t_fb = r.commands().nextToken();
    assert(r.commands().push({CommandType::NoteOn, 60, 100, 0, t_fb, 0}));
    renderBlocks(r, 4);
    r.commands().release(60, t_fb);
    r.commands().push({CommandType::NoteOff, 60, 0, 0, t_fb, 0});

    // Render 60 blocks (~320 ms) under maximum feedback
    auto fb_pcm = renderBlocks(r, 60);
    int16_t peak = 0;
    for (int16_t s : fb_pcm) {
        if (std::abs(s) > peak) peak = std::abs(s);
    }
    printf("Max Feedback (0.95) Peak Sample = %5d, Clipped = %u\n", peak, r.snapshot().clipped);

    // Must never overflow 16-bit integer or cause unhandled clipping
    assert(peak < 32767);
    assert(r.snapshot().clipped == 0);
    printf("PASS Feedback 0.95 remains strictly stable without runaway or buffer overflow\n");

    // Flush
    req.config.delay_mix = 0.0f;
    req.request = 11;
    assert(r.commands().fx.push(req));
    renderBlocks(r, 30);

    // -------------------------------------------------------------
    // Test 3: Hard Real-Time Safety (Zero Heap Allocations During Live Delay Tweaks)
    // -------------------------------------------------------------
    printf("--- Test 3: Hard Real-Time Safety (Live Delay Time & Feedback Changes) ---\n");
    const size_t bytes_before = probe_bytes();
    const uint32_t calls_before = probe_calls();

    probe_realtime(true);
    for (int i = 0; i < 50; ++i) {
        req.config.delay_time_ms = 10.0f + (i * 19.5f); // 10 ms to 985 ms
        req.config.delay_feedback = (i % 9) / 10.0f;
        req.config.delay_mix = (i % 8) / 10.0f;
        req.request = 100 + i;
        assert(r.commands().fx.push(req));
        r.apply(i * 10);
        const int16_t* pcm = r.render();
        assert(pcm != nullptr);
    }
    probe_realtime(false);
    assert(probe_bytes() == bytes_before);
    assert(probe_calls() == calls_before);
    printf("PASS zero heap allocations during 50 live delay time/feedback modifications\n");

    // -------------------------------------------------------------
    // Test 4: Combined Multi-FX (Chorus + Delay + Reverb) Under 8-Voice Polyphony
    // -------------------------------------------------------------
    printf("--- Test 4: Combined Multi-FX (Chorus + Delay + Reverb) with 8 Voices ---\n");
    req.config.chorus_mode = static_cast<uint8_t>(ChorusMode::JunoI);
    req.config.chorus_depth = 0.50f;
    req.config.delay_time_ms = 250.0f;
    req.config.delay_feedback = 0.40f;
    req.config.delay_mix = 0.30f;
    req.config.reverb_size = 0.70f;
    req.config.reverb_mix = 0.30f;
    req.request = 200;
    assert(r.commands().fx.push(req));

    // Turn on sustain pedal
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
        poly_cmd.value = 85;
        tokens[n] = r.commands().nextToken();
        poly_cmd.token = tokens[n];
        assert(r.commands().push(poly_cmd));
    }

    auto combo_pcm = renderBlocks(r, 40);
    double combo_rms = computeRMS(combo_pcm.data(), combo_pcm.size());
    int16_t combo_peak = 0;
    for (int16_t s : combo_pcm) {
        if (std::abs(s) > combo_peak) combo_peak = std::abs(s);
    }
    printf("Combined (Chorus + Delay + Reverb): RMS = %6.2f, Peak = %5d, Clipped = %u\n",
           combo_rms, combo_peak, r.snapshot().clipped);

    assert(r.snapshot().clipped == 0);
    assert(combo_peak < 32767);
    assert(combo_rms > 100.0);
    printf("PASS Multi-FX bus chain operates stably without saturation or corruption\n");

    // Release
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
    printf("=== GATE E DELAY QUALIFICATION: ALL TESTS PASSED (PASS) ===\n");
    return 0;
}
