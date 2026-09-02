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
    printf("=== Testing AMY sequential patch loading from 0 to 255 ===\n");
    amy_config_t c = amy_default_config();
    c.features.startup_bleep = 0;
    c.features.default_synths = 1;
    c.max_oscs = 200;
    amy_start(c);

    for (int p = 0; p < 256; ++p) {
        amy_event e = amy_default_event();
        e.synth = 1;
        e.patch_number = p;
        e.num_voices = 8;
        amy_add_event(&e);
        amy_execute_deltas();
        amy_render(0, AMY_OSCS, 0);
    }
    printf("SUCCESS: Loaded and rendered all 256 patches sequentially with 0 crashes!\n");
    amy_stop();
    return 0;
}
