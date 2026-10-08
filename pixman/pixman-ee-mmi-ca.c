/*
 * PlayStation 2 EE MMI component-alpha (CA) solid OVER.
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
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifdef HAVE_CONFIG_H
#include <pixman-config.h>
#endif

#include <stdint.h>
#include "pixman-private.h"
#include "pixman-combine32.h"

/* Each 32-byte prepared quad contains 4 premultiplied source words and
 * 4 words holding the inverse per-channel destination multiplier. */
extern void ee_mmi_ca_over_quads (uint32_t *dst, const uint32_t *prepared,
                                   unsigned int blocks,
                                   const uint16_t *rounding);

static const uint16_t rounding_128[8] __attribute__((aligned(16))) =
{
    128, 128, 128, 128, 128, 128, 128, 128
};

static force_inline void
ca_over_scalar (uint32_t *dst, uint32_t color, uint32_t mask)
{
    uint32_t s = color;
    uint32_t alpha = color >> 24;
    uint32_t d;

    if (!mask)
        return;

    if (mask == 0xffffffff)
    {
        if (alpha == 255)
            *dst = s;
        else
        {
            d = *dst;
            UN8x4_MUL_UN8_ADD_UN8x4 (d, (~s) >> 24, s);
            *dst = d;
        }
        return;
    }

    d = *dst;
    UN8x4_MUL_UN8x4 (s, mask);
    UN8x4_MUL_UN8 (mask, alpha);
    mask = ~mask;
    UN8x4_MUL_UN8x4_ADD_UN8x4 (d, mask, s);
    *dst = d;
}

static pixman_bool_t
overlap_32 (const uint32_t *x, const uint32_t *y, size_t n)
{
    uintptr_t a = (uintptr_t)x;
    uintptr_t b = (uintptr_t)y;
    size_t size = n * 4;

    return a <= b ? b - a < size : a - b < size;
}

static void
ca_over_span (uint32_t *dst, const uint32_t *mask, size_t n,
              uint32_t color)
{
    uint32_t prepared[128] __attribute__((aligned(16)));
    unsigned int src_alpha = color >> 24;
    size_t i, blocks, count;

    if (n < 16 || overlap_32 (dst, mask, n))
    {
        while (n--)
            ca_over_scalar (dst++, color, *mask++);
        return;
    }

    while (n && ((uintptr_t)dst & 15))
    {
        ca_over_scalar (dst++, color, *mask++);
        --n;
    }

    while (n >= 4)
    {
        count = n >= 64 ? 64 : (n & ~(size_t)3);
        blocks = count / 4;

        /* This local staging buffer is 4 KB? No: 128x4 = 512 bytes.
         * Four pixels: [source0..3][inverse0..3].
         * The MMI kernel performs destination * inverse / 255 and
         * saturating addition of scaled source to the result.
         */
        for (i = 0; i < count; ++i)
        {
            uint32_t m = mask[i];
            uint32_t s = color;
            uint32_t ia;
            size_t offset = (i / 4) * 8 + (i & 3);

            UN8x4_MUL_UN8x4 (s, m);
            ia = m;
            UN8x4_MUL_UN8 (ia, src_alpha);

            prepared[offset] = s;
            prepared[offset + 4] = ~ia;
        }

        ee_mmi_ca_over_quads (dst, prepared, (unsigned int)blocks,
                              rounding_128);

        dst += count;
        mask += count;
        n -= count;
    }

    while (n--)
        ca_over_scalar (dst++, color, *mask++);
}

void
ee_mmi_composite_over_solid_8888_ca (pixman_implementation_t *imp,
                                      pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint32_t solid = _pixman_image_get_solid (imp, src_image,
                                              dest_image->bits.format);
    uint32_t *mask_line, *dst_line;
    int mask_stride, dst_stride;

    if (!solid)
        return;

    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint32_t,
                           mask_stride, mask_line, 1);
    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint32_t,
                           dst_stride, dst_line, 1);

    while (height-- > 0)
    {
        ca_over_span (dst_line, mask_line, (size_t)width, solid);
        mask_line += mask_stride;
        dst_line += dst_stride;
    }
}
