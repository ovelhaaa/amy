#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
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

    // Render up to block 90
    for (int b = 0; b <= 90; ++b) {
        amy_render(0, AMY_OSCS, 0);
        int16_t *buf = amy_fill_buffer();
        if (b == 90) {
            printf("Samples at block 90 (first 32 stereo pairs):\n");
            for (int s = 0; s < 32; ++s) {
                printf("%6d ", buf[s*2]);
                if ((s+1)%8 == 0) printf("\n");
            }
        }
    }

    amy_stop();
    return 0;
}
