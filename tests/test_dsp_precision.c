#include "amy.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

extern const uint16_t clipping_lookup_table[];
#define FIRST_NONLIN 29491
#define FIRST_HARDCLIP 34405

void delay_ms(uint32_t ms) {
    (void)ms;
}

int main() {
    printf("== Running DSP Precision and Arithmetic Tests ==\n");

    // Test 1: Full-precision s8.23 multiplication
    // Small amplitude signal multiplication (where bit-truncation previously destroyed resolution)
    SAMPLE small_sig = F2S(0.005f); // 0.005
    SAMPLE scale = F2S(0.1f);      // 0.1
    SAMPLE expected_prod = F2S(0.005f * 0.1f); // 0.0005f

    SAMPLE res_mul0 = MUL0_SS(small_sig, scale);
    SAMPLE res_mul4 = MUL4_SS(small_sig, scale);
    SAMPLE res_mul5 = MUL5A_SS(small_sig, scale);
    SAMPLE res_mul6 = MUL6A_SS(small_sig, scale);
    SAMPLE res_mul8 = MUL8_SS(small_sig, scale);

    printf("Small signal test: expected ~ %d (%f)\n", expected_prod, S2F(expected_prod));
    printf("MUL0_SS:  %d (%f)\n", res_mul0, S2F(res_mul0));
    printf("MUL4_SS:  %d (%f)\n", res_mul4, S2F(res_mul4));
    printf("MUL5A_SS: %d (%f)\n", res_mul5, S2F(res_mul5));
    printf("MUL6A_SS: %d (%f)\n", res_mul6, S2F(res_mul6));
    printf("MUL8_SS:  %d (%f)\n", res_mul8, S2F(res_mul8));

    // Under 64-bit precision, all MULx_SS operations give the exact same mathematically accurate result:
    assert(abs(res_mul0 - expected_prod) <= 1);
    assert(abs(res_mul4 - expected_prod) <= 1);
    assert(abs(res_mul5 - expected_prod) <= 1);
    assert(abs(res_mul6 - expected_prod) <= 1);
    assert(abs(res_mul8 - expected_prod) <= 1);
    printf("  ok   64-bit multiplication preserves full 23-bit fractional precision\n");

    // Test 2: Dynamic Range and Unity Gain
    SAMPLE unity = F2S(1.0f);
    SAMPLE half = F2S(0.5f);
    SAMPLE prod_unity = MUL0_SS(unity, half);
    assert(abs(prod_unity - half) <= 1);
    printf("  ok   unity gain multiplication exact\n");

    // Test 3: Soft Clipping Saturation Bounds
    // Verify soft clipping table bounds
    for (int32_t val = 0; val < 40000; val += 100) {
        int32_t clipped = val;
        if (clipped >= FIRST_NONLIN) {
            if (clipped >= FIRST_HARDCLIP) {
                clipped = SAMPLE_MAX;
            } else {
                clipped = clipping_lookup_table[clipped - FIRST_NONLIN];
            }
        }
        assert(clipped <= SAMPLE_MAX);
        assert(clipped >= 0);
    }
    printf("  ok   soft-clipping table bounds verified (0 to %d)\n", SAMPLE_MAX);

    printf("All DSP precision tests passed successfully!\n");
    return 0;
}
