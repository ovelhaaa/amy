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

float test_wirecode(const char* wirecode, const char* label) {
    amy_reset_oscs();
    amy_play_message((char*)wirecode);
    amy_event e = amy_default_event();
    e.osc = 0;
    e.midi_note = 60.0f;
    e.velocity = 0.8f;
    amy_add_event(&e);
    amy_execute_deltas();

    float max_hf_ratio = 0;
    for (int b = 0; b < 200; ++b) {
        amy_render(0, AMY_OSCS, 0);
        int16_t *buf = amy_fill_buffer();
        float hf_energy = 0;
        float total_energy = 0;
        for (int s = 1; s < AMY_BLOCK_SIZE; ++s) {
            float diff = (float)(buf[s*2] - buf[(s-1)*2]);
            hf_energy += diff * diff;
            total_energy += (float)buf[s*2] * (float)buf[s*2];
        }
        hf_energy /= AMY_BLOCK_SIZE;
        total_energy /= AMY_BLOCK_SIZE;
        float ratio = total_energy > 500 ? (hf_energy / total_energy) : 0;
        if (ratio > max_hf_ratio) max_hf_ratio = ratio;
    }
    printf("%-35s -> Max HF noise ratio: %.3f\n", label, max_hf_ratio);
    return max_hf_ratio;
}

int main(void) {
    amy_config_t c = amy_default_config();
    c.features.startup_bleep = 0;
    c.features.default_synths = 1;
    c.max_oscs = 200;
    amy_start(c);

    // Original Patch 208 wirecode
    const char* orig = "v2a0.840896,0,0,1,0,1P0.25A0,0.000188,3,1,0,0.771105,12,0.125,75000,0.000188L1I2Zv3a0.594604,0,0,1,0,0P0.25A0,0.000188,114,1,9000,0.458502,1676,0.840896,59,0.000188L1I1Zv4a0.917004,0,0,1,0,0P0.25A0,0.000188,22,1,0,0.771105,10,0.176777,79000,0.000188L1I2.0175Zv5a2,0,0,1,0,0P0.25A0,0.000188,91,1,9000,0.458502,1676,0.840896,59,0.000188L1I1.01Zv6a0.917004,0,0,1,0,0P0.25A0,0.000188,12,1,0,0.771105,10,0.176777,79000,0.000188L1I1.9825Zv7a2,0,0,1,0,0P0.25A0,0.000188,81,1,9000,0.458502,1676,0.840896,59,0.000188L1I1Zv1w0a1f6.166667P0.25Zv0w8a1,0,1,0,0,0f0,1,0,1,0,0.019192b0.04A0,1,0,1,0,1,0,1,12000,1L1O2,3,4,5,6,7o6Z";
    test_wirecode(orig, "Original Patch 208");

    // 1. Without feedback (b0)
    const char* no_fb = "v2a0.840896,0,0,1,0,1P0.25A0,0.000188,3,1,0,0.771105,12,0.125,75000,0.000188L1I2Zv3a0.594604,0,0,1,0,0P0.25A0,0.000188,114,1,9000,0.458502,1676,0.840896,59,0.000188L1I1Zv4a0.917004,0,0,1,0,0P0.25A0,0.000188,22,1,0,0.771105,10,0.176777,79000,0.000188L1I2.0175Zv5a2,0,0,1,0,0P0.25A0,0.000188,91,1,9000,0.458502,1676,0.840896,59,0.000188L1I1.01Zv6a0.917004,0,0,1,0,0P0.25A0,0.000188,12,1,0,0.771105,10,0.176777,79000,0.000188L1I1.9825Zv7a2,0,0,1,0,0P0.25A0,0.000188,81,1,9000,0.458502,1676,0.840896,59,0.000188L1I1Zv1w0a1f6.166667P0.25Zv0w8a1,0,1,0,0,0f0,1,0,1,0,0.019192b0A0,1,0,1,0,1,0,1,12000,1L1O2,3,4,5,6,7o6Z";
    test_wirecode(no_fb, "1. Without feedback (b0)");

    // 2. Without v2 LFO amp mod (v2a0.840896,0,0,1,0,0)
    const char* no_v2_lfo = "v2a0.840896,0,0,1,0,0P0.25A0,0.000188,3,1,0,0.771105,12,0.125,75000,0.000188L1I2Zv3a0.594604,0,0,1,0,0P0.25A0,0.000188,114,1,9000,0.458502,1676,0.840896,59,0.000188L1I1Zv4a0.917004,0,0,1,0,0P0.25A0,0.000188,22,1,0,0.771105,10,0.176777,79000,0.000188L1I2.0175Zv5a2,0,0,1,0,0P0.25A0,0.000188,91,1,9000,0.458502,1676,0.840896,59,0.000188L1I1.01Zv6a0.917004,0,0,1,0,0P0.25A0,0.000188,12,1,0,0.771105,10,0.176777,79000,0.000188L1I1.9825Zv7a2,0,0,1,0,0P0.25A0,0.000188,81,1,9000,0.458502,1676,0.840896,59,0.000188L1I1Zv1w0a1f6.166667P0.25Zv0w8a1,0,1,0,0,0f0,1,0,1,0,0.019192b0.04A0,1,0,1,0,1,0,1,12000,1L1O2,3,4,5,6,7o6Z";
    test_wirecode(no_v2_lfo, "2. Without v2 LFO amp mod");

    // 3. Without v0 LFO pitch mod (f0,1,0,1,0,0)
    const char* no_v0_pitch_lfo = "v2a0.840896,0,0,1,0,1P0.25A0,0.000188,3,1,0,0.771105,12,0.125,75000,0.000188L1I2Zv3a0.594604,0,0,1,0,0P0.25A0,0.000188,114,1,9000,0.458502,1676,0.840896,59,0.000188L1I1Zv4a0.917004,0,0,1,0,0P0.25A0,0.000188,22,1,0,0.771105,10,0.176777,79000,0.000188L1I2.0175Zv5a2,0,0,1,0,0P0.25A0,0.000188,91,1,9000,0.458502,1676,0.840896,59,0.000188L1I1.01Zv6a0.917004,0,0,1,0,0P0.25A0,0.000188,12,1,0,0.771105,10,0.176777,79000,0.000188L1I1.9825Zv7a2,0,0,1,0,0P0.25A0,0.000188,81,1,9000,0.458502,1676,0.840896,59,0.000188L1I1Zv1w0a1f6.166667P0.25Zv0w8a1,0,1,0,0,0f0,1,0,1,0,0b0.04A0,1,0,1,0,1,0,1,12000,1L1O2,3,4,5,6,7o6Z";
    test_wirecode(no_v0_pitch_lfo, "3. Without v0 pitch LFO");

    // 4. Without both LFO mods
    const char* no_lfo_at_all = "v2a0.840896,0,0,1,0,0P0.25A0,0.000188,3,1,0,0.771105,12,0.125,75000,0.000188L1I2Zv3a0.594604,0,0,1,0,0P0.25A0,0.000188,114,1,9000,0.458502,1676,0.840896,59,0.000188L1I1Zv4a0.917004,0,0,1,0,0P0.25A0,0.000188,22,1,0,0.771105,10,0.176777,79000,0.000188L1I2.0175Zv5a2,0,0,1,0,0P0.25A0,0.000188,91,1,9000,0.458502,1676,0.840896,59,0.000188L1I1.01Zv6a0.917004,0,0,1,0,0P0.25A0,0.000188,12,1,0,0.771105,10,0.176777,79000,0.000188L1I1.9825Zv7a2,0,0,1,0,0P0.25A0,0.000188,81,1,9000,0.458502,1676,0.840896,59,0.000188L1I1Zv1w0a1f6.166667P0.25Zv0w8a1,0,1,0,0,0f0,1,0,1,0,0b0.04A0,1,0,1,0,1,0,1,12000,1L1O2,3,4,5,6,7o6Z";
    test_wirecode(no_lfo_at_all, "4. Without both LFO mods");

    amy_stop();
    return 0;
}
