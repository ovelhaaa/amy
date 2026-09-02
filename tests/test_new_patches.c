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

void test_wire(const char* name, const char* wire, int poly_notes) {
    amy_config_t c = amy_default_config();
    c.features.startup_bleep = 0;
    c.features.default_synths = 0;
    c.max_oscs = 200;
    amy_start(c);

    // Setup patch directly via amy_play_message
    amy_play_message((char*)wire);
    amy_execute_deltas();

    for (int n = 0; n < poly_notes; ++n) {
        amy_event on = amy_default_event();
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
    printf("  %-28s: Peak = %5d, Clips = %4d %s\n",
           name, peak, clips, (clips > 0 ? "[CLIPPING]" : (peak == 0 ? "[SILENT]" : "[OK]")));
    amy_stop();
}

int main(void) {
    printf("=== Testing New Clean Patch Formulations ===\n");
    test_wire("Patch 042 (Juno Frontier Org)", "v1w4a1,,0,1Zv0w20c2L1G4Zv2w1c3L1Zv3w3c4L1Zv4w1c5L1Zv5w5L1Zv1f5.263A22,1.0,10000,0Zv2a1,,0,0f440,1,,,,0.001,1d0.5,,,,,0.335m0Zv3a0,,0,0f440,1,,,,0.001,1m0Zv4a0.228,,0,0f220,1,,,,0.001,1m0Zv5a0,,0,0Zv0F1427.9,0.409,,0,0,0R2.276Zv0a0,,1,1,0A5,0.45,0,0.45,0,0B6,1,0,1,0,0Zx7,-3,-3k0Z", 4);
    test_wire("Patch 062 (Juno FX Sweep)", "v1w4a1,,0,1Zv0w20c2L1G4Zv2w1c3L1Zv3w3c4L1Zv4w1c5L1Zv5w5L1Zv1f20.195A339,1.0,10000,0Zv2a0.4,,0,0f220,1,,,,0.027,1d0.5,,,,,0.402m0Zv3a0,,0,0f220,1,,,,0.027,1m0Zv4a0.3,,0,0f110,1,,,,0.027,1m0Zv5a0.3,,0,0Zv0F1931.3,0.74,,-7.189,0,0R1.829Zv0a0,,1,1,0A6,0.6,20258,0,11687,0Zx0,0,0k0Z", 1);
    test_wire("Patch 074 (Juno Orch Pad)", "v1w4a1,,0,1Zv0w20c2L1G4Zv2w1c3L1Zv3w3c4L1Zv4w1c5L1Zv5w5L1Zv1f1.004A128,1.0,10000,0Zv2a1,,0,0f440,1,,,,0,1d0.5,,,,,0.413m0Zv3a1,,0,0f440,1,,,,0,1m0Zv4a1,,0,0f220,1,,,,0,1m0Zv5a0,,0,0Zv0F1800,0.283,,2.5,0,0R0.7Zv0a0,,1,1,0A238,0.5,14202,0.35,685,0Zx7,-3,-3k1,,0.83,0.5Z", 1);
    test_wire("Patch 078 (Juno FX Rise 1)", "v1w4a1,,0,1Zv0w20c2L1G4Zv2w1c3L1Zv3w3c4L1Zv4w1c5L1Zv5w5L1Zv1f14.86A5,1.0,10000,0Zv2a0.5,,0,0f440,1,,,,0,1d0.5,,,,,0.3m0Zv3a0.4,,0,0f440,1,,,,0,1m0Zv4a0.3,,0,0f220,1,,,,0,1m0Zv5a0,,0,0Zv0F800,0.504,,3.5,0,0.5R2.5Zv0a0,,1,1,0A10,0.5,0,0.5,3195,0Zx7,-3,-3k1,,0.5,0.5Z", 1);
    test_wire("Patch 084 (Juno Tomita)", "v1w4a1,,0,1Zv0w20c2L1G4Zv2w1c3L1Zv3w3c4L1Zv4w1c5L1Zv5w5L1Zv1f5.116A5,1.0,10000,0Zv2a0.6,,0,0f440,1,,,,0,1d0.7,,,,,0.2m0Zv3a0.4,,0,0f440,1,,,,0,1m0Zv4a0.3,,0,0f220,1,,,,0,1m0Zv5a0,,0,0Zv0F1200,0.6,,0,2.5,0.02R3.5Zv0a0,,1,1,0A10,0.5,102,0.35,300,0Zx7,-3,-3k0Z", 1);
    test_wire("Patch 086 (Juno Sharp Reed)", "v1w4a1,,0,1Zv0w20c2L1G4Zv2w1c3L1Zv3w3c4L1Zv4w1c5L1Zv5w5L1Zv1f0.974A5,1.0,10000,0Zv2a0.5,,0,0f880,1,,,,0,1d0.5,,,,,0.287m0Zv3a0.5,,0,0f880,1,,,,0,1m0Zv4a0,,0,0f440,1,,,,0,1m0Zv5a0,,0,0Zv0F1400,0.512,,0,2.5,0R0.7Zv0a0,,1,1,0A5,0.5,0,0.5,50,0B22,1,151,0.882,0,0Zx7,-3,-3k0Z", 1);
    test_wire("Patch 114 (Juno Meow Brass)", "v1w4a1,,0,1Zv0w20c2L1G4Zv2w1c3L1Zv3w3c4L1Zv4w1c5L1Zv5w5L1Zv1f2.367A5991,1.0,10000,0Zv2a0.5,,0,0f440,1,,,,0,1d0.5,,,,,0.287m0Zv3a0.5,,0,0f440,1,,,,0,1m0Zv4a0,,0,0Zv5a0,,0,0Zv0F450,0.512,,0,3.031,0R3.0Zv0a0,,1,1,0A10,0.5,150,0.4,170,0B38,1,15988,0,170,0Zx7,-3,-3k0Z", 1);
    test_wire("Patch 116 (Juno High Bells)", "v1w4a1,,0,1Zv0w20c2L1G4Zv2w1c3L1Zv3w3c4L1Zv4w1c5L1Zv5w5L1Zv1f1.278A128,1.0,10000,0Zv2a0.6,,0,0f880,1,,,,0.03,1d0.5,,,,,0.3m0Zv3a0,,0,0Zv4a0.4,,0,0f440,1,,,,0.03,1m0Zv5a0,,0,0Zv0F3500,0.8,,0,0,0R2.0Zv0a0,,1,1,0A5,0.5,212,0.1,600,0Zx-15,8,8k0Z", 1);
    test_wire("Patch 124 (Juno Rocket Men)", "v1w4a1,,0,1Zv0w20c2L1G4Zv2w1c3L1Zv3w3c4L1Zv4w1c5L1Zv5w5L1Zv1f11.884A5,1.0,10000,0Zv2a0.4,,0,0f440,1,,,,0,1d0.5,,,,,0.3m0Zv3a0,,0,0Zv4a0.3,,0,0f220,1,,,,0,1m0Zv5a0.3,,0,0Zv0F2500,0.8,,0,2.5,0.62R3.0Zv0a0,,1,1,0A15,0.45,142057,0.402,4035,0Zx7,-3,-3k1,,0.83,0.5Z", 1);
    test_wire("Patch 158 (DX7 Train)", "v2a0.35,0,0,1,0,0P0.25A0,0.917004,4,1,0,1,0,1,4,0.917004L1I5Zv3a0.3,0,0,1,0,1P0.25A0,1,0,1,0,1,0,1,262,1L1I9.03375Zv4a0.2,0,0,1,0,0f971.627952P0.25A0,0.000188,0,1,6709,0.000188,0,0.000188,5078,0.000188L1Zv5a0.8,0,0,1,0,0f373.680125P0.25A0,0.000188,0,1,6709,0.000188,0,0.000188,2594,0.000188L1Zv6a0.1,0,0,1,0,0P0.25A0,0.000188,324,1,13843,0.037163,401,0.057313,32,0.000188L1I3.03Zv7a0.8,0,0,1,0,0P0.25A0,0.000188,16,1,1647,0.297302,0,0.297302,234,0.000188L1I1.64Zv1w4a1f6.5P0.25Zv0w8a1,0,1,0,0,0f0,1,0,1,0,0b0.05A0,1,0,1,0,1,0,1,731,1L1O2,3,4,5,6,7o5Z", 1);
    return 0;
}
