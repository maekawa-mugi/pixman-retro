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

/*
 * Solid + A8 coverage mask -> premultiplied 8888 OVER.
 *
 * A local 256-entry table performs the EXACT Pixman multiply-by-mask
 * rounding once per coverage value. Large scanlines then use the EE's
 * four-pixel OVER kernel on an aligned batch of masked source pixels.
 * For tiny spans, scalar processing avoids paying for table creation.
 */
static force_inline void
masked_solid_pixel (uint32_t *dst, uint32_t solid, uint8_t mask)
{
    uint32_t s, d;

    if (mask == 0)
        return;

    s = solid;
    if (mask != 255)
        UN8x4_MUL_UN8 (s, mask);

    if ((s >> 24) == 255)
    {
        *dst = s;
    }
    else
    {
        d = *dst;
        UN8x4_MUL_UN8_ADD_UN8x4 (d, (~s) >> 24, s);
        *dst = d;
    }
}

static void
masked_solid_span (uint32_t *dst, const uint8_t *mask, size_t n,
                   uint32_t solid, const uint32_t *table)
{
    uint32_t masked[64] __attribute__((aligned(16)));
    size_t i, count;

    /* Staging a batch of masks could change results when the A8 mask
     * aliases the output pixels. Preserve the C implementation's
     * forward read/modify/write ordering in that case.
     */
    if (!table || spans_overlap (mask, n, dst, n * 4))
    {
        while (n--)
        {
            masked_solid_pixel (dst, solid, *mask++);
            ++dst;
        }
        return;
    }

    while (n && ((uintptr_t)dst & 15))
    {
        masked_solid_pixel (dst, solid, *mask++);
        ++dst;
        --n;
    }

    while (n >= 4)
    {
        count = n < 64 ? (n & ~(size_t)3) : 64;
        for (i = 0; i < count; ++i)
            masked[i] = table[mask[i]];

        ee_mmi_over_quads (dst, masked, (unsigned int)(count / 4),
                           over_constants);
        dst += count;
        mask += count;
        n -= count;
    }

    while (n--)
    {
        masked_solid_pixel (dst, solid, *mask++);
        ++dst;
    }
}

void
ee_mmi_composite_over_n_8_8888 (pixman_implementation_t *imp,
                                pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint32_t *dst_line, color;
    uint8_t *mask_line;
    int dst_stride, mask_stride, row;
    uint32_t table[256];

    color = _pixman_image_get_solid (imp, src_image, dest_image->bits.format);
    if (color == 0)
        return;

    /* A one-off small rectangle is cheaper without the 256-value LUT. */
    if (width >= 32)
    {
        unsigned int m;
        for (m = 0; m != 256; ++m)
        {
            uint32_t s = color;
            if (m != 255)
                UN8x4_MUL_UN8 (s, m);
            table[m] = s;
        }
    }

    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint32_t,
                           dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint8_t,
                           mask_stride, mask_line, 1);

    for (row = 0; row < height; ++row)
    {
        masked_solid_span (dst_line, mask_line, (size_t)width,
                           color, width >= 32 ? table : NULL);
        dst_line += dst_stride;
        mask_line += mask_stride;
    }
}

/*
 * Premultiplied 8888 OVER into 565. For groups of eight pixels we
 * unpack RGB565 into two aligned 32-bit quads, use the existing OVER
 * MMI kernel on both, then pack the results using the existing 565
 * conversion kernel. This intentionally reuses the bit-exact kernels.
 */
static force_inline void
over_565_pixel (uint16_t *dst, uint32_t s)
{
    uint32_t d, a;

    if (!s)
        return;

    if ((s >> 24) == 255)
    {
        *dst = convert_8888_to_0565 (s);
        return;
    }

    d = convert_0565_to_0888 (*dst);
    a = (~s) >> 24;
    UN8x4_MUL_UN8_ADD_UN8x4 (d, a, s);
    *dst = convert_8888_to_0565 (d);
}

static void
over_565_span (uint16_t *dst, const uint32_t *src, size_t n)
{
    uint32_t dst_quads[8] __attribute__((aligned(16)));
    uint32_t src_quads[8] __attribute__((aligned(16)));
    unsigned int i;

    if (n < 32 || spans_overlap (src, n * 4, dst, n * 2))
    {
        while (n--)
            over_565_pixel (dst++, *src++);
        return;
    }

    /* Destination needs aligned SQ, source is copied to aligned
     * scratch so an unaligned texture can still use the MMI core. */
    while (n && ((uintptr_t)dst & 15))
    {
        over_565_pixel (dst++, *src++);
        --n;
    }

    while (n >= 8)
    {
        for (i = 0; i < 8; ++i)
        {
            dst_quads[i] = convert_0565_to_0888 (dst[i]);
            src_quads[i] = src[i];
        }

        ee_mmi_over_quads (dst_quads, src_quads, 2, over_constants);
        ee_mmi_8888_to_0565_quads (dst, dst_quads, 1, convert_masks);
        dst += 8;
        src += 8;
        n -= 8;
    }

    while (n--)
        over_565_pixel (dst++, *src++);
}

void
ee_mmi_composite_over_8888_0565 (pixman_implementation_t *imp,
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
        over_565_span (dst_line, src_line, (size_t)width);
        src_line += src_stride;
        dst_line += dst_stride;
    }
}


/*
 * Non-solid image + A8 mask OVER.
 *
 * The mask first modulates the premultiplied source using precisely the
 * same Pixman byte multiplication as in the scalar combine routines.
 * We gather 64 masked pixels at a time into a 16-byte-aligned scratch
 * buffer, then use the common MMI OVER kernel. The mask, source and
 * destination may have unrelated scanline alignments.
 *
 * When either source or mask aliases destination, retain the original
 * forward pixel-by-pixel traversal, because a batch read-ahead could
 * change the result.
 */
static force_inline void
masked_image_pixel (uint32_t *dst, uint32_t src, uint8_t mask,
                    pixman_bool_t src_is_opaque)
{
    if (!mask)
        return;

    if (src_is_opaque)
        src |= 0xff000000;

    if (mask != 255)
        UN8x4_MUL_UN8 (src, mask);

    over_pixel (dst, src);
}

static void
masked_image_span (uint32_t *dst, const uint32_t *src, const uint8_t *mask,
                   size_t n, pixman_bool_t src_is_opaque)
{
    uint32_t staged[64] __attribute__((aligned(16)));
    size_t batch, i;

    if (n < 16 ||
        spans_overlap (src, n * 4, dst, n * 4) ||
        spans_overlap (mask, n, dst, n * 4))
    {
        while (n--)
        {
            masked_image_pixel (dst, *src, *mask, src_is_opaque);
            ++src;
            ++mask;
            ++dst;
        }
        return;
    }

    while (n && ((uintptr_t)dst & 15))
    {
        masked_image_pixel (dst, *src, *mask, src_is_opaque);
        ++src;
        ++mask;
        ++dst;
        --n;
    }

    while (n >= 4)
    {
        batch = n >= 64 ? 64 : (n & ~(size_t)3);

        for (i = 0; i < batch; ++i)
        {
            uint32_t s = src[i];
            uint8_t m = mask[i];

            if (src_is_opaque)
                s |= 0xff000000;

            if (m != 255)
                UN8x4_MUL_UN8 (s, m);

            staged[i] = s;
        }

        ee_mmi_over_quads (dst, staged, (unsigned int)(batch / 4),
                           over_constants);
        dst += batch;
        src += batch;
        mask += batch;
        n -= batch;
    }

    while (n--)
    {
        masked_image_pixel (dst, *src, *mask, src_is_opaque);
        ++src;
        ++mask;
        ++dst;
    }
}

void
ee_mmi_composite_over_8888_8_8888 (pixman_implementation_t *imp,
                                   pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint32_t *src_line, *dst_line;
    uint8_t *mask_line;
    int src_stride, dst_stride, mask_stride;
    pixman_bool_t opaque =
        (src_image->bits.format == PIXMAN_x8r8g8b8 ||
         src_image->bits.format == PIXMAN_x8b8g8r8);

    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint32_t,
                           src_stride, src_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint8_t,
                           mask_stride, mask_line, 1);
    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint32_t,
                           dst_stride, dst_line, 1);

    while (height-- > 0)
    {
        masked_image_span (dst_line, src_line, mask_line,
                           (size_t)width, opaque);
        src_line += src_stride;
        mask_line += mask_stride;
        dst_line += dst_stride;
    }
}

/*
 * Solid premultiplied color OVER without mask.
 *
 * Allocate a single aligned repeated-source buffer for a composite call.
 * EE can process up to 64 pixels with one MMI call per destination chunk,
 * and does not need to fetch a second image from main RAM.
 */
void
ee_mmi_composite_over_solid_8888 (pixman_implementation_t *imp,
                                  pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint32_t solid = _pixman_image_get_solid (imp, src_image,
                                              dest_image->bits.format);
    uint32_t staged[64] __attribute__((aligned(16)));
    uint32_t *dst_line, *dst;
    int dst_stride, y, i;
    size_t n, batch;

    if (!solid)
        return;

    /* OVER with an opaque solid source is exactly SRC. Avoid the
     * multiply/pack pipeline and use Pixman's accelerated fill hook.
     */
    if ((solid >> 24) == 255)
    {
        pixman_fill (dest_image->bits.bits, dest_image->bits.rowstride,
                     PIXMAN_FORMAT_BPP (dest_image->bits.format),
                     dest_x, dest_y, width, height, solid);
        return;
    }

    for (i = 0; i < 64; ++i)
        staged[i] = solid;

    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint32_t,
                           dst_stride, dst_line, 1);

    for (y = 0; y < height; ++y)
    {
        dst = dst_line;
        n = (size_t)width;

        if (n < 16)
        {
            while (n--)
                over_pixel (dst++, solid);
        }
        else
        {
            while (n && ((uintptr_t)dst & 15))
            {
                over_pixel (dst++, solid);
                --n;
            }

            while (n >= 4)
            {
                batch = n >= 64 ? 64 : (n & ~(size_t)3);
                ee_mmi_over_quads (dst, staged, (unsigned int)(batch / 4),
                                   over_constants);
                dst += batch;
                n -= batch;
            }

            while (n--)
                over_pixel (dst++, solid);
        }

        dst_line += dst_stride;
    }
}


/*
 * Accelerate generic 32-bit unified-alpha OVER in the iterator combiner.
 *
 * This is used for uncommon image formats and transformations where none
 * of the specialized Pixman fast paths match. It preserves Pixman's
 * exact (S + D * (255 - Sa)) premultiplied composition and its mask
 * multiplication rounding.
 */
void
ee_mmi_combine_over_u (pixman_implementation_t *imp, pixman_op_t op,
                       uint32_t *dst, const uint32_t *src,
                       const uint32_t *mask, int width)
{
    uint32_t staged[64] __attribute__((aligned(16)));
    size_t n = width > 0 ? (size_t)width : 0;
    size_t count, i;

    if (!mask)
    {
        over_span (dst, src, n);
        return;
    }

    if (n < 16 ||
        spans_overlap (src, n * 4, dst, n * 4) ||
        spans_overlap (mask, n * 4, dst, n * 4))
    {
        while (n--)
        {
            uint32_t m = (*mask++) >> 24;
            uint32_t s = *src++;

            if (m == 255)
            {
                over_pixel (dst, s);
            }
            else if (m && s)
            {
                UN8x4_MUL_UN8 (s, m);
                over_pixel (dst, s);
            }
            ++dst;
        }
        return;
    }

    while (n && ((uintptr_t)dst & 15))
    {
        uint32_t s = *src++;
        uint32_t m = (*mask++) >> 24;
        if (m != 255)
            UN8x4_MUL_UN8 (s, m);
        over_pixel (dst++, s);
        --n;
    }

    while (n >= 4)
    {
        count = n >= 64 ? 64 : (n & ~(size_t)3);
        for (i = 0; i < count; ++i)
        {
            uint32_t s = src[i];
            uint32_t m = mask[i] >> 24;
            if (m != 255)
                UN8x4_MUL_UN8 (s, m);
            staged[i] = s;
        }

        ee_mmi_over_quads (dst, staged, (unsigned int)(count / 4),
                           over_constants);
        dst += count;
        src += count;
        mask += count;
        n -= count;
    }

    while (n--)
    {
        uint32_t s = *src++;
        uint32_t m = (*mask++) >> 24;
        if (m != 255)
            UN8x4_MUL_UN8 (s, m);
        over_pixel (dst++, s);
    }
}
