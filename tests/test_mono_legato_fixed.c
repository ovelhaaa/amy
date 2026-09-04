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

// Monophonic Legato State Machine
#define MONO_STACK_CAP 16
static uint8_t s_mono_stack[MONO_STACK_CAP];
static uint8_t s_mono_stack_size = 0;

void mono_note_on(uint8_t note, uint8_t vel, uint8_t synth_id) {
    // Remove if already present
    int found = -1;
    for (int i = 0; i < s_mono_stack_size; ++i) {
        if (s_mono_stack[i] == note) { found = i; break; }
    }
    if (found >= 0) {
        for (int i = found; i < s_mono_stack_size - 1; ++i)
            s_mono_stack[i] = s_mono_stack[i+1];
        s_mono_stack_size--;
    }
    if (s_mono_stack_size < MONO_STACK_CAP) {
        s_mono_stack[s_mono_stack_size++] = note;
    }

    amy_event e = amy_default_event();
    e.synth = synth_id;
    e.midi_note = (float)note;

    if (s_mono_stack_size == 1) {
        // Initial Note-On: trigger envelope
        e.velocity = (float)vel / 127.0f;
    } else {
        // Legato Note-On: pitch glide WITHOUT re-triggering attack envelope!
        // Do not set velocity so note_on_clock is not reset.
    }
    amy_add_event(&e);
    amy_execute_deltas();
}

void mono_note_off(uint8_t note, uint8_t synth_id) {
    int found = -1;
    for (int i = 0; i < s_mono_stack_size; ++i) {
        if (s_mono_stack[i] == note) { found = i; break; }
    }
    if (found < 0) return; // Note wasn't in stack

    bool was_top = (found == s_mono_stack_size - 1);

    for (int i = found; i < s_mono_stack_size - 1; ++i) {
        s_mono_stack[i] = s_mono_stack[i+1];
    }
    s_mono_stack_size--;

    if (s_mono_stack_size == 0) {
        // All keys released: send Note-Off
        amy_event e = amy_default_event();
        e.synth = synth_id;
        e.midi_note = (float)note;
        e.velocity = 0.0f;
        amy_add_event(&e);
        amy_execute_deltas();
    } else if (was_top) {
        // The released note was the active one, but previous notes are still held!
        // Fall back to the previous held note (legato return)!
        uint8_t prev_note = s_mono_stack[s_mono_stack_size - 1];
        amy_event e = amy_default_event();
        e.synth = synth_id;
        e.midi_note = (float)prev_note;
        // Pitch glide back without retriggering
        amy_add_event(&e);
        amy_execute_deltas();
    }
    // If was not top, the held note remains sounding uninterrupted!
}

int main(void) {
    amy_config_t c = amy_default_config();
    c.features.startup_bleep = 0;
    c.features.default_synths = 1;
    c.max_oscs = 200;
    amy_start(c);

    printf("=== Testing Fixed Monophonic Legato Logic ===\n");
    amy_event e = amy_default_event();
    e.synth = 1;
    e.patch_number = 142; // DX7 Bass
    e.num_voices = 1;
    e.portamento_ms = 60;
    amy_add_event(&e);
    amy_execute_deltas();

    // 1. Play Note 48
    printf("\n1. Press Note 48...\n");
    mono_note_on(48, 100, 1);
    for (int b = 0; b < 5; ++b) printf("   Block %d RMS = %6.0f\n", b, get_block_rms());

    // 2. Play Note 50 (Legato while 48 held)
    printf("\n2. Press Note 50 (Legato)...\n");
    mono_note_on(50, 100, 1);
    for (int b = 0; b < 5; ++b) printf("   Block %d RMS = %6.0f\n", b, get_block_rms());

    // 3. Release Note 48 (First key released, 50 still held)
    printf("\n3. Release Note 48 (50 still held)...\n");
    mono_note_off(48, 1);
    for (int b = 0; b < 10; ++b) printf("   Block %d RMS = %6.0f\n", b, get_block_rms());

    // 4. Release Note 50 (All keys released)
    printf("\n4. Release Note 50 (All released)...\n");
    mono_note_off(50, 1);
    for (int b = 0; b < 10; ++b) printf("   Block %d RMS = %6.0f\n", b, get_block_rms());

    // TEST B: Legato fallback
    printf("\n--- Test B: Fallback to held note when newer note is released ---\n");
    printf("1. Press Note 48...\n");
    mono_note_on(48, 100, 1);
    for (int b = 0; b < 3; ++b) printf("   Block %d RMS = %6.0f\n", b, get_block_rms());

    printf("2. Press Note 52 (trill / legato jump)...\n");
    mono_note_on(52, 100, 1);
    for (int b = 0; b < 3; ++b) printf("   Block %d RMS = %6.0f\n", b, get_block_rms());

    printf("3. Release Note 52 (while 48 still held -> falls back to 48!)...\n");
    mono_note_off(52, 1);
    for (int b = 0; b < 5; ++b) printf("   Block %d RMS = %6.0f\n", b, get_block_rms());

    printf("4. Release Note 48 (final release)...\n");
    mono_note_off(48, 1);
    for (int b = 0; b < 5; ++b) printf("   Block %d RMS = %6.0f\n", b, get_block_rms());

    amy_stop();
    return 0;
}
