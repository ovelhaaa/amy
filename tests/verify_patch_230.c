#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
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

    // Load Patch 230
    amy_event e = amy_default_event();
    e.synth = 1;
    e.patch_number = 230;
    e.num_voices = 1;
    amy_add_event(&e);
    amy_execute_deltas();

    printf("=== Voice 0 Oscs for Patch 230 ===\n");
    for (int o = 0; o < 8; ++o) {
        if (synth[o] != NULL) {
            printf("Osc %d: wave=%d, amp_coefs=[%.3f, %.3f, %.3f, %.3f, %.3f, %.3f], mod_source=%d, role=%d\n",
                   o, synth[o]->wave,
                   synth[o]->amp_coefs[0], synth[o]->amp_coefs[1], synth[o]->amp_coefs[2],
                   synth[o]->amp_coefs[3], synth[o]->amp_coefs[4], synth[o]->amp_coefs[5],
                   synth[o]->mod_source, synth[o]->role);
        }
    }

    amy_stop();
    return 0;
}
