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

void test_depth(float depth) {
    amy_reset_oscs();
    char msg[512];
    snprintf(msg, sizeof(msg),
        "v2a0.840896,0,0,1,0,%gP0.25A0,0.000188,3,1,0,0.771105,12,0.125,75000,0.000188L1I2Zv3a0.594604,0,0,1,0,0P0.25A0,0.000188,114,1,9000,0.458502,1676,0.840896,59,0.000188L1I1Zv4a0.917004,0,0,1,0,0P0.25A0,0.000188,22,1,0,0.771105,10,0.176777,79000,0.000188L1I2.0175Zv5a2,0,0,1,0,0P0.25A0,0.000188,91,1,9000,0.458502,1676,0.840896,59,0.000188L1I1.01Zv6a0.917004,0,0,1,0,0P0.25A0,0.000188,12,1,0,0.771105,10,0.176777,79000,0.000188L1I1.9825Zv7a2,0,0,1,0,0P0.25A0,0.000188,81,1,9000,0.458502,1676,0.840896,59,0.000188L1I1Zv1w0a1f6.166667P0.25Zv0w8a1,0,1,0,0,0f0,1,0,1,0,0.019192b0.04A0,1,0,1,0,1,0,1,12000,1L1O2,3,4,5,6,7o6Z",
        depth);
    amy_play_message(msg);

    amy_event e = amy_default_event();
    e.osc = 0;
    e.midi_note = 60.0f;
    e.velocity = 0.8f;
    amy_add_event(&e);
    amy_execute_deltas();

    float max_peak = 0;
    float max_hf_ratio = 0;
    for (int b = 0; b < 200; ++b) {
        amy_render(0, AMY_OSCS, 0);
        int16_t *buf = amy_fill_buffer();
        float hf_energy = 0;
        float total_energy = 0;
        for (int s = 1; s < AMY_BLOCK_SIZE; ++s) {
            float diff = (float)(buf[s*2] - buf[(s-1)*2]);
            hf_energy += diff * diff;
            float val = fabsf((float)buf[s*2]);
            if (val > max_peak) max_peak = val;
            total_energy += val * val;
        }
        hf_energy /= AMY_BLOCK_SIZE;
        total_energy /= AMY_BLOCK_SIZE;
        float ratio = total_energy > 500 ? (hf_energy / total_energy) : 0;
        if (ratio > max_hf_ratio) max_hf_ratio = ratio;
    }
    printf("LFO Depth %-6g -> Peak = %6.0f, Max HF Ratio = %.3f\n", depth, max_peak, max_hf_ratio);
}

int main(void) {
    amy_config_t c = amy_default_config();
    c.features.startup_bleep = 0;
    c.features.default_synths = 1;
    c.max_oscs = 200;
    amy_start(c);

    test_depth(1.0f);   // Original
    test_depth(0.5f);
    test_depth(0.2f);
    test_depth(0.1f);
    test_depth(0.05f);
    test_depth(0.02f);
    test_depth(0.01f);
    test_depth(0.0f);   // Disabled

    amy_stop();
    return 0;
}
