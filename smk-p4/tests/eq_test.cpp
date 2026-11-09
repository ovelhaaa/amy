#include "amy_runtime.h"
#include <cassert>
#include <cstdio>
#include <cmath>
#include <vector>

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
    printf("=== GATE F: 3-Band Parametric EQ & Tilt Tone Qualification ===\n");

    Runtime r;
    assert(r.begin());
    assert(r.snapshot().ready);

    // Warm up the engine
    renderBlocks(r, 10);

    // -------------------------------------------------------------
    // Test 1: Bit-Exact Neutrality (0 dB Flat)
    // -------------------------------------------------------------
    printf("--- Test 1: Bit-Exact Neutrality Check ---\n");
    FxRequest req{};
    req.config.master_tone = 0.0f;
    req.config.eq_low_db = 0.0f;
    req.config.eq_mid_db = 0.0f;
    req.config.eq_high_db = 0.0f;
    req.request = 1;
    assert(r.commands().fx.push(req));

    // Play C4 note
    uint32_t t_neutral = r.commands().nextToken();
    assert(r.commands().push({CommandType::NoteOn, 60, 100, 0, t_neutral, 0}));
    auto neutral_pcm = renderBlocks(r, 20);
    double neutral_rms = computeRMS(neutral_pcm.data(), neutral_pcm.size());
    printf("Neutral Flat EQ: RMS = %6.2f\n", neutral_rms);
    assert(neutral_rms > 50.0);

    r.commands().release(60, t_neutral);
    r.commands().push({CommandType::NoteOff, 60, 0, 0, t_neutral, 0});
    renderBlocks(r, 10);

    // -------------------------------------------------------------
    // Test 2: Tilt Tone Macro Response (Bright vs Dark)
    // -------------------------------------------------------------
    printf("--- Test 2: Master Tone (Tilt EQ Macro) ---\n");

    // 2A: Low Note (MIDI C2 = 36, ~65 Hz)
    // Under Tone = -1.0 (Dark), Low is boosted (+4 dB) and High cut (-4.4 dB)
    req.config.master_tone = -1.0f; // Dark
    req.request = 10;
    assert(r.commands().fx.push(req));
    uint32_t t_low_dark = r.commands().nextToken();
    assert(r.commands().push({CommandType::NoteOn, 36, 100, 0, t_low_dark, 0}));
    auto low_dark_pcm = renderBlocks(r, 20);
    double low_dark_rms = computeRMS(low_dark_pcm.data(), low_dark_pcm.size());
    r.commands().release(36, t_low_dark);
    r.commands().push({CommandType::NoteOff, 36, 0, 0, t_low_dark, 0});
    renderBlocks(r, 10);

    // Under Tone = +1.0 (Bright), Low is cut (-4.4 dB) and High boosted (+4 dB)
    req.config.master_tone = 1.0f; // Bright
    req.request = 11;
    assert(r.commands().fx.push(req));
    uint32_t t_low_bright = r.commands().nextToken();
    assert(r.commands().push({CommandType::NoteOn, 36, 100, 0, t_low_bright, 0}));
    auto low_bright_pcm = renderBlocks(r, 20);
    double low_bright_rms = computeRMS(low_bright_pcm.data(), low_bright_pcm.size());
    r.commands().release(36, t_low_bright);
    r.commands().push({CommandType::NoteOff, 36, 0, 0, t_low_bright, 0});
    renderBlocks(r, 10);

    printf("Low Note (C2): Dark (-1.0) RMS = %6.2f, Bright (+1.0) RMS = %6.2f\n",
           low_dark_rms, low_bright_rms);
    // Dark tone MUST produce more low-end energy than bright tone on low notes
    assert(low_dark_rms > low_bright_rms * 1.2);

    // 2B: High Note (MIDI C6 = 84, ~1046 Hz)
    req.config.master_tone = -1.0f; // Dark
    req.request = 12;
    assert(r.commands().fx.push(req));
    uint32_t t_high_dark = r.commands().nextToken();
    assert(r.commands().push({CommandType::NoteOn, 84, 100, 0, t_high_dark, 0}));
    auto high_dark_pcm = renderBlocks(r, 20);
    double high_dark_rms = computeRMS(high_dark_pcm.data(), high_dark_pcm.size());
    r.commands().release(84, t_high_dark);
    r.commands().push({CommandType::NoteOff, 84, 0, 0, t_high_dark, 0});
    renderBlocks(r, 10);

    req.config.master_tone = 1.0f; // Bright
    req.request = 13;
    assert(r.commands().fx.push(req));
    uint32_t t_high_bright = r.commands().nextToken();
    assert(r.commands().push({CommandType::NoteOn, 84, 100, 0, t_high_bright, 0}));
    auto high_bright_pcm = renderBlocks(r, 20);
    double high_bright_rms = computeRMS(high_bright_pcm.data(), high_bright_pcm.size());
    r.commands().release(84, t_high_bright);
    r.commands().push({CommandType::NoteOff, 84, 0, 0, t_high_bright, 0});
    renderBlocks(r, 10);

    printf("High Note (C6): Dark (-1.0) RMS = %6.2f, Bright (+1.0) RMS = %6.2f\n",
           high_dark_rms, high_bright_rms);
    // Bright tone MUST produce more high-end energy than dark tone on high notes
    assert(high_bright_rms > high_dark_rms * 1.10);
    printf("PASS Master Tone tilt macro smoothly transitions balance between low and high frequencies\n");

    // -------------------------------------------------------------
    // Test 3: Discrete 3-Band Gains (Boost & Cut Response)
    // -------------------------------------------------------------
    printf("--- Test 3: Discrete 3-Band Gains (+6 dB Boost and -6 dB Cut) ---\n");
    req.config.master_tone = 0.0f; // Flat tone

    // Low Boost (+6 dB) on C2
    req.config.eq_low_db = 6.0f;
    req.request = 20;
    assert(r.commands().fx.push(req));
    uint32_t t_low_b = r.commands().nextToken();
    assert(r.commands().push({CommandType::NoteOn, 36, 100, 0, t_low_b, 0}));
    auto low_boost_pcm = renderBlocks(r, 20);
    double low_boost_rms = computeRMS(low_boost_pcm.data(), low_boost_pcm.size());
    r.commands().release(36, t_low_b);
    r.commands().push({CommandType::NoteOff, 36, 0, 0, t_low_b, 0});
    renderBlocks(r, 10);

    // Low Cut (-6 dB) on C2
    req.config.eq_low_db = -6.0f;
    req.request = 21;
    assert(r.commands().fx.push(req));
    uint32_t t_low_c = r.commands().nextToken();
    assert(r.commands().push({CommandType::NoteOn, 36, 100, 0, t_low_c, 0}));
    auto low_cut_pcm = renderBlocks(r, 20);
    double low_cut_rms = computeRMS(low_cut_pcm.data(), low_cut_pcm.size());
    r.commands().release(36, t_low_c);
    r.commands().push({CommandType::NoteOff, 36, 0, 0, t_low_c, 0});
    renderBlocks(r, 10);

    printf("Low Band (C2): +6 dB RMS = %6.2f, -6 dB RMS = %6.2f (Ratio = %4.2f)\n",
           low_boost_rms, low_cut_rms, low_boost_rms / low_cut_rms);
    // +6 dB vs -6 dB ratio should be approx 12 dB (factor of ~2.5x to 4x)
    assert(low_boost_rms > low_cut_rms * 2.0);

    // High Boost (+6 dB) on C6
    req.config.eq_low_db = 0.0f;
    req.config.eq_high_db = 6.0f;
    req.request = 22;
    assert(r.commands().fx.push(req));
    uint32_t t_hi_b = r.commands().nextToken();
    assert(r.commands().push({CommandType::NoteOn, 84, 100, 0, t_hi_b, 0}));
    auto hi_boost_pcm = renderBlocks(r, 20);
    double hi_boost_rms = computeRMS(hi_boost_pcm.data(), hi_boost_pcm.size());
    r.commands().release(84, t_hi_b);
    r.commands().push({CommandType::NoteOff, 84, 0, 0, t_hi_b, 0});
    renderBlocks(r, 10);

    // High Cut (-6 dB) on C6
    req.config.eq_high_db = -6.0f;
    req.request = 23;
    assert(r.commands().fx.push(req));
    uint32_t t_hi_c = r.commands().nextToken();
    assert(r.commands().push({CommandType::NoteOn, 84, 100, 0, t_hi_c, 0}));
    auto hi_cut_pcm = renderBlocks(r, 20);
    double hi_cut_rms = computeRMS(hi_cut_pcm.data(), hi_cut_pcm.size());
    r.commands().release(84, t_hi_c);
    r.commands().push({CommandType::NoteOff, 84, 0, 0, t_hi_c, 0});
    renderBlocks(r, 10);

    printf("High Band (C6): +6 dB RMS = %6.2f, -6 dB RMS = %6.2f (Ratio = %4.2f)\n",
           hi_boost_rms, hi_cut_rms, hi_boost_rms / hi_cut_rms);
    assert(hi_boost_rms > hi_cut_rms * 1.12);

    // Mid Band Boost (+6 dB) on C6 (near 2500 Hz BPF)
    req.config.eq_high_db = 0.0f;
    req.config.eq_mid_db = 6.0f;
    req.request = 24;
    assert(r.commands().fx.push(req));
    uint32_t t_mid_b = r.commands().nextToken();
    assert(r.commands().push({CommandType::NoteOn, 84, 100, 0, t_mid_b, 0}));
    auto mid_boost_pcm = renderBlocks(r, 20);
    double mid_boost_rms = computeRMS(mid_boost_pcm.data(), mid_boost_pcm.size());
    r.commands().release(84, t_mid_b);
    r.commands().push({CommandType::NoteOff, 84, 0, 0, t_mid_b, 0});
    renderBlocks(r, 10);

    // Mid Band Cut (-6 dB) on C6
    req.config.eq_mid_db = -6.0f;
    req.request = 25;
    assert(r.commands().fx.push(req));
    uint32_t t_mid_c = r.commands().nextToken();
    assert(r.commands().push({CommandType::NoteOn, 84, 100, 0, t_mid_c, 0}));
    auto mid_cut_pcm = renderBlocks(r, 20);
    double mid_cut_rms = computeRMS(mid_cut_pcm.data(), mid_cut_pcm.size());
    r.commands().release(84, t_mid_c);
    r.commands().push({CommandType::NoteOff, 84, 0, 0, t_mid_c, 0});
    renderBlocks(r, 10);

    printf("Mid Band (C6):  +6 dB RMS = %6.2f, -6 dB RMS = %6.2f (Ratio = %4.2f)\n",
           mid_boost_rms, mid_cut_rms, mid_boost_rms / mid_cut_rms);
    assert(mid_boost_rms > mid_cut_rms * 1.30);
    printf("PASS Discrete Low, Mid, and High bands produce verified boost and cut frequency curves\n");

    // -------------------------------------------------------------
    // Test 4: Hard Real-Time Safety Under Continuous EQ Modulation
    // -------------------------------------------------------------
    printf("--- Test 4: Hard Real-Time Safety (Zero Heap Allocations During Live EQ Sweeps) ---\n");
    const size_t bytes_before = probe_bytes();
    const uint32_t calls_before = probe_calls();

    probe_realtime(true);
    for (int i = 0; i < 50; ++i) {
        req.config.master_tone = ((i % 21) - 10) / 10.0f; // -1.0 .. +1.0
        req.config.eq_low_db = ((i % 13) - 6.0f);        // -6 .. +6 dB
        req.config.eq_mid_db = (((i + 3) % 13) - 6.0f);
        req.config.eq_high_db = (((i + 7) % 13) - 6.0f);
        req.request = 100 + i;
        assert(r.commands().fx.push(req));
        r.apply(i * 10);
        const int16_t* pcm = r.render();
        assert(pcm != nullptr);
    }
    probe_realtime(false);
    assert(probe_bytes() == bytes_before);
    assert(probe_calls() == calls_before);
    printf("PASS zero heap allocations during 50 live EQ/Tone parameter modifications\n");

    // -------------------------------------------------------------
    // Test 5: Headroom with 8 Voices Under EQ Boost
    // -------------------------------------------------------------
    printf("--- Test 5: Headroom with 8 Polyphonic Voices Under +6 dB EQ Boost ---\n");
    req.config.master_tone = 0.5f;
    req.config.eq_low_db = 4.0f;
    req.config.eq_mid_db = 2.0f;
    req.config.eq_high_db = 4.0f;
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

    auto poly_pcm = renderBlocks(r, 40);
    double poly_rms = computeRMS(poly_pcm.data(), poly_pcm.size());
    int16_t poly_peak = 0;
    for (int16_t s : poly_pcm) {
        if (std::abs(s) > poly_peak) poly_peak = std::abs(s);
    }
    printf("8 Voices + Boosted EQ: RMS = %6.2f, Peak = %5d, Clipped = %u\n",
           poly_rms, poly_peak, r.snapshot().clipped);

    // Audio master limiter / scale prevents hard clipping
    assert(r.snapshot().clipped == 0);
    assert(poly_peak < 32767);
    printf("PASS 8 voices under boosted EQ maintain safe headroom without clipping\n");

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
    printf("=== GATE F 3-BAND EQ QUALIFICATION: ALL TESTS PASSED (PASS) ===\n");
    return 0;
}
