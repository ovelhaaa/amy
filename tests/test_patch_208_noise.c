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

    amy_event e = amy_default_event();
    e.synth = 1;
    e.patch_number = 208;
    e.num_voices = 1;
    amy_add_event(&e);
    amy_execute_deltas();

    e = amy_default_event();
    e.synth = 1;
    e.midi_note = 60.0f;
    e.velocity = 0.8f;
    amy_add_event(&e);
    amy_execute_deltas();

    // Render 1 second (187 blocks)
    for (int b = 0; b < 200; ++b) {
        amy_render(0, AMY_OSCS, 0);
        int16_t *buf = amy_fill_buffer();
        // High-pass filter or consecutive differences to measure high frequency / white noise
        float hf_energy = 0;
        float total_energy = 0;
        for (int s = 1; s < AMY_BLOCK_SIZE; ++s) {
            float diff = (float)(buf[s*2] - buf[(s-1)*2]);
            hf_energy += diff * diff;
            total_energy += (float)buf[s*2] * (float)buf[s*2];
        }
        hf_energy /= AMY_BLOCK_SIZE;
        total_energy /= AMY_BLOCK_SIZE;
        float hf_ratio = total_energy > 0 ? (hf_energy / total_energy) : 0;
        
        // Print every 10 blocks (around 53ms apart)
        if (b % 10 == 0) {
            printf("Block %3d (t=%5.3fs): RMS=%6.0f, HF_diff_RMS=%6.0f, Ratio=%.3f\n",
                   b, b * 256.0f / 48000.0f, sqrtf(total_energy), sqrtf(hf_energy), hf_ratio);
        }
    }

    amy_stop();
    return 0;
}
