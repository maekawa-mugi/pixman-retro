/* EE MMI direct RGB565 OVER differential regression.
 * Compile with PS2 EE toolchain and pixman/pixman-ee-mmi-565-asm.S.
 */
#ifdef PS2_TEST_PROGRESS
#include "ps2/progress.h"
#endif
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern void ee_mmi_over_8888_0565_quads (uint16_t *, const uint32_t *,
                                          unsigned int, const uint32_t *);

#define PIXELS 128
#define GUARD 16
static uint32_t src[PIXELS + GUARD] __attribute__((aligned(16)));
static uint32_t src_copy[PIXELS + GUARD] __attribute__((aligned(16)));
static uint16_t dst[PIXELS + GUARD] __attribute__((aligned(16)));
static uint16_t expect[PIXELS + GUARD] __attribute__((aligned(16)));
static const uint32_t constants[28] __attribute__((aligned(16))) =
{
    0x001f001f, 0x001f001f, 0x001f001f, 0x001f001f,
    0x003f003f, 0x003f003f, 0x003f003f, 0x003f003f,
    0x00ff00ff, 0x00ff00ff, 0x00ff00ff, 0x00ff00ff,
    0x00800080, 0x00800080, 0x00800080, 0x00800080,
    0x0000f800, 0x0000f800, 0x0000f800, 0x0000f800,
    0x000007e0, 0x000007e0, 0x000007e0, 0x000007e0,
    0x0000001f, 0x0000001f, 0x0000001f, 0x0000001f
};

static uint32_t rng_state = 0x53801723u;
static uint32_t
rng (void)
{
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return rng_state = x;
}

static uint32_t
expand565 (uint16_t s)
{
    return ((((uint32_t)s << 3) & 0xf8) | (((uint32_t)s >> 2) & 0x7)) |
           (((((uint32_t)s << 5) & 0xfc00) | (((uint32_t)s >> 1) & 0x300))) |
           ((((uint32_t)s << 8) & 0xf80000) | (((uint32_t)s << 3) & 0x70000));
}

static uint16_t
reduce565 (uint32_t s)
{
    uint32_t a = (s >> 3) & 0x1f001f;
    uint32_t b = s & 0xfc00;
    a |= a >> 5;
    a |= b >> 5;
    return (uint16_t)a;
}

static unsigned int
mul255 (unsigned int a, unsigned int b)
{
    unsigned int p = a * b + 128;
    return (p + (p >> 8)) >> 8;
}

static uint16_t
reference (uint32_t s, uint16_t d)
{
    unsigned int a = 255 - (s >> 24), sh;
    uint32_t old = expand565 (d), out = 0;
    for (sh = 0; sh < 32; sh += 8)
    {
        unsigned int src_ch = (s >> sh) & 255;
        unsigned int dst_ch = (old >> sh) & 255;
        unsigned int ch = src_ch + mul255 (dst_ch, a);
        if (ch > 255)
            ch = 255;
        out |= (uint32_t)ch << sh;
    }
    return reduce565 (out);
}

int
main (void)
{
    rng_state=0x53801723u;
    unsigned int it, i, blocks, n;
    for (it = 0; it < 16000; ++it)
    {
#ifdef PS2_TEST_PROGRESS
        if ((it & 255) == 0) ps2_progress_pulse(it, 16000);
#endif
        blocks = it % (PIXELS / 8 + 1);
        n = blocks * 8;
        for (i = 0; i < PIXELS + GUARD; ++i)
        {
            src[i] = rng ();
            if (it % 11 == 0)
                src[i] = 0;
            else if (it % 11 == 1)
                src[i] |= 0xff000000u;
            else if (it % 11 == 2)
                src[i] &= 0x00ffffffu;
            else if (it % 11 == 3)
                src[i] = (src[i] & 0x00ffffffu) | 0x7f000000u;
            dst[i] = (uint16_t)rng ();
            expect[i] = i < n ? reference (src[i], dst[i]) : dst[i];
        }
        memcpy (src_copy, src, sizeof (src));
        ee_mmi_over_8888_0565_quads (dst, src, blocks, constants);
        if (memcmp (dst, expect, sizeof (dst)) ||
            memcmp (src, src_copy, sizeof (src)))
        {
            for (i = 0; i < PIXELS + GUARD; ++i)
                if (dst[i] != expect[i])
                {
                    printf ("FAIL case=%u blocks=%u pixel=%u expected=%04x actual=%04x\n",
                            it, blocks, i, expect[i], dst[i]);
                    return 1;
                }
            puts ("FAIL: source modified");
            return 1;
        }
    }

    /* Exhaustive destination RGB565 values with alpha=0, 128 and 255.
     * This catches errors in 5/6-bit expansion and re-quantization at
     * every bit boundary, not only pseudorandomly sampled colors.
     */
    {
        static const uint32_t solids[3] =
        {
            0x00000000u, 0x80201008u, 0xff112233u
        };
        unsigned int mode, base;
        for (mode = 0; mode < 3; ++mode)
        {
            for (base = 0; base < 65536; base += PIXELS)
            {
                for (i = 0; i < PIXELS + GUARD; ++i)
                {
                    src[i] = solids[mode];
                    dst[i] = i < PIXELS ? (uint16_t)(base + i) :
                                               (uint16_t)rng ();
                    expect[i] = i < PIXELS ?
                                reference (src[i], dst[i]) : dst[i];
                }
                memcpy (src_copy, src, sizeof (src));
                ee_mmi_over_8888_0565_quads (dst, src, PIXELS / 8,
                                               constants);
                if (memcmp (dst, expect, sizeof (dst)) ||
                    memcmp (src, src_copy, sizeof (src)))
                {
                    printf ("FAIL exhaustive mode=%u base=%u\n", mode, base);
                    return 1;
                }
            }
        }
    }

    puts ("PASS: 16000 random cases and 196608 exhaustive RGB565 pixels");
    return 0;
}
