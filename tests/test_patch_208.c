#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
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
    amy_config_t c = amy_default_config();
    c.features.startup_bleep = 0;
    c.features.default_synths = 1;
    c.max_oscs = 200;
    amy_start(c);

    printf("=== Inspecting Patch 208 ===\n");
    amy_event e = amy_default_event();
    e.synth = 1;
    e.patch_number = 208;
    e.num_voices = 1;
    amy_add_event(&e);
    amy_execute_deltas();

    // Check idle rendering
    int idle_nonzero = 0;
    for (int i = 0; i < 50; ++i) {
        amy_render(0, AMY_OSCS, 0);
        int16_t *buf = amy_fill_buffer();
        for (int s = 0; s < AMY_BLOCK_SIZE * AMY_NCHANS; ++s) {
            if (abs(buf[s]) > 10) idle_nonzero++;
        }
    }
    printf("Idle non-zero samples before Note-On: %d\n", idle_nonzero);

    // Note-On: note 60, vel 0.8
    e = amy_default_event();
    e.synth = 1;
    e.midi_note = 60.0f;
    e.velocity = 0.8f;
    amy_add_event(&e);
    amy_execute_deltas();

    // Render for 4 seconds (approx 750 blocks)
    float max_val = 0;
    int clipped = 0;
    for (int b = 0; b < 400; ++b) {
        amy_render(0, AMY_OSCS, 0);
        int16_t *buf = amy_fill_buffer();
        int b_max = 0;
        for (int s = 0; s < AMY_BLOCK_SIZE * AMY_NCHANS; ++s) {
            int val = abs(buf[s]);
            if (val > b_max) b_max = val;
            if (val > max_val) max_val = val;
            if (val >= 32760) clipped++;
        }
        if (b % 30 == 0) {
            printf("Block %3d (time %5.2fs): max sample = %6d, sample[0]=%6d, sample[1]=%6d\n", 
                   b, (float)b * 256.0f / 48000.0f, b_max, buf[0], buf[1]);
        }
    }
    printf("Note played: Max peak = %.0f, Clipped samples = %d\n", max_val, clipped);

    // Note-Off
    e = amy_default_event();
    e.synth = 1;
    e.midi_note = 60.0f;
    e.velocity = 0.0f;
    amy_add_event(&e);
    amy_execute_deltas();

    // Render 400 blocks post release
    int post_release_nonzero = 0;
    for (int b = 0; b < 400; ++b) {
        amy_render(0, AMY_OSCS, 0);
        int16_t *buf = amy_fill_buffer();
        int b_max = 0;
        for (int s = 0; s < AMY_BLOCK_SIZE * AMY_NCHANS; ++s) {
            int val = abs(buf[s]);
            if (val > b_max) b_max = val;
            if (val > 20) post_release_nonzero++;
        }
        if (b % 40 == 0) {
            printf("Post Note-Off Block %3d (time %5.2fs): max sample = %6d\n", 
                   b, (float)b * 256.0f / 48000.0f, b_max);
        }
    }
    printf("Post-release non-zero samples: %d\n", post_release_nonzero);

    amy_stop();
    return 0;
}
