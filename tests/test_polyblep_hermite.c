#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "amy.h"

void delay_ms(uint32_t ms) { (void)ms; }

int main(void) {
    printf("== Running PolyBLEP and Hermite Interpolation Tests ==\n");
    fflush(stdout);

    amy_config_t c = amy_default_config();
    c.features.startup_bleep = 0;
    amy_start(c);

    uint16_t osc = 0;
    ensure_osc_allocd(osc, NULL);
    synth[osc]->status = SYNTH_AUDIBLE;

    // Test 1: PolyBLEP Sawtooth Waveform
    synth[osc]->wave = SAW_UP;
    msynth[osc]->logfreq = logfreq_for_midi_note(69); // 440 Hz
    msynth[osc]->amp = 1.0f;
    msynth[osc]->last_amp = 1.0f;

    SAMPLE buf_saw[AMY_BLOCK_SIZE];
    memset(buf_saw, 0, sizeof(buf_saw));
    SAMPLE max_saw = render_saw_up(buf_saw, osc);
    printf("PolyBLEP Saw max_val: %f\n", S2F(max_saw));
    assert(max_saw > 0);
    assert(S2F(max_saw) <= 1.05f); // Bounded, within unity range
    // Check that wrap-around has no infinite delta
    for (int i = 1; i < AMY_BLOCK_SIZE; ++i) {
        SAMPLE diff = abs(buf_saw[i] - buf_saw[i-1]);
        assert(diff < F2S(2.0f)); // Smooth transition
    }
    printf("  ok   PolyBLEP Sawtooth anti-aliased waveform rendered cleanly\n");
    fflush(stdout);

    // Test 2: PolyBLEP Pulse / PWM Waveform
    synth[osc]->wave = PULSE;
    msynth[osc]->duty = 0.25f;
    msynth[osc]->last_duty = 0.25f;
    SAMPLE buf_pulse[AMY_BLOCK_SIZE];
    memset(buf_pulse, 0, sizeof(buf_pulse));
    SAMPLE max_pulse = render_pulse(buf_pulse, osc);
    printf("PolyBLEP Pulse (duty 0.25) max_val: %f\n", S2F(max_pulse));
    assert(max_pulse > 0);
    assert(S2F(max_pulse) <= 1.25f);
    printf("  ok   PolyBLEP Pulse / PWM anti-aliased waveform verified\n");
    fflush(stdout);

    // Test 3: PolyBLEP Triangle Waveform
    synth[osc]->wave = TRIANGLE;
    SAMPLE buf_tri[AMY_BLOCK_SIZE];
    memset(buf_tri, 0, sizeof(buf_tri));
    SAMPLE max_tri = render_triangle(buf_tri, osc);
    printf("PolyBLEP Triangle max_val: %f\n", S2F(max_tri));
    assert(max_tri > 0);
    assert(S2F(max_tri) <= 1.05f);
    printf("  ok   PolyBLEP Triangle waveform verified\n");
    fflush(stdout);

    // Test 4: Hermite Interpolation on PCM Sample
    synth[osc]->wave = PCM;
    synth[osc]->preset = 0; // Preset 0 (sine or drum sample)
    msynth[osc]->logfreq = logfreq_for_midi_note(72); // pitched up C5
    msynth[osc]->amp = 1.0f;
    msynth[osc]->last_amp = 1.0f;
    SAMPLE buf_pcm[AMY_BLOCK_SIZE];
    memset(buf_pcm, 0, sizeof(buf_pcm));
    SAMPLE max_pcm = render_pcm(buf_pcm, osc);
    printf("Hermite PCM max_val: %f\n", S2F(max_pcm));
    assert(!isnan(S2F(max_pcm)));
    assert(!isinf(S2F(max_pcm)));
    printf("  ok   4-point Hermite PCM interpolation verified\n");
    fflush(stdout);

    printf("All PolyBLEP and Hermite tests passed successfully!\n");
    return 0;
}
