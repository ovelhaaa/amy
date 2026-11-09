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

struct StereoStats {
    double rms_l = 0;
    double rms_r = 0;
    double rms_mid = 0;
    double rms_side = 0;
    double correlation = 0;
    int16_t peak_l = 0;
    int16_t peak_r = 0;
};

static StereoStats computeStereoStats(const std::vector<int16_t>& pcm) {
    StereoStats stats{};
    size_t num_frames = pcm.size() / 2;
    if (num_frames == 0) return stats;

    double sum_l2 = 0;
    double sum_r2 = 0;
    double sum_lr = 0;
    double sum_mid2 = 0;
    double sum_side2 = 0;

    for (size_t f = 0; f < num_frames; ++f) {
        double l = pcm[f * 2 + 0];
        double r = pcm[f * 2 + 1];
        if (std::abs(l) > stats.peak_l) stats.peak_l = std::abs(l);
        if (std::abs(r) > stats.peak_r) stats.peak_r = std::abs(r);

        double mid = 0.5 * (l + r);
        double side = 0.5 * (l - r);

        sum_l2 += l * l;
        sum_r2 += r * r;
        sum_lr += l * r;
        sum_mid2 += mid * mid;
        sum_side2 += side * side;
    }

    stats.rms_l = std::sqrt(sum_l2 / num_frames);
    stats.rms_r = std::sqrt(sum_r2 / num_frames);
    stats.rms_mid = std::sqrt(sum_mid2 / num_frames);
    stats.rms_side = std::sqrt(sum_side2 / num_frames);

    double denom = std::sqrt(sum_l2 * sum_r2);
    stats.correlation = (denom > 1e-6) ? (sum_lr / denom) : 1.0;

    return stats;
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
    printf("=== GATE C: Native Stereo Chorus Qualification ===\n");

    Runtime r;
    assert(r.begin());
    assert(r.snapshot().ready);

    // Warm up the engine
    renderBlocks(r, 10);

    // -------------------------------------------------------------
    // Test 1: Stereo Decorrelation & Side Energy Across All Modes
    // -------------------------------------------------------------
    printf("--- Test 1: Chorus Modes Analysis (Mono Source -> Stereo Widening) ---\n");
    // Play middle C on voice (centered dry mono)
    uint32_t note_token = r.commands().nextToken();
    assert(r.commands().push({CommandType::NoteOn, 60, 100, 0, note_token, 0}));

    // Render dry (Mode = Off)
    FxRequest req{};
    req.config.chorus_mode = static_cast<uint8_t>(ChorusMode::Off);
    req.request = 1;
    assert(r.commands().fx.push(req));

    auto dry_pcm = renderBlocks(r, 20); // ~106 ms
    StereoStats dry_stats = computeStereoStats(dry_pcm);
    printf("Mode OFF:      RMS_Mid=%6.1f, RMS_Side=%6.1f, Correlation=%7.4f, Peak=(%5d, %5d)\n",
           dry_stats.rms_mid, dry_stats.rms_side, dry_stats.correlation, dry_stats.peak_l, dry_stats.peak_r);

    // In Mode OFF, dry mono panned center must have zero side energy and correlation = 1.0
    assert(dry_stats.rms_side < 1.0);
    assert(dry_stats.correlation > 0.999);

    r.commands().release(60, note_token);
    r.commands().push({CommandType::NoteOff, 60, 0, 0, note_token, 0});
    renderBlocks(r, 5);

    const char* mode_names[] = {"Off", "Classic", "JunoI", "JunoII", "Ensemble", "Vibrato"};
    StereoStats mode_stats[6];
    mode_stats[0] = dry_stats;

    for (int m = 1; m < 6; ++m) {
        // Re-trigger fresh note for fair comparison against dry envelope
        uint32_t t = r.commands().nextToken();
        assert(r.commands().push({CommandType::NoteOn, 60, 100, 0, t, 0}));

        req.config.chorus_mode = m;
        req.config.chorus_depth = 0.60f;
        req.request = m + 10;
        assert(r.commands().fx.push(req));

        auto pcm = renderBlocks(r, 20); // identical duration to dry_pcm
        mode_stats[m] = computeStereoStats(pcm);
        printf("Mode %-8s: RMS_Mid=%6.1f, RMS_Side=%6.1f, Correlation=%7.4f, Peak=(%5d, %5d)\n",
               mode_names[m], mode_stats[m].rms_mid, mode_stats[m].rms_side, mode_stats[m].correlation,
               mode_stats[m].peak_l, mode_stats[m].peak_r);
        fflush(stdout);

        // Every active chorus mode MUST create measurable stereo widening (Side energy > 0)
        assert(mode_stats[m].rms_side > 20.0);
        // Stereo correlation must be decorrelated (< 0.99)
        assert(mode_stats[m].correlation < 0.95);
        // Mono compatibility: Mid energy must remain dominant (no phase cancellation destroying mono sum)
        assert(mode_stats[m].rms_mid > 50.0);
        assert(mode_stats[m].rms_mid > mode_stats[m].rms_side);
        // No clipping
        assert(mode_stats[m].peak_l < 32767 && mode_stats[m].peak_r < 32767);

        // Release note
        r.commands().release(60, t);
        r.commands().push({CommandType::NoteOff, 60, 0, 0, t, 0});
        renderBlocks(r, 5);
    }
    printf("PASS all 5 active chorus modes produce measurable stereo decorrelation and side energy\n");

    // -------------------------------------------------------------
    // Test 2: Depth Scaling Verification (Min vs Max Depth)
    // -------------------------------------------------------------
    printf("--- Test 2: Chorus Depth Response ---\n");
    req.config.chorus_mode = static_cast<uint8_t>(ChorusMode::JunoI);
    req.config.chorus_depth = 0.15f; // low depth
    req.request = 20;
    assert(r.commands().fx.push(req));
    uint32_t t_low = r.commands().nextToken();
    assert(r.commands().push({CommandType::NoteOn, 60, 100, 0, t_low, 0}));
    auto low_depth_pcm = renderBlocks(r, 20);
    StereoStats low_depth_stats = computeStereoStats(low_depth_pcm);
    r.commands().release(60, t_low);
    r.commands().push({CommandType::NoteOff, 60, 0, 0, t_low, 0});
    renderBlocks(r, 5);

    req.config.chorus_depth = 0.95f; // high depth
    req.request = 21;
    assert(r.commands().fx.push(req));
    uint32_t t_high = r.commands().nextToken();
    assert(r.commands().push({CommandType::NoteOn, 60, 100, 0, t_high, 0}));
    auto high_depth_pcm = renderBlocks(r, 20);
    StereoStats high_depth_stats = computeStereoStats(high_depth_pcm);
    r.commands().release(60, t_high);
    r.commands().push({CommandType::NoteOff, 60, 0, 0, t_high, 0});
    renderBlocks(r, 5);

    double pcm_depth_diff2 = 0;
    for (size_t i = 0; i < low_depth_pcm.size(); ++i) {
        double d = high_depth_pcm[i] - low_depth_pcm[i];
        pcm_depth_diff2 += d * d;
    }
    double depth_diff_rms = std::sqrt(pcm_depth_diff2 / low_depth_pcm.size());
    printf("JunoI Depth 0.15: RMS_Side=%6.1f, Correlation=%7.4f\n", low_depth_stats.rms_side, low_depth_stats.correlation);
    printf("JunoI Depth 0.95: RMS_Side=%6.1f, Correlation=%7.4f\n", high_depth_stats.rms_side, high_depth_stats.correlation);
    printf("Depth Min vs Max PCM Difference: RMS_Diff=%6.1f\n", depth_diff_rms);

    // Depth min vs max must produce significant PCM variation
    assert(depth_diff_rms > 50.0);
    printf("PASS Depth min vs max produces confirmed measurable PCM difference\n");

    // Rate Slow (Juno I, 0.4 Hz) vs Fast (Vibrato/Ensemble, 2.5 Hz)
    req.config.chorus_mode = static_cast<uint8_t>(ChorusMode::JunoI);
    req.config.chorus_depth = 0.50f;
    req.request = 25;
    assert(r.commands().fx.push(req));
    uint32_t t_slow = r.commands().nextToken();
    assert(r.commands().push({CommandType::NoteOn, 60, 100, 0, t_slow, 0}));
    auto slow_rate_pcm = renderBlocks(r, 20);
    r.commands().release(60, t_slow);
    r.commands().push({CommandType::NoteOff, 60, 0, 0, t_slow, 0});
    renderBlocks(r, 5);

    req.config.chorus_mode = static_cast<uint8_t>(ChorusMode::Vibrato);
    req.config.chorus_depth = 0.50f;
    req.request = 26;
    assert(r.commands().fx.push(req));
    uint32_t t_fast = r.commands().nextToken();
    assert(r.commands().push({CommandType::NoteOn, 60, 100, 0, t_fast, 0}));
    auto fast_rate_pcm = renderBlocks(r, 20);
    r.commands().release(60, t_fast);
    r.commands().push({CommandType::NoteOff, 60, 0, 0, t_fast, 0});
    renderBlocks(r, 5);

    double pcm_rate_diff2 = 0;
    for (size_t i = 0; i < slow_rate_pcm.size(); ++i) {
        double d = fast_rate_pcm[i] - slow_rate_pcm[i];
        pcm_rate_diff2 += d * d;
    }
    double rate_diff_rms = std::sqrt(pcm_rate_diff2 / slow_rate_pcm.size());
    printf("Rate Slow (JunoI) vs Fast (Vibrato) PCM Difference: RMS_Diff=%6.1f\n", rate_diff_rms);
    assert(rate_diff_rms > 50.0);
    printf("PASS Rate slow vs fast produces confirmed measurable PCM difference\n");

    // -------------------------------------------------------------
    // Test 3: Bypass / Return to Clean Neutral
    // -------------------------------------------------------------
    printf("--- Test 3: Bypass / Return to Neutral ---\n");
    req.config.chorus_mode = static_cast<uint8_t>(ChorusMode::Off);
    req.request = 30;
    assert(r.commands().fx.push(req));
    uint32_t t_byp = r.commands().nextToken();
    assert(r.commands().push({CommandType::NoteOn, 60, 100, 0, t_byp, 0}));
    // Let chorus delay line clear
    renderBlocks(r, 5);
    auto bypassed_pcm = renderBlocks(r, 20);
    StereoStats bypassed_stats = computeStereoStats(bypassed_pcm);
    r.commands().release(60, t_byp);
    r.commands().push({CommandType::NoteOff, 60, 0, 0, t_byp, 0});
    renderBlocks(r, 5);

    printf("Return to OFF: RMS_Mid=%6.1f, RMS_Side=%6.1f, Correlation=%7.4f\n",
           bypassed_stats.rms_mid, bypassed_stats.rms_side, bypassed_stats.correlation);
    assert(bypassed_stats.rms_side < 1.0);
    assert(bypassed_stats.correlation > 0.999);
    printf("PASS returning to Mode OFF cleanly restores zero-side mono center\n");

    // -------------------------------------------------------------
    // Test 4: Hard Real-Time Safety Under Rapid Adjustments
    // -------------------------------------------------------------
    printf("--- Test 4: Hard Real-Time Safety (Zero Heap Allocations During Live FX Changes) ---\n");
    const size_t bytes_before = probe_bytes();
    const uint32_t calls_before = probe_calls();
    probe_realtime(true);
    for (int i = 0; i < 50; ++i) {
        req.config.chorus_mode = (i % 5) + 1;
        req.config.chorus_depth = (i % 10) / 10.0f;
        req.request = 100 + i;
        assert(r.commands().fx.push(req));
        r.apply(i * 10);
        const int16_t* pcm = r.render();
        assert(pcm != nullptr);
    }
    probe_realtime(false);
    assert(probe_bytes() == bytes_before);
    assert(probe_calls() == calls_before);
    printf("PASS zero heap allocations during 50 live chorus mode/depth modifications during active rendering\n");

    // -------------------------------------------------------------
    // Test 5: Polyphony, Harmony, Sustain and Patch Switching
    // -------------------------------------------------------------
    printf("--- Test 5: 8 Voices Polyphony, Harmony & Sustain with Chorus ---\n");
    // Enable Chorus Juno II
    req.config.chorus_mode = static_cast<uint8_t>(ChorusMode::JunoII);
    req.config.chorus_depth = 0.50f;
    req.request = 200;
    assert(r.commands().fx.push(req));

    // Turn on sustain pedal (CC 64 = 127)
    Command sustain_cmd{};
    sustain_cmd.type = CommandType::Control;
    sustain_cmd.id = 64;
    sustain_cmd.value = 127;
    assert(r.commands().push(sustain_cmd));

    // Play 8 notes
    uint32_t tokens[8];
    for (uint8_t n = 0; n < 8; ++n) {
        Command poly_cmd{};
        poly_cmd.type = CommandType::NoteOn;
        poly_cmd.id = 48 + n * 3; // C3, Eb3, F#3, A3, C4...
        poly_cmd.value = 90;
        tokens[n] = r.commands().nextToken();
        poly_cmd.token = tokens[n];
        assert(r.commands().push(poly_cmd));
    }
    auto poly_pcm = renderBlocks(r, 40);
    StereoStats poly_stats = computeStereoStats(poly_pcm);
    printf("8 Voices + Sustain: RMS_Mid=%6.1f, RMS_Side=%6.1f, Correlation=%7.4f, Peak=(%5d, %5d)\n",
           poly_stats.rms_mid, poly_stats.rms_side, poly_stats.correlation, poly_stats.peak_l, poly_stats.peak_r);
    assert(poly_stats.rms_side > 200.0);
    assert(poly_stats.correlation < 0.90);
    assert(r.snapshot().clipped == 0);

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

    // Test with Factory Patches (Moving Strings patch 4, Brass Juno patch 8, DX7 Piano patch 128)
    const uint16_t test_patches[] = {4, 8, 128};
    for (uint16_t patch_id : test_patches) {
        Command patch_cmd{};
        patch_cmd.type = CommandType::Patch;
        patch_cmd.id = patch_id;
        assert(r.commands().push(patch_cmd));
        r.apply(1000);
        r.loadPendingPatch();
        assert(r.snapshot().patch_state == PatchState::Ready);

        // Turn on Chorus Ensemble
        req.config.chorus_mode = static_cast<uint8_t>(ChorusMode::Ensemble);
        req.config.chorus_depth = 0.50f;
        req.request = 300 + patch_id;
        assert(r.commands().fx.push(req));

        Command test_note{};
        test_note.type = CommandType::NoteOn;
        test_note.id = 60;
        test_note.value = 100;
        test_note.token = r.commands().nextToken();
        assert(r.commands().push(test_note));

        auto patch_pcm = renderBlocks(r, 30);
        StereoStats p_stats = computeStereoStats(patch_pcm);
        printf("Patch %3u (Ensemble): RMS_Mid=%6.1f, RMS_Side=%6.1f, Correlation=%7.4f\n",
               patch_id, p_stats.rms_mid, p_stats.rms_side, p_stats.correlation);
        assert(p_stats.rms_side > 15.0);
        assert(p_stats.correlation < 0.95);

        test_note.type = CommandType::NoteOff;
        r.commands().release(test_note.id, test_note.token);
        assert(r.commands().push(test_note));
        renderBlocks(r, 20);
    }

    r.end();
    printf("=== GATE C CHORUS QUALIFICATION: ALL TESTS PASSED (PASS) ===\n");
    return 0;
}
