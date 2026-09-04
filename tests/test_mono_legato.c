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

float get_block_rms(void) {
    amy_render(0, AMY_OSCS, 0);
    int16_t *buf = amy_fill_buffer();
    float total = 0;
    for (int s = 0; s < AMY_BLOCK_SIZE; ++s) {
        float val = (float)buf[s*2];
        total += val * val;
    }
    return sqrtf(total / AMY_BLOCK_SIZE);
}

int main(void) {
    amy_config_t c = amy_default_config();
    c.features.startup_bleep = 0;
    c.features.default_synths = 1;
    c.max_oscs = 200;
    amy_start(c);

    printf("=== Testing Monophonic Mode (num_voices = 1) ===\n");
    // Load a preset with 1 voice (monophonic)
    amy_event e = amy_default_event();
    e.synth = 1;
    e.patch_number = 142; // DX7 Bass
    e.num_voices = 1;
    amy_add_event(&e);
    amy_execute_deltas();

    // Step 1: Trigger Note 1 (MIDI 48 = C3)
    printf("1. Triggering Note 48 (C3)...\n");
    e = amy_default_event();
    e.synth = 1;
    e.midi_note = 48.0f;
    e.velocity = 0.8f;
    amy_add_event(&e);
    amy_execute_deltas();

    for (int b = 0; b < 10; ++b) {
        printf("   Block %2d (Note 48 sounding): RMS = %6.0f\n", b, get_block_rms());
    }

    // Step 2: Trigger Note 2 (MIDI 50 = D3) WHILE Note 48 is still held! (Legato)
    printf("2. Triggering Note 50 (D3) while Note 48 is held (Legato)...\n");
    e = amy_default_event();
    e.synth = 1;
    e.midi_note = 50.0f;
    e.velocity = 0.8f;
    amy_add_event(&e);
    amy_execute_deltas();

    for (int b = 0; b < 10; ++b) {
        printf("   Block %2d (After Note 50 On): RMS = %6.0f\n", b, get_block_rms());
    }

    // Step 3: Release Note 48 (first note released)
    printf("3. Releasing Note 48 (Note 50 still held)...\n");
    e = amy_default_event();
    e.synth = 1;
    e.midi_note = 48.0f;
    e.velocity = 0.0f;
    amy_add_event(&e);
    amy_execute_deltas();

    for (int b = 0; b < 10; ++b) {
        printf("   Block %2d (After Note 48 Off): RMS = %6.0f\n", b, get_block_rms());
    }

    // Step 4: Release Note 50
    printf("4. Releasing Note 50...\n");
    e = amy_default_event();
    e.synth = 1;
    e.midi_note = 50.0f;
    e.velocity = 0.0f;
    amy_add_event(&e);
    amy_execute_deltas();

    for (int b = 0; b < 10; ++b) {
        printf("   Block %2d (After Note 50 Off): RMS = %6.0f\n", b, get_block_rms());
    }

    amy_stop();
    return 0;
}
