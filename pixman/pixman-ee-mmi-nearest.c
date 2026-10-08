/*
 * PlayStation 2 EE MMI nearest-neighbor scaled 8888 OVER.
 *
 * Follow Pixman's generic nearest sampler for transforms and repeats.
 * Stage up to 64 pixels with the original byte/coordinate semantics;
 * use the existing EE MMI 4-pixel OVER composite for the actual blend.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.
 */
#ifdef HAVE_CONFIG_H
#include <pixman-config.h>
#endif

#include <stdint.h>
#include "pixman-private.h"
#include "pixman-inlines.h"
#include "pixman-combine32.h"

extern void ee_mmi_over_quads (uint32_t *dst, const uint32_t *src,
                               unsigned int quads, const uint16_t *constants);

extern void ee_mmi_nearest_opaque_quads (uint32_t *dst,
                                          const uint32_t *src,
                                          unsigned int quads,
                                          const uint32_t *alpha_mask);

static const uint32_t nearest_alpha_mask[4]
    __attribute__((aligned(16))) =
{
    0xff000000u, 0xff000000u, 0xff000000u, 0xff000000u
};

static const uint16_t nearest_constants[16] __attribute__((aligned(16))) =
{
    255, 255, 255, 255, 255, 255, 255, 255,
    128, 128, 128, 128, 128, 128, 128, 128
};

/* Exactly the scalar OVER behavior of fast_composite_scaled_nearest. */
static force_inline void
nearest_over_pixel (uint32_t *dst, uint32_t s)
{
    if (s)
    {
        uint32_t a = 255 - (s >> 24);
        if (a == 0)
            *dst = s;
        else
            UN8x4_MUL_UN8_ADD_UN8x4 (*dst, a, s);
    }
}

static force_inline uint32_t
nearest_fetch (const uint32_t *src, pixman_repeat_t src_repeat,
               int x, int src_width, pixman_bool_t src_is_opaque)
{
    if (!repeat (src_repeat, &x, src_width))
        return 0;

    if (src_is_opaque)
        return src[x] | 0xff000000;

    return src[x];
}

void
ee_mmi_composite_nearest_over_8888 (pixman_implementation_t *imp,
                                    pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint32_t *dst_line, *src_base;
    int dst_stride, src_stride;
    int src_width, src_height;
    pixman_repeat_t src_repeat;
    pixman_fixed_t unit_x, unit_y, vy;
    pixman_vector_t v;
    pixman_bool_t opaque, alias;
    uint32_t staged[64] __attribute__((aligned(16)));

    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint32_t,
                           dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (src_image, 0, 0, uint32_t,
                           src_stride, src_base, 1);

    v.vector[0] = pixman_int_to_fixed (src_x) + pixman_fixed_1 / 2;
    v.vector[1] = pixman_int_to_fixed (src_y) + pixman_fixed_1 / 2;
    v.vector[2] = pixman_fixed_1;

    if (!pixman_transform_point_3d (src_image->common.transform, &v))
        return;

    unit_x = src_image->common.transform->matrix[0][0];
    unit_y = src_image->common.transform->matrix[1][1];

    /* Same fixed-point half-pixel rounding as fast_composite_scaled_nearest. */
    v.vector[0] -= pixman_fixed_e;
    v.vector[1] -= pixman_fixed_e;

    src_width = src_image->bits.width;
    src_height = src_image->bits.height;
    src_repeat = src_image->common.repeat;
    opaque = (src_image->bits.format == PIXMAN_x8r8g8b8 ||
              src_image->bits.format == PIXMAN_x8b8g8r8);
    alias = (src_image->bits.bits == dest_image->bits.bits);

    /* An empty source contributes transparent black for OVER. Avoid
     * invoking the NORMAL/PAD repeat helpers with a zero-sized image.
     */
    if (src_width <= 0 || src_height <= 0)
        return;

    vy = v.vector[1];
    while (height-- > 0)
    {
        int sy = pixman_fixed_to_int (vy);
        uint32_t *dst = dst_line;
        size_t n = (size_t)width;
        pixman_fixed_t vx = v.vector[0];
        const uint32_t *src;
        int i;

        dst_line += dst_stride;
        vy += unit_y;

        /* OUTSIDE source with repeat NONE leaves destination unchanged
         * for OVER, just as Pixman's reference nearest fast path. */
        if (!repeat (src_repeat, &sy, src_height))
            continue;

        src = src_base + (ptrdiff_t)sy * src_stride;

        /* At unit X scale, all nearest samples are consecutive.
         * When this run is wholly in bounds and non-aliased, pass source
         * directly to EE MMI and skip the 64-pixel staging buffer.
         * Opaque XRGB needs its high alpha bits set before destination
         * storage; the dedicated MMI kernel handles that.
         */
        if (unit_x == pixman_fixed_1 && !alias && n >= 16)
        {
            int first_x = pixman_fixed_to_int (vx);

            if (first_x >= 0 && first_x < src_width &&
                n <= (size_t)(src_width - first_x))
            {
                const uint32_t *contiguous = src + first_x;

                if ((((uintptr_t)dst ^ (uintptr_t)contiguous) & 15) == 0)
                {
                    size_t blocks;

                    while (n && ((uintptr_t)dst & 15))
                    {
                        uint32_t pixel = *contiguous++;
                        if (opaque)
                            pixel |= 0xff000000u;
                        nearest_over_pixel (dst++, pixel);
                        --n;
                    }

                    blocks = n / 4;
                    if (blocks)
                    {
                        if (opaque)
                            ee_mmi_nearest_opaque_quads (
                                dst, contiguous, (unsigned int)blocks,
                                nearest_alpha_mask);
                        else
                            ee_mmi_over_quads (
                                dst, contiguous, (unsigned int)blocks,
                                nearest_constants);

                        dst += blocks * 4;
                        contiguous += blocks * 4;
                        n -= blocks * 4;
                    }

                    while (n--)
                    {
                        uint32_t pixel = *contiguous++;
                        if (opaque)
                            pixel |= 0xff000000u;
                        nearest_over_pixel (dst++, pixel);
                    }
                    continue;
                }
            }
        }

        if (n < 32 || alias)
        {
            while (n--)
            {
                int sx = pixman_fixed_to_int (vx);
                uint32_t s = nearest_fetch (src, src_repeat,
                                            sx, src_width, opaque);
                nearest_over_pixel (dst++, s);
                vx += unit_x;
            }
            continue;
        }

        /* Destination must be 16-byte aligned before SQ. */
        while (n && ((uintptr_t)dst & 15))
        {
            int sx = pixman_fixed_to_int (vx);
            uint32_t s = nearest_fetch (src, src_repeat,
                                        sx, src_width, opaque);
            nearest_over_pixel (dst++, s);
            vx += unit_x;
            --n;
        }

        while (n >= 4)
        {
            size_t count = n >= 64 ? 64 : (n & ~(size_t)3);

            for (i = 0; i < (int)count; ++i)
            {
                int sx = pixman_fixed_to_int (vx);
                staged[i] = nearest_fetch (src, src_repeat,
                                            sx, src_width, opaque);
                vx += unit_x;
            }

            ee_mmi_over_quads (dst, staged, (unsigned int)(count / 4),
                               nearest_constants);
            dst += count;
            n -= count;
        }

        while (n--)
        {
            int sx = pixman_fixed_to_int (vx);
            uint32_t s = nearest_fetch (src, src_repeat,
                                        sx, src_width, opaque);
            nearest_over_pixel (dst++, s);
            vx += unit_x;
        }
    }
}
