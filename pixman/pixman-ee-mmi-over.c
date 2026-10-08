/*
 * PS2 EE MMI unmasked 32-bit OVER and 8888-to-0565 SRC fast paths.
 * Copyright (c) 2026
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifdef HAVE_CONFIG_H
#include <pixman-config.h>
#endif

#include <stdint.h>
#include "pixman-private.h"
#include "pixman-combine32.h"

extern void ee_mmi_over_quads (uint32_t *dst, const uint32_t *src,
                               unsigned int quads, const uint16_t *constants);
extern void ee_mmi_8888_to_0565_quads (uint16_t *dst, const uint32_t *src,
                                       unsigned int quads, const uint32_t *masks);

/* Read-only, 16-byte-aligned literal vectors passed to assembly in a3. */
static const uint16_t over_constants[16] __attribute__((aligned(16))) =
{
    255, 255, 255, 255, 255, 255, 255, 255,
    128, 128, 128, 128, 128, 128, 128, 128
};
static const uint32_t convert_masks[12] __attribute__((aligned(16))) =
{
    0x0000f800, 0x0000f800, 0x0000f800, 0x0000f800,
    0x000007e0, 0x000007e0, 0x000007e0, 0x000007e0,
    0x0000001f, 0x0000001f, 0x0000001f, 0x0000001f
};

static force_inline void
over_pixel (uint32_t *dst, uint32_t s)
{
    if ((s >> 24) == 255)
        *dst = s;
    else if (s)
    {
        uint32_t d = *dst;
        uint32_t a = (~s) >> 24;
        UN8x4_MUL_UN8_ADD_UN8x4 (d, a, s);
        *dst = d;
    }
}

/* True for intersecting ranges, using subtraction to avoid pointer+size
 * wraparound and undefined ordering comparisons of unrelated pointers. */
static pixman_bool_t
spans_overlap (const void *src, size_t src_n, const void *dst, size_t dst_n)
{
    uintptr_t s = (uintptr_t)src;
    uintptr_t d = (uintptr_t)dst;
    return s <= d ? (d - s < src_n) : (s - d < dst_n);
}

static void
over_span (uint32_t *dst, const uint32_t *src, size_t n)
{
    unsigned int blocks;

    /* LQ and SQ mask away low address bits on the EE.
     * No 128-bit access is made unless both operands are aligned.
     */
    if (n < 8 || (((uintptr_t)dst ^ (uintptr_t)src) & 15) ||
        (dst != src && spans_overlap (src, n * 4, dst, n * 4)))
    {
        while (n--)
            over_pixel (dst++, *src++);
        return;
    }

    while (n && ((uintptr_t)dst & 15))
    {
        over_pixel (dst++, *src++);
        --n;
    }

    blocks = (unsigned int)(n / 4);
    if (blocks)
    {
        ee_mmi_over_quads (dst, src, blocks, over_constants);
        dst += (size_t)blocks * 4;
        src += (size_t)blocks * 4;
        n -= (size_t)blocks * 4;
    }

    while (n--)
        over_pixel (dst++, *src++);
}

void
ee_mmi_composite_over_8888 (pixman_implementation_t *imp,
                            pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint32_t *src_line, *dst_line;
    int src_stride, dst_stride;

    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint32_t,
                           src_stride, src_line, 1);
    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint32_t,
                           dst_stride, dst_line, 1);

    while (height-- > 0)
    {
        over_span (dst_line, src_line, (size_t)width);
        src_line += src_stride;
        dst_line += dst_stride;
    }
}

/* RGB565 and BGR565 use the same bit extraction: the two 8888 channel
 * orders are paired with their corresponding 0565 channel orders. */
static void
convert_span (uint16_t *dst, const uint32_t *src, size_t n)
{
    unsigned int blocks;
    size_t peeled = 0;

    /* If the source and destination alias, preserve the original C
     * fast path's forward traversal rather than vector-read ahead. */
    if (spans_overlap (src, n * 4, dst, n * 2))
    {
        while (n--)
            *dst++ = convert_8888_to_0565 (*src++);
        return;
    }

    if (n >= 32)
    {
        /* Source alignment repeats every four pixels and destination
         * alignment every eight. Try at most eight scalar pixels. */
        while (n && peeled < 8 &&
               (((uintptr_t)dst | (uintptr_t)src) & 15))
        {
            *dst++ = convert_8888_to_0565 (*src++);
            ++peeled;
            --n;
        }

        if ((((uintptr_t)dst | (uintptr_t)src) & 15) == 0)
        {
            blocks = (unsigned int)(n / 8);
            if (blocks)
            {
                ee_mmi_8888_to_0565_quads (dst, src, blocks, convert_masks);
                src += (size_t)blocks * 8;
                dst += (size_t)blocks * 8;
                n -= (size_t)blocks * 8;
            }
        }
    }

    while (n--)
        *dst++ = convert_8888_to_0565 (*src++);
}

void
ee_mmi_composite_src_8888_0565 (pixman_implementation_t *imp,
                                pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint32_t *src_line;
    uint16_t *dst_line;
    int src_stride, dst_stride;

    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint32_t,
                           src_stride, src_line, 1);
    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint16_t,
                           dst_stride, dst_line, 1);

    while (height-- > 0)
    {
        convert_span (dst_line, src_line, (size_t)width);
        src_line += src_stride;
        dst_line += dst_stride;
    }
}
