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

    printf("=== Testing Drum Pitch Bend Isolation ===\n");
    // Default synths loads synth 10 as drums (channel 9, patch 258)
    // 1. Play drum note 36 (bass drum) with 0 pitch bend
    amy_event e = amy_default_event();
    e.synth = 10;
    e.midi_note = 36.0f;
    e.velocity = 0.8f;
    amy_add_event(&e);
    amy_execute_deltas();

    int16_t drum_samples_normal[256];
    amy_render(0, AMY_OSCS, 0);
    int16_t *buf = amy_fill_buffer();
    memcpy(drum_samples_normal, buf, sizeof(drum_samples_normal));

    // Reset and try with extreme pitch bend (+8191 = +1/6 octave)
    amy_reset_oscs();
    e = amy_default_event();
    e.synth = 10;
    e.patch_number = 258;
    amy_add_event(&e);
    // Apply pitch bend
    e = amy_default_event();
    e.pitch_bend = 0.5f;
    amy_add_event(&e);
    // Trigger drum note 36
    e = amy_default_event();
    e.synth = 10;
    e.midi_note = 36.0f;
    e.velocity = 0.8f;
    amy_add_event(&e);
    amy_execute_deltas();

    amy_render(0, AMY_OSCS, 0);
    buf = amy_fill_buffer();

    int diff_count = 0;
    for (int i = 0; i < 256; ++i) {
        if (abs(buf[i] - drum_samples_normal[i]) > 5) diff_count++;
    }
    printf("Drum samples diff with pitch bend: %d out of 256\n", diff_count);

    amy_stop();
    return 0;
}
