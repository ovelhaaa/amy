#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "amy.h"

void delay_ms(uint32_t ms) { (void)ms; }
void amy_platform_init(void) {}
void amy_platform_deinit(void) {}
void miniaudio_start(void) {}
void miniaudio_stop(void) {}
void amy_update_tasks(void) {}
int16_t *amy_render_audio(void) { return NULL; }
size_t amy_i2s_write(const uint8_t *buffer, size_t nbytes) { return nbytes; }

int main(void) {
    printf("=========================================================\n");
    printf("=== Testing All 256 Patches for Audio Quality & Noise ===\n");
    printf("=========================================================\n");

    int anomaly_count = 0;

    for (int p = 0; p < 256; ++p) {
        amy_config_t c = amy_default_config();
        c.features.startup_bleep = 0;
        c.features.default_synths = 0;
        c.max_oscs = 200;
        amy_start(c);

        // Load patch on synth 1
        amy_event e = amy_default_event();
        e.synth = 1;
        e.patch_number = p;
        e.num_voices = 8;
        amy_add_event(&e);
        amy_execute_deltas();

        // 1. Check Idle Floor (5 blocks before note)
        int32_t max_idle = 0;
        for (int b = 0; b < 5; ++b) {
            amy_render(0, AMY_OSCS, 0);
            int16_t *buf = amy_fill_buffer();
            for (int i = 0; i < AMY_BLOCK_SIZE * AMY_NCHANS; ++i) {
                if (abs(buf[i]) > max_idle) max_idle = abs(buf[i]);
            }
        }

        // 2. Play Note On (MIDI 60)
        amy_event on = amy_default_event();
        on.synth = 1;
        on.midi_note = 60.0f;
        on.velocity = 0.85f;
        amy_add_event(&on);
        amy_execute_deltas();

        int32_t max_note_peak = 0;
        int clip_samples = 0;
        float note_energy = 0;
        int total_note_samples = 0;

        for (int b = 0; b < 15; ++b) {
            amy_render(0, AMY_OSCS, 0);
            int16_t *buf = amy_fill_buffer();
            for (int i = 0; i < AMY_BLOCK_SIZE * AMY_NCHANS; ++i) {
                int16_t s = buf[i];
                if (abs(s) > max_note_peak) max_note_peak = abs(s);
                if (abs(s) >= 32766) clip_samples++;
                note_energy += (float)s * (float)s;
                total_note_samples++;
            }
        }
        float note_rms = sqrtf(note_energy / total_note_samples);

        // 3. Note Off
        amy_event off = amy_default_event();
        off.synth = 1;
        off.midi_note = 60.0f;
        off.velocity = 0.0f;
        amy_add_event(&off);
        amy_execute_deltas();

        // Wait 1.5s (280 blocks)
        for (int b = 0; b < 280; ++b) {
            amy_render(0, AMY_OSCS, 0);
            amy_fill_buffer();
        }

        // 4. Measure Post-Release Noise Floor
        int32_t max_post_release = 0;
        for (int b = 0; b < 10; ++b) {
            amy_render(0, AMY_OSCS, 0);
            int16_t *buf = amy_fill_buffer();
            for (int i = 0; i < AMY_BLOCK_SIZE * AMY_NCHANS; ++i) {
                if (abs(buf[i]) > max_post_release) max_post_release = abs(buf[i]);
            }
        }

        if (max_idle > 0 || clip_samples > 100 || max_note_peak == 0) {
            printf("  [ANOMALY] Patch #%3d: Note Peak=%5d, RMS=%6.1f, Clips=%3d, Idle=%d, PostRel=%d\n",
                   p, max_note_peak, note_rms, clip_samples, max_idle, max_post_release);
            anomaly_count++;
        }

        amy_stop();
    }

    printf("\n=== Audit Finished: %d anomalies found ===\n", anomaly_count);
    return 0;
}
