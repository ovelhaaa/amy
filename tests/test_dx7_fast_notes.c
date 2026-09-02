#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
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
    printf("====================================================\n");
    printf("=== Running DX7 Fast Phrases & Voice Stress Test ===\n");
    printf("====================================================\n");

    amy_config_t c = amy_default_config();
    c.features.startup_bleep = 0;
    c.features.default_synths = 1;
    c.max_oscs = 200;
    amy_start(c);

    // Test DX7 Patch 128 (DX7 Brass 1) and Patch 138 (DX7 E.Piano 1)
    int test_patches[] = { 128, 135, 138, 142 };
    int num_test_patches = sizeof(test_patches) / sizeof(test_patches[0]);

    for (int p = 0; p < num_test_patches; ++p) {
        int patch_id = test_patches[p];
        printf("\n[TEST] Testing DX7 Patch #%d...\n", patch_id);

        // Load DX7 preset with 8 voices on synth 1
        amy_event e = amy_default_event();
        e.synth = 1;
        e.patch_number = patch_id;
        e.num_voices = 8;
        amy_add_event(&e);
        amy_execute_deltas();

        // 1. Rapid repeated notes on same pitch (Note 60)
        int successful_repeats = 0;
        for (int rep = 0; rep < 30; ++rep) {
            // Note On
            amy_event on = amy_default_event();
            on.synth = 1;
            on.midi_note = 60.0f;
            on.velocity = 0.9f;
            amy_add_event(&on);
            amy_execute_deltas();

            // Render 1 block (5.33 ms @ 48kHz, 256 samples)
            amy_render(0, AMY_OSCS, 0);
            int16_t *buf = amy_fill_buffer();
            assert(buf != NULL);

            int32_t sum = 0;
            for (int i = 0; i < AMY_BLOCK_SIZE * AMY_NCHANS; ++i) {
                sum += abs(buf[i]);
            }
            if (sum > 50) {
                successful_repeats++;
            } else {
                printf("  [WARN] Repetition %d silent! (sum=%d)\n", rep, sum);
            }

            // Note Off
            amy_event off = amy_default_event();
            off.synth = 1;
            off.midi_note = 60.0f;
            off.velocity = 0.0f;
            amy_add_event(&off);
            amy_execute_deltas();

            // Render 1 block of release
            amy_render(0, AMY_OSCS, 0);
            amy_fill_buffer();
        }
        printf("  -> Rapid single-note repetitions: %d / 30 sounded\n", successful_repeats);

        // 2. Fast chromatic run across 3 octaves (Notes 48 to 84)
        int sounded_chromatic = 0;
        for (int note = 48; note <= 84; ++note) {
            amy_event on = amy_default_event();
            on.synth = 1;
            on.midi_note = (float)note;
            on.velocity = 0.85f;
            amy_add_event(&on);
            amy_execute_deltas();

            amy_render(0, AMY_OSCS, 0);
            int16_t *buf = amy_fill_buffer();
            int32_t sum = 0;
            for (int i = 0; i < AMY_BLOCK_SIZE * AMY_NCHANS; ++i) {
                sum += abs(buf[i]);
            }
            if (sum > 50) {
                sounded_chromatic++;
            } else {
                printf("  [WARN] Chromatic note %d silent! (sum=%d)\n", note, sum);
            }

            amy_event off = amy_default_event();
            off.synth = 1;
            off.midi_note = (float)note;
            off.velocity = 0.0f;
            amy_add_event(&off);
            amy_execute_deltas();
            amy_render(0, AMY_OSCS, 0);
            amy_fill_buffer();
        }
        printf("  -> Fast chromatic phrase: %d / 37 sounded\n", sounded_chromatic);

        // 3. Rapid polyphonic chords (4-note chords repeated with voice stealing)
        int sounded_chords = 0;
        for (int ch = 0; ch < 20; ++ch) {
            int base_note = 48 + (ch % 7) * 2;
            int chord_notes[4] = { base_note, base_note + 4, base_note + 7, base_note + 11 };

            for (int k = 0; k < 4; ++k) {
                amy_event on = amy_default_event();
                on.synth = 1;
                on.midi_note = (float)chord_notes[k];
                on.velocity = 0.8f;
                amy_add_event(&on);
            }
            amy_execute_deltas();

            amy_render(0, AMY_OSCS, 0);
            int16_t *buf = amy_fill_buffer();
            int32_t sum = 0;
            for (int i = 0; i < AMY_BLOCK_SIZE * AMY_NCHANS; ++i) {
                sum += abs(buf[i]);
            }
            if (sum > 100) {
                sounded_chords++;
            } else {
                printf("  [WARN] Chord %d silent! (sum=%d)\n", ch, sum);
            }

            // Note off
            for (int k = 0; k < 4; ++k) {
                amy_event off = amy_default_event();
                off.synth = 1;
                off.midi_note = (float)chord_notes[k];
                off.velocity = 0.0f;
                amy_add_event(&off);
            }
            amy_execute_deltas();
            amy_render(0, AMY_OSCS, 0);
            amy_fill_buffer();
        }
        printf("  -> Fast polyphonic chords: %d / 20 sounded\n", sounded_chords);
    }

    amy_stop();
    printf("\n=== DX7 Fast Notes Test Finished ===\n");
    return 0;
}
