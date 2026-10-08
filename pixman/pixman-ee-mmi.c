/*
 * PlayStation 2 Emotion Engine 128-bit Multimedia Instruction fast paths.
 *
 * Copyright (c) 2026
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include <pixman-config.h>
#endif

#include <stdint.h>
#include <string.h>
#include "pixman-private.h"

/* Assembly routines accept ONLY 16-byte aligned pointers and whole quads.
 * The C side handles misaligned heads, tails, small spans, and overlaps.
 */
extern void ee_mmi_add_quads (uint8_t *dst, const uint8_t *src,
                              unsigned int quads);
extern void ee_mmi_copy_quads (uint8_t *dst, const uint8_t *src,
                               unsigned int quads);
extern void ee_mmi_fill_quads (uint8_t *dst, const uint32_t *pattern,
                               unsigned int quads);

static void
add_bytes_scalar (uint8_t *dst, const uint8_t *src, size_t n)
{
    size_t i;

    for (i = 0; i < n; ++i)
    {
        unsigned int value = (unsigned int)dst[i] + src[i];
        dst[i] = (uint8_t)(value > 255 ? 255 : value);
    }
}

static pixman_bool_t
overlaps (const uint8_t *a, const uint8_t *b, size_t n)
{
    uintptr_t x = (uintptr_t)a;
    uintptr_t y = (uintptr_t)b;

    return x < y ? (y - x < n) : (x - y < n);
}

static void
add_bytes (uint8_t *dst, const uint8_t *src, size_t n)
{
    size_t prefix;
    unsigned int quads;

    /* Scalar C matches the forward traversal of Pixman's C ADD path.
     * Distinct overlapping spans must not be processed block-at-a-time.
     */
    if (n < 64 ||
        (((uintptr_t)dst ^ (uintptr_t)src) & 15) ||
        (dst != src && overlaps (dst, src, n)))
    {
        add_bytes_scalar (dst, src, n);
        return;
    }

    prefix = (16 - ((uintptr_t)dst & 15)) & 15;
    if (prefix)
    {
        add_bytes_scalar (dst, src, prefix);
        dst += prefix;
        src += prefix;
        n -= prefix;
    }

    quads = (unsigned int)(n / 16);
    if (quads)
    {
        ee_mmi_add_quads (dst, src, quads);
        dst += (size_t)quads * 16;
        src += (size_t)quads * 16;
        n -= (size_t)quads * 16;
    }

    add_bytes_scalar (dst, src, n);
}

static void
copy_bytes (uint8_t *dst, const uint8_t *src, size_t n)
{
    size_t prefix;
    unsigned int quads;

    if (dst == src)
        return;

    if (overlaps (dst, src, n))
    {
        memmove (dst, src, n);
        return;
    }

    if (n < 64 || (((uintptr_t)dst ^ (uintptr_t)src) & 15))
    {
        memcpy (dst, src, n);
        return;
    }

    prefix = (16 - ((uintptr_t)dst & 15)) & 15;
    if (prefix)
    {
        memcpy (dst, src, prefix);
        dst += prefix;
        src += prefix;
        n -= prefix;
    }

    quads = (unsigned int)(n / 16);
    if (quads)
    {
        ee_mmi_copy_quads (dst, src, quads);
        dst += (size_t)quads * 16;
        src += (size_t)quads * 16;
        n -= (size_t)quads * 16;
    }

    if (n)
        memcpy (dst, src, n);
}

static void
ee_mmi_composite_add_8888 (pixman_implementation_t *imp,
                            pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint8_t *src_line, *dst_line;
    int src_stride, dst_stride;

    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint8_t,
                           src_stride, src_line, 4);
    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint8_t,
                           dst_stride, dst_line, 4);

    while (height-- > 0)
    {
        add_bytes (dst_line, src_line, (size_t)width * 4);
        src_line += src_stride;
        dst_line += dst_stride;
    }
}

static void
ee_mmi_composite_add_a8 (pixman_implementation_t *imp,
                          pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint8_t *src_line, *dst_line;
    int src_stride, dst_stride;

    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint8_t,
                           src_stride, src_line, 1);
    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint8_t,
                           dst_stride, dst_line, 1);

    while (height-- > 0)
    {
        add_bytes (dst_line, src_line, (size_t)width);
        src_line += src_stride;
        dst_line += dst_stride;
    }
}

static void
ee_mmi_composite_src_8888 (pixman_implementation_t *imp,
                            pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint8_t *src_line, *dst_line;
    int src_stride, dst_stride;

    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint8_t,
                           src_stride, src_line, 4);
    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint8_t,
                           dst_stride, dst_line, 4);

    while (height-- > 0)
    {
        copy_bytes (dst_line, src_line, (size_t)width * 4);
        src_line += src_stride;
        dst_line += dst_stride;
    }
}

static pixman_bool_t
ee_mmi_fill (pixman_implementation_t *imp,
             uint32_t *bits, int stride, int bpp,
             int x, int y, int width, int height, uint32_t filler)
{
    uint32_t pattern[4] __attribute__((aligned(16)));
    int row, i;

    if ((bpp != 16 && bpp != 32) || width <= 0 || height <= 0)
        return FALSE;

    /* pixman_fill() passes the 16-bit color in filler's lower halfword.
     * Replicate it eight times for one aligned 128-bit SQ.
     */
    if (bpp == 16)
    {
        uint32_t p = (uint16_t)filler;
        p |= p << 16;
        for (i = 0; i < 4; ++i)
            pattern[i] = p;

        for (row = 0; row < height; ++row)
        {
            uint16_t *dst = (uint16_t *)(bits +
                                        (ptrdiff_t)(y + row) * stride) + x;
            int remaining = width;

            while (remaining && ((uintptr_t)dst & 15))
            {
                *dst++ = (uint16_t)filler;
                --remaining;
            }

            if (remaining >= 32)
            {
                unsigned int quads = (unsigned int)remaining / 8;
                ee_mmi_fill_quads ((uint8_t *)dst, pattern, quads);
                dst += (size_t)quads * 8;
                remaining -= (int)(quads * 8);
            }

            while (remaining--)
                *dst++ = (uint16_t)filler;
        }
    }
    else
    {
        for (i = 0; i < 4; ++i)
            pattern[i] = filler;

        for (row = 0; row < height; ++row)
        {
            uint32_t *dst = bits + (ptrdiff_t)(y + row) * stride + x;
            int remaining = width;

            while (remaining && ((uintptr_t)dst & 15))
            {
                *dst++ = filler;
                --remaining;
            }

            if (remaining >= 16)
            {
                unsigned int quads = (unsigned int)remaining / 4;
                ee_mmi_fill_quads ((uint8_t *)dst, pattern, quads);
                dst += (size_t)quads * 4;
                remaining -= (int)(quads * 4);
            }

            while (remaining--)
                *dst++ = filler;
        }
    }

    return TRUE;
}

/* Composite entry points are compiled from pixman-ee-mmi-over.c. */
extern void ee_mmi_composite_over_8888 (pixman_implementation_t *,
                                         pixman_composite_info_t *);
extern void ee_mmi_composite_src_8888_0565 (pixman_implementation_t *,
                                             pixman_composite_info_t *);
extern void ee_mmi_composite_over_n_8_8888 (pixman_implementation_t *,
                                             pixman_composite_info_t *);
extern void ee_mmi_composite_over_8888_0565 (pixman_implementation_t *,
                                              pixman_composite_info_t *);
extern void ee_mmi_composite_over_8888_8_8888 (pixman_implementation_t *,
                                                pixman_composite_info_t *);
extern void ee_mmi_composite_over_solid_8888 (pixman_implementation_t *,
                                               pixman_composite_info_t *);

static const pixman_fast_path_t ee_mmi_fast_paths[] =
{
    /* Premultiplied 8888 source with A8 coverage mask.
     * X888 variants are treated as fully opaque before masking. */
    PIXMAN_STD_FAST_PATH (OVER, a8r8g8b8, a8, a8r8g8b8,
                          ee_mmi_composite_over_8888_8_8888),
    PIXMAN_STD_FAST_PATH (OVER, a8r8g8b8, a8, x8r8g8b8,
                          ee_mmi_composite_over_8888_8_8888),
    PIXMAN_STD_FAST_PATH (OVER, a8b8g8r8, a8, a8b8g8r8,
                          ee_mmi_composite_over_8888_8_8888),
    PIXMAN_STD_FAST_PATH (OVER, a8b8g8r8, a8, x8b8g8r8,
                          ee_mmi_composite_over_8888_8_8888),
    PIXMAN_STD_FAST_PATH (OVER, x8r8g8b8, a8, a8r8g8b8,
                          ee_mmi_composite_over_8888_8_8888),
    PIXMAN_STD_FAST_PATH (OVER, x8r8g8b8, a8, x8r8g8b8,
                          ee_mmi_composite_over_8888_8_8888),
    PIXMAN_STD_FAST_PATH (OVER, x8b8g8r8, a8, a8b8g8r8,
                          ee_mmi_composite_over_8888_8_8888),
    PIXMAN_STD_FAST_PATH (OVER, x8b8g8r8, a8, x8b8g8r8,
                          ee_mmi_composite_over_8888_8_8888),

    /* Solid-color alpha OVER without a coverage mask. */
    PIXMAN_STD_FAST_PATH (OVER, solid, null, a8r8g8b8,
                          ee_mmi_composite_over_solid_8888),
    PIXMAN_STD_FAST_PATH (OVER, solid, null, x8r8g8b8,
                          ee_mmi_composite_over_solid_8888),
    PIXMAN_STD_FAST_PATH (OVER, solid, null, a8b8g8r8,
                          ee_mmi_composite_over_solid_8888),
    PIXMAN_STD_FAST_PATH (OVER, solid, null, x8b8g8r8,
                          ee_mmi_composite_over_solid_8888),

    /* Solid-color glyph/text drawing with an A8 coverage mask. */
    PIXMAN_STD_FAST_PATH (OVER, solid, a8, a8r8g8b8,
                          ee_mmi_composite_over_n_8_8888),
    PIXMAN_STD_FAST_PATH (OVER, solid, a8, x8r8g8b8,
                          ee_mmi_composite_over_n_8_8888),
    PIXMAN_STD_FAST_PATH (OVER, solid, a8, a8b8g8r8,
                          ee_mmi_composite_over_n_8_8888),
    PIXMAN_STD_FAST_PATH (OVER, solid, a8, x8b8g8r8,
                          ee_mmi_composite_over_n_8_8888),

    /* Premultiplied 8888 OVER onto 16-bit RGB565 or BGR565. */
    PIXMAN_STD_FAST_PATH (OVER, a8r8g8b8, null, r5g6b5,
                          ee_mmi_composite_over_8888_0565),
    PIXMAN_STD_FAST_PATH (OVER, a8b8g8r8, null, b5g6r5,
                          ee_mmi_composite_over_8888_0565),
    PIXMAN_STD_FAST_PATH (OVER, a8r8g8b8, null, a8r8g8b8,
                          ee_mmi_composite_over_8888),
    PIXMAN_STD_FAST_PATH (OVER, a8r8g8b8, null, x8r8g8b8,
                          ee_mmi_composite_over_8888),
    PIXMAN_STD_FAST_PATH (OVER, a8b8g8r8, null, a8b8g8r8,
                          ee_mmi_composite_over_8888),
    PIXMAN_STD_FAST_PATH (OVER, a8b8g8r8, null, x8b8g8r8,
                          ee_mmi_composite_over_8888),

    PIXMAN_STD_FAST_PATH (SRC, a8r8g8b8, null, r5g6b5,
                          ee_mmi_composite_src_8888_0565),
    PIXMAN_STD_FAST_PATH (SRC, x8r8g8b8, null, r5g6b5,
                          ee_mmi_composite_src_8888_0565),
    PIXMAN_STD_FAST_PATH (SRC, a8b8g8r8, null, b5g6r5,
                          ee_mmi_composite_src_8888_0565),
    PIXMAN_STD_FAST_PATH (SRC, x8b8g8r8, null, b5g6r5,
                          ee_mmi_composite_src_8888_0565),

    PIXMAN_STD_FAST_PATH (ADD, a8r8g8b8, null, a8r8g8b8,
                          ee_mmi_composite_add_8888),
    PIXMAN_STD_FAST_PATH (ADD, a8b8g8r8, null, a8b8g8r8,
                          ee_mmi_composite_add_8888),
    PIXMAN_STD_FAST_PATH (ADD, a8, null, a8,
                          ee_mmi_composite_add_a8),

    PIXMAN_STD_FAST_PATH (SRC, a8r8g8b8, null, a8r8g8b8,
                          ee_mmi_composite_src_8888),
    PIXMAN_STD_FAST_PATH (SRC, a8b8g8r8, null, a8b8g8r8,
                          ee_mmi_composite_src_8888),
    PIXMAN_STD_FAST_PATH (SRC, x8r8g8b8, null, x8r8g8b8,
                          ee_mmi_composite_src_8888),
    PIXMAN_STD_FAST_PATH (SRC, x8b8g8r8, null, x8b8g8r8,
                          ee_mmi_composite_src_8888),

    { PIXMAN_OP_NONE }
};

pixman_implementation_t *
_pixman_implementation_create_ee_mmi (pixman_implementation_t *fallback)
{
    pixman_implementation_t *imp =
        _pixman_implementation_create (fallback, ee_mmi_fast_paths);

    if (imp)
        imp->fill = ee_mmi_fill;

    return imp;
}
