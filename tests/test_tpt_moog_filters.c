#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "amy.h"

void delay_ms(uint32_t ms) { (void)ms; }

int main(void) {
    printf("== Running TPT SVF and Moog 24dB Filter Tests ==\n");
    fflush(stdout);

    amy_config_t c = amy_default_config();
    c.features.startup_bleep = 0;
    amy_start(c);

    // Test 1: TPT SVF Filter Processing and Stability
    uint16_t osc = 0;
    ensure_osc_allocd(osc, NULL);
    synth[osc]->wave = SINE;
    synth[osc]->filter_type = FILTER_TPT_SVF;
    synth[osc]->status = SYNTH_AUDIBLE;
    msynth[osc]->filter_logfreq = logfreq_for_midi_note(69); // A4 = 440 Hz
    msynth[osc]->resonance = 2.0f;

    SAMPLE block[AMY_BLOCK_SIZE];
    for (int i = 0; i < AMY_BLOCK_SIZE; ++i) {
        block[i] = F2S(0.5f * sinf(2.0f * (float)M_PI * (float)i / 32.0f));
    }

    SAMPLE max_val = filter_process(block, osc, F2S(0.5f));
    printf("TPT SVF output max_val: %f\n", S2F(max_val));
    assert(max_val > 0);
    assert(S2F(max_val) < 2.0f); // bounded
    printf("  ok   TPT SVF 12dB lowpass processing stable\n");
    fflush(stdout);

    // Test 2: Rapid Cutoff Sweeps on TPT SVF (Sweep from 50Hz to 12kHz in steps)
    for (int step = 0; step < 50; ++step) {
        float note = 20.0f + (float)step * 2.0f; // MIDI 20 to 120
        msynth[osc]->filter_logfreq = logfreq_for_midi_note(note);
        msynth[osc]->resonance = 4.0f; // high resonance
        for (int i = 0; i < AMY_BLOCK_SIZE; ++i) {
            block[i] = F2S(0.5f * ((i % 16 > 8) ? 1.0f : -1.0f)); // square input
        }
        max_val = filter_process(block, osc, F2S(0.5f));
        assert(!isnan(S2F(max_val)));
        assert(!isinf(S2F(max_val)));
        assert(S2F(max_val) < 10.0f); // Zero Delay Feedback prevents unbounded explosion
    }
    printf("  ok   TPT SVF rapid cutoff sweep test passed without instability\n");
    fflush(stdout);

    // Test 3: Moog 24dB Ladder Filter with Resonance Saturation
    synth[osc]->filter_type = FILTER_MOOG24;
    reset_filter(osc);
    msynth[osc]->filter_logfreq = logfreq_for_midi_note(60); // C4 ~ 261 Hz
    msynth[osc]->resonance = 7.5f; // very high resonance near self-oscillation

    for (int i = 0; i < AMY_BLOCK_SIZE; ++i) {
        block[i] = F2S(0.8f * ((i % 32 > 16) ? 1.0f : -1.0f));
    }

    max_val = filter_process(block, osc, F2S(0.8f));
    printf("Moog 24dB output max_val with high resonance: %f\n", S2F(max_val));
    assert(max_val > 0);
    assert(S2F(max_val) <= 4.0f); // Saturated ladder limits peak amplitude cleanly
    printf("  ok   Moog 24dB non-linear saturation test passed\n");
    fflush(stdout);

    // Test 4: Slew-rate limiter on hold_and_modify
    amy_global.total_samples = 1000; // Past initial note-on
    synth[osc]->note_on_clock = 0;
    msynth[osc]->last_filter_logfreq = logfreq_for_midi_note(30);
    synth[osc]->filter_logfreq_coefs[COEF_CONST] = logfreq_for_midi_note(100);
    synth[osc]->resonance = 2.0f;
    hold_and_modify(osc);
    printf("msynth.last_filter_logfreq=%f, target=%f, msynth.filter_logfreq=%f\n",
           msynth[osc]->last_filter_logfreq,
           logfreq_for_midi_note(100),
           msynth[osc]->filter_logfreq);
    fflush(stdout);
    assert(msynth[osc]->filter_logfreq < logfreq_for_midi_note(95));
    printf("  ok   slew-rate smoothing on cutoff frequency verified\n");
    fflush(stdout);

    printf("All TPT SVF and Moog 24dB filter tests passed successfully!\n");
    return 0;
}
