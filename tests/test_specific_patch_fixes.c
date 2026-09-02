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

void test_patch(int p, int poly_notes) {
    amy_config_t c = amy_default_config();
    c.features.startup_bleep = 0;
    c.features.default_synths = 0;
    c.max_oscs = 200;
    amy_start(c);

    amy_event e = amy_default_event();
    e.synth = 1;
    e.patch_number = p;
    e.num_voices = 8;
    amy_add_event(&e);
    amy_execute_deltas();

    for (int n = 0; n < poly_notes; ++n) {
        amy_event on = amy_default_event();
        on.synth = 1;
        on.midi_note = 60.0f + n * 4.0f;
        on.velocity = 0.85f;
        amy_add_event(&on);
    }
    amy_execute_deltas();

    int32_t peak = 0;
    int clips = 0;
    for (int b = 0; b < 25; ++b) {
        amy_render(0, AMY_OSCS, 0);
        int16_t *buf = amy_fill_buffer();
        for (int i = 0; i < AMY_BLOCK_SIZE * AMY_NCHANS; ++i) {
            if (abs(buf[i]) > peak) peak = abs(buf[i]);
            if (abs(buf[i]) >= 32766) clips++;
        }
    }
    printf("Patch #%3d (Poly=%d): Peak = %5d, Clips = %4d %s\n",
           p, poly_notes, peak, clips, (clips > 0 ? "[CLIPPING]" : (peak == 0 ? "[SILENT]" : "[OK]")));
    amy_stop();
}

int main(void) {
    printf("=== Testing Specific Patches Before Fixes ===\n");
    int list[] = {42, 62, 74, 78, 84, 86, 114, 116, 124, 158};
    for (int i = 0; i < 10; ++i) {
        test_patch(list[i], (list[i] == 42 ? 3 : 1));
    }
    return 0;
}
