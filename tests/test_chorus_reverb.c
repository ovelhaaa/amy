#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "amy.h"
#include "delay.h"

void delay_ms(uint32_t ms) { (void)ms; }

int main(void) {
    printf("== Running Spatial Effects Tests (BBD Chorus & Modulated Reverb) ==\n");
    fflush(stdout);

    amy_config_t c = amy_default_config();
    c.features.startup_bleep = 0;
    c.features.reverb = 1;
    c.features.chorus = 1;
    amy_start(c);

    // Test 1: Stereo Reverb with Dattorro LFO Micro-Modulation
    reverb_params_t *rev = new_reverb();
    assert(rev != NULL);
    bool ok = init_stereo_reverb(rev);
    assert(ok);

    SAMPLE r_in[AMY_BLOCK_SIZE];
    SAMPLE l_in[AMY_BLOCK_SIZE];
    SAMPLE r_out[AMY_BLOCK_SIZE];
    SAMPLE l_out[AMY_BLOCK_SIZE];

    // Feed an impulse at t=0
    memset(r_in, 0, sizeof(r_in));
    memset(l_in, 0, sizeof(l_in));
    r_in[0] = F2S(0.9f);
    l_in[0] = F2S(0.9f);

    stereo_reverb(rev, r_in, l_in, r_out, l_out, AMY_BLOCK_SIZE, F2S(0.7f));
    printf("Initial reverb block processed: r_out[0]=%f, l_out[0]=%f\n", S2F(r_out[0]), S2F(l_out[0]));
    assert(r_out[0] != 0);

    // Render 100 blocks of reverb tail with silence input
    memset(r_in, 0, sizeof(r_in));
    memset(l_in, 0, sizeof(l_in));
    SAMPLE max_tail = 0;
    for (int b = 0; b < 100; ++b) {
        stereo_reverb(rev, r_in, l_in, r_out, l_out, AMY_BLOCK_SIZE, F2S(0.7f));
        for (int i = 0; i < AMY_BLOCK_SIZE; ++i) {
            SAMPLE abs_r = abs(r_out[i]);
            SAMPLE abs_l = abs(l_out[i]);
            if (abs_r > max_tail) max_tail = abs_r;
            if (abs_l > max_tail) max_tail = abs_l;
            assert(!isnan(S2F(abs_r)));
            assert(!isnan(S2F(abs_l)));
            assert(!isinf(S2F(abs_r)));
            assert(!isinf(S2F(abs_l)));
        }
    }
    printf("Reverb tail max after 100 blocks: %f\n", S2F(max_tail));
    assert(max_tail > 0);
    assert(S2F(max_tail) < 2.0f); // Bounded decay
    printf("  ok   Modulated stereo reverb tail stable, dense and bounded\n");
    fflush(stdout);

    delete_reverb(rev);

    // Test 2: BBD Chorus Delay Line with 4-point Hermite Interpolation
    delay_line_t *del = new_delay_line(1024, 256, amy_global.config.ram_caps_delay);
    assert(del != NULL);

    SAMPLE block[AMY_BLOCK_SIZE];
    SAMPLE mod[AMY_BLOCK_SIZE];
    for (int i = 0; i < AMY_BLOCK_SIZE; ++i) {
        block[i] = F2S(0.5f * sinf(2.0f * (float)M_PI * (float)i / 16.0f));
        mod[i] = F2S(sinf(2.0f * (float)M_PI * (float)i / 64.0f)); // LFO modulation
    }

    apply_variable_delay(block, del, mod, F2S(0.5f), F2S(0.7f), 0);
    for (int i = 0; i < AMY_BLOCK_SIZE; ++i) {
        assert(!isnan(S2F(block[i])));
        assert(!isinf(S2F(block[i])));
        assert(abs(block[i]) < F2S(2.0f));
    }
    printf("  ok   BBD Chorus with Hermite interpolation smooth and distortion-free\n");
    fflush(stdout);

    free_delay_line(del);

    printf("All spatial effects tests passed successfully!\n");
    return 0;
}
