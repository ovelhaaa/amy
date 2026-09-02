#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
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
    printf("====================================================\n");
    printf("=== Testing Isolated Patches Background Noise =====\n");
    printf("====================================================\n");

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

        // 1. Measure Idle Noise (before any note is played)
        int32_t max_idle_amp = 0;
        for (int b = 0; b < 5; ++b) {
            amy_render(0, AMY_OSCS, 0);
            int16_t *buf = amy_fill_buffer();
            for (int i = 0; i < AMY_BLOCK_SIZE * AMY_NCHANS; ++i) {
                if (abs(buf[i]) > max_idle_amp) max_idle_amp = abs(buf[i]);
            }
        }

        // 2. Play Note On and Note Off
        amy_event on = amy_default_event();
        on.synth = 1;
        on.midi_note = 60.0f;
        on.velocity = 0.9f;
        amy_add_event(&on);
        amy_execute_deltas();

        // Render 10 blocks (50 ms) of note active
        int32_t max_note_amp = 0;
        for (int b = 0; b < 10; ++b) {
            amy_render(0, AMY_OSCS, 0);
            int16_t *buf = amy_fill_buffer();
            for (int i = 0; i < AMY_BLOCK_SIZE * AMY_NCHANS; ++i) {
                if (abs(buf[i]) > max_note_amp) max_note_amp = abs(buf[i]);
            }
        }

        // Note off
        amy_event off = amy_default_event();
        off.synth = 1;
        off.midi_note = 60.0f;
        off.velocity = 0.0f;
        amy_add_event(&off);
        amy_execute_deltas();

        // Render 200 blocks (1.0 sec) to let release finish
        for (int b = 0; b < 200; ++b) {
            amy_render(0, AMY_OSCS, 0);
            amy_fill_buffer();
        }

        // 3. Measure Post-Release Noise (5 blocks after 1s release)
        int32_t max_post_release_amp = 0;
        for (int b = 0; b < 10; ++b) {
            amy_render(0, AMY_OSCS, 0);
            int16_t *buf = amy_fill_buffer();
            for (int i = 0; i < AMY_BLOCK_SIZE * AMY_NCHANS; ++i) {
                if (abs(buf[i]) > max_post_release_amp) max_post_release_amp = abs(buf[i]);
            }
        }

        if (max_idle_amp > 10 || max_post_release_amp > 10 || max_note_amp == 0) {
            printf("  [NOISE/ANOMALY] Patch #%3d: Note Peak = %5d, Idle Peak = %5d, Post-Release Peak = %5d\n",
                   p, max_note_amp, max_idle_amp, max_post_release_amp);
        }

        amy_stop();
    }

    printf("\n=== Isolated Patch Noise Test Finished ===\n");
    return 0;
}
