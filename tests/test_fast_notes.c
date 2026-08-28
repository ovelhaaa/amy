#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "amy.h"

void delay_ms(uint32_t ms) { (void)ms; }

int main(void) {
    printf("== Running Fast Phrases and Repeated Notes Test ==\n");
    fflush(stdout);

    amy_config_t c = amy_default_config();
    c.features.startup_bleep = 0;
    c.features.default_synths = 1;
    amy_start(c);

    // Load Juno patch 0 with 8 voices on synth 1
    amy_event e = amy_default_event();
    e.synth = 1;
    e.patch_number = 0;
    e.num_voices = 8;
    amy_add_event(&e);
    amy_execute_deltas();

    // Test 1: Rapid repeated notes on the same pitch (Note 60)
    int successful_repeats = 0;
    for (int rep = 0; rep < 20; ++rep) {
        // Note On
        amy_event on = amy_default_event();
        on.synth = 1;
        on.midi_note = 60.0f;
        on.velocity = 0.9f;
        amy_add_event(&on);
        amy_execute_deltas();

        // Render 1 block (2.6 ms)
        amy_render(0, AMY_OSCS, 0);
        int16_t *buf = amy_fill_buffer();
        assert(buf != NULL);

        // Check that audio is sounding
        int32_t sum = 0;
        for (int i = 0; i < AMY_BLOCK_SIZE * AMY_NCHANS; ++i) {
            sum += abs(buf[i]);
        }
        if (sum > 100) {
            successful_repeats++;
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

    printf("Rapid note repetitions sounded: %d / 20\n", successful_repeats);
    assert(successful_repeats == 20);
    printf("  ok   Rapid note repetition on same pitch sounds 100%% of the time\n");
    fflush(stdout);

    // Test 2: Fast 16th-note arpeggio phrase across 4 octaves
    int sounded_notes = 0;
    for (int note = 48; note <= 84; ++note) {
        amy_event on = amy_default_event();
        on.synth = 1;
        on.midi_note = (float)note;
        on.velocity = 0.8f;
        amy_add_event(&on);
        amy_execute_deltas();

        amy_render(0, AMY_OSCS, 0);
        int16_t *buf = amy_fill_buffer();
        int32_t sum = 0;
        for (int i = 0; i < AMY_BLOCK_SIZE * AMY_NCHANS; ++i) {
            sum += abs(buf[i]);
        }
        if (sum > 100) {
            sounded_notes++;
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

    printf("Fast arpeggio notes sounded: %d / 37\n", sounded_notes);
    assert(sounded_notes == 37);
    printf("  ok   Fast chromatic phrase across all octaves sounded 100%% cleanly\n");
    fflush(stdout);

    amy_stop();
    printf("All fast phrase and note repetition tests passed successfully!\n");
    return 0;
}
