/* Standalone differential test. Compile on the PS2 EE toolchain
 * together with pixman/pixman-ee-mmi-ca-asm.S; no pixman headers needed.
 */
#ifdef PS2_TEST_PROGRESS
#include "ps2/progress.h"
#endif
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern void ee_mmi_ca_over_solid_quads (uint32_t *, const uint32_t *,
                                        unsigned int, const uint32_t *);

#define PIXELS 128
#define CANARY 8
static uint32_t dst[PIXELS + CANARY] __attribute__((aligned(16)));
static uint32_t mask[PIXELS + CANARY] __attribute__((aligned(16)));
static uint32_t expect[PIXELS + CANARY] __attribute__((aligned(16)));
static uint32_t constants[16] __attribute__((aligned(16)));
static uint32_t state = 0x7154b3adu;

static uint32_t
random32 (void)
{
    uint32_t x = state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return state = x;
}

static unsigned int
mul255 (unsigned int a, unsigned int b)
{
    unsigned int x = a * b + 128;
    return (x + (x >> 8)) >> 8;
}

static uint32_t
reference_pixel (uint32_t d, uint32_t color, uint32_t m)
{
    uint32_t out = 0;
    unsigned int sh, a = color >> 24;
    for (sh = 0; sh < 32; sh += 8)
    {
        unsigned int s = (color >> sh) & 255;
        unsigned int mask_channel = (m >> sh) & 255;
        unsigned int dst_channel = (d >> sh) & 255;
        unsigned int inv = 255 - mul255 (a, mask_channel);
        unsigned int result = mul255 (s, mask_channel) +
                              mul255 (dst_channel, inv);
        if (result > 255)
            result = 255;
        out |= (uint32_t)result << sh;
    }
    return out;
}

int
main (void)
{
    state=0x7154b3adu;
    unsigned int it, i, blocks, count;
    uint32_t color, a;

    for (it = 0; it < 12000; ++it)
    {
#ifdef PS2_TEST_PROGRESS
        if ((it & 255) == 0) ps2_progress_pulse(it, 12000);
#endif
        blocks = it % (PIXELS / 4 + 1);
        count = blocks * 4;
        color = random32 ();
        if (it % 7 == 0)
            color = 0;
        else if (it % 7 == 1)
            color = 0xffffffffu;
        else if (it % 7 == 2)
            color |= 0xff000000u;
        else if (it % 7 == 3)
            color &= 0x00ffffffu;

        a = color >> 24;
        a |= a << 16;
        for (i = 0; i < 4; ++i)
        {
            constants[i] = color;
            constants[i + 4] = 0x00800080u;
            constants[i + 8] = a;
            constants[i + 12] = 0x00ff00ffu;
        }

        for (i = 0; i < PIXELS + CANARY; ++i)
        {
            uint32_t m = random32 ();
            if (i % 11 == 0)
                m = 0;
            else if (i % 11 == 1)
                m = 0xffffffffu;
            else if (i % 11 == 2)
                m = 0x0080ff01u;
            mask[i] = m;
            dst[i] = random32 ();
            expect[i] = i < count ? reference_pixel (dst[i], color, m)
                                  : dst[i];
        }

        ee_mmi_ca_over_solid_quads (dst, mask, blocks, constants);
        if (memcmp (dst, expect, sizeof (dst)) != 0)
        {
            for (i = 0; i < PIXELS + CANARY; ++i)
                if (dst[i] != expect[i])
                {
                    printf ("FAIL case=%u quads=%u pixel=%u expected=%08x got=%08x\n",
                            it, blocks, i, expect[i], dst[i]);
                    return 1;
                }
            return 1;
        }
    }

    puts ("PASS: fused EE CA 12000 differential cases");
    return 0;
}
