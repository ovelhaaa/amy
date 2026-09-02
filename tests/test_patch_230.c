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
    printf("=== Testing Patch 230 (Syn-Clav 3) Audio ===\n");

    amy_config_t c = amy_default_config();
    c.features.startup_bleep = 0;
    c.features.default_synths = 0;
    c.max_oscs = 200;
    amy_start(c);

    // Load Patch 230
    amy_event e = amy_default_event();
    e.synth = 1;
    e.patch_number = 230;
    e.num_voices = 8;
    amy_add_event(&e);
    amy_execute_deltas();

    // Play Note 60
    amy_event on = amy_default_event();
    on.synth = 1;
    on.midi_note = 60.0f;
    on.velocity = 0.8f;
    amy_add_event(&on);
    amy_execute_deltas();

    printf("Rendering 10 blocks during Note On...\n");
    for (int b = 0; b < 10; ++b) {
        amy_render(0, AMY_OSCS, 0);
        int16_t *buf = amy_fill_buffer();
        int16_t min_s = 32767, max_s = -32768;
        float energy = 0;
        for (int i = 0; i < AMY_BLOCK_SIZE * AMY_NCHANS; ++i) {
            if (buf[i] < min_s) min_s = buf[i];
            if (buf[i] > max_s) max_s = buf[i];
            energy += (float)buf[i] * (float)buf[i];
        }
        float rms = sqrtf(energy / (AMY_BLOCK_SIZE * AMY_NCHANS));
        printf("  Block %2d: Min=%6d, Max=%6d, RMS=%.1f\n", b, min_s, max_s, rms);
    }

    amy_stop();
    return 0;
}
