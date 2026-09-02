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
    amy_config_t c = amy_default_config();
    c.features.startup_bleep = 0;
    c.features.default_synths = 0;
    c.max_oscs = 200;
    amy_start(c);

    // Load DX7 patch 128 on synth 1
    amy_event e = amy_default_event();
    e.synth = 1;
    e.patch_number = 128;
    e.num_voices = 8;
    amy_add_event(&e);
    amy_execute_deltas();

    printf("=== Inspecting Osc States for Patch 128 ===\n");
    for (int i = 0; i < AMY_OSCS; ++i) {
        if (synth[i] != NULL && synth[i]->status != SYNTH_OFF) {
            printf("Osc %3d: wave=%2d status=%d role=%d amp_const=%.3f amp_eg0=%.3f amp_mod=%.3f eg0_times[0]=%u eg0_vals[0]=%.3f bp_r=%d note_on=%u note_off=%u\n",
                   i, synth[i]->wave, synth[i]->status, synth[i]->role,
                   synth[i]->amp_coefs[COEF_CONST], synth[i]->amp_coefs[COEF_EG0], synth[i]->amp_coefs[COEF_MOD],
                   synth[i]->breakpoint_times[0][0], synth[i]->breakpoint_values[0][0],
                   synth[i]->max_num_breakpoints[0],
                   synth[i]->note_on_clock, synth[i]->note_off_clock);
        }
    }

    // Render 1 block
    amy_render(0, AMY_OSCS, 0);
    int16_t *buf = amy_fill_buffer();
    int32_t max_val = 0;
    for (int i = 0; i < AMY_BLOCK_SIZE * AMY_NCHANS; ++i) {
        if (abs(buf[i]) > max_val) max_val = abs(buf[i]);
    }
    printf("Render 1 Block (in silence) Peak: %d\n", max_val);

    amy_stop();
    return 0;
}
