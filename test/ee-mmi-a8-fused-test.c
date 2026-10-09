/* Standalone PS2 EE fused solid + A8 OVER differential regression.
 * Link this file with pixman/pixman-ee-mmi-a8-asm.S using an EE compiler.
 */
#ifdef PS2_TEST_PROGRESS
#include "ps2/progress.h"
#endif
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern void ee_mmi_over_solid_a8_quads (uint32_t *, const uint8_t *,
                                        unsigned int, const uint32_t *);

#define PIXELS 128
#define GUARD 32

static uint32_t dst[PIXELS + GUARD] __attribute__((aligned(16)));
static uint32_t expected[PIXELS + GUARD] __attribute__((aligned(16)));
static uint8_t masks[PIXELS + GUARD] __attribute__((aligned(16)));
static uint8_t masks_saved[PIXELS + GUARD] __attribute__((aligned(16)));
static uint32_t constants[16] __attribute__((aligned(16)));
static uint32_t state = 0x49ab201fu;

static uint32_t
rnd (void)
{
    uint32_t x = state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return state = x;
}

static unsigned int
div255 (unsigned int a, unsigned int b)
{
    unsigned int p = a * b + 128;
    return (p + (p >> 8)) >> 8;
}

static uint32_t
reference (uint32_t d, uint32_t color, uint8_t m)
{
    uint32_t out = 0;
    unsigned int shift;
    unsigned int alpha = div255 (color >> 24, m);
    unsigned int inverse = 255 - alpha;
    for (shift = 0; shift < 32; shift += 8)
    {
        unsigned int src = div255 ((color >> shift) & 255, m);
        unsigned int dst = (d >> shift) & 255;
        unsigned int value = src + div255 (dst, inverse);
        if (value > 255)
            value = 255;
        out |= (uint32_t)value << shift;
    }
    return out;
}

int
main (void)
{
    state=0x49ab201fu;
    unsigned int iter, i, blocks, mask_offset, n;
    uint32_t color, alpha_word;

    for (iter = 0; iter < 16000; ++iter)
    {
#ifdef PS2_TEST_PROGRESS
        if ((iter & 255) == 0) ps2_progress_pulse(iter, 16000);
#endif
        mask_offset = iter % 16;
        /* Guarantee every A8 coverage value reaches the MMI kernel.
         * The remaining iterations vary the quad count, including zero.
         */
        blocks = iter < 256 ? 32 : (iter / 16) % 33;
        n = blocks * 4;
        color = rnd ();
        if (iter % 13 == 0)
            color = 0;
        else if (iter % 13 == 1)
            color = 0xffffffffu;
        else if (iter % 13 == 2)
            color = (color & 0x00ffffffu) | 0xff000000u;
        else if (iter % 13 == 3)
            color &= 0x00ffffffu;

        alpha_word = color >> 24;
        alpha_word |= alpha_word << 16;
        for (i = 0; i < 4; ++i)
        {
            constants[i] = color;
            constants[i + 4] = 0x00800080u;
            constants[i + 8] = alpha_word;
            constants[i + 12] = 0x00ff00ffu;
        }

        for (i = 0; i < PIXELS + GUARD; ++i)
        {
            masks[i] = (uint8_t)rnd ();
            dst[i] = rnd ();
        }

        /* Exercise all coverage values at different byte offsets. */
        if (iter < 256)
        {
            for (i = 0; i < n; ++i)
                masks[mask_offset + i] = (uint8_t)iter;
        }
        else if (iter % 11 == 0)
        {
            for (i = 0; i < n; ++i)
                masks[mask_offset + i] = 0;
        }
        else if (iter % 11 == 1)
        {
            for (i = 0; i < n; ++i)
                masks[mask_offset + i] = 255;
        }

        memcpy (masks_saved, masks, sizeof (masks));
        memcpy (expected, dst, sizeof (dst));
        for (i = 0; i < n; ++i)
            expected[i] = reference (dst[i], color, masks[mask_offset + i]);

        ee_mmi_over_solid_a8_quads (dst, masks + mask_offset, blocks,
                                    constants);
        if (memcmp (dst, expected, sizeof (dst)) != 0 ||
            memcmp (masks, masks_saved, sizeof (masks)) != 0)
        {
            for (i = 0; i < PIXELS + GUARD; ++i)
                if (dst[i] != expected[i])
                {
                    printf ("FAIL iter=%u offset=%u blocks=%u pixel=%u expected=%08x got=%08x\n",
                            iter, mask_offset, blocks, i, expected[i], dst[i]);
                    return 1;
                }
            puts ("FAIL: mask unexpectedly modified");
            return 1;
        }
    }

    puts ("PASS: fused A8 OVER 16000 differential cases");
    return 0;
}
