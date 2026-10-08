/*
 * PS2 EE MMI 32-bit scaled bilinear OVER.
 *
 * Keep Pixman's exact bilinear_interpolation function, bilinear weight
 * extraction, and fixed-point transform. Feed 64-pixel sampled blocks
 * to the R5900 128-bit PMULTH/PADDUB alpha compositor. All repeat modes
 * use Pixman's repeat() helper independently for the four filter taps.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions: the above copyright notice and this
 * permission notice shall be included in all copies or substantial
 * portions of the Software.
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

extern void ee_mmi_bilinear_lerp_quads (uint32_t *dst,
                                         const uint32_t *first,
                                         const uint32_t *second,
                                         const uint16_t *weights,
                                         unsigned int blocks);

static const uint16_t bilinear_constants[16] __attribute__((aligned(16))) =
{
    255, 255, 255, 255, 255, 255, 255, 255,
    128, 128, 128, 128, 128, 128, 128, 128
};

static force_inline uint32_t
bilinear_sample_one (const uint32_t *src_base, int stride,
                     int width, int height, pixman_repeat_t mode,
                     int x, int y, pixman_bool_t source_opaque)
{
    if (!repeat (mode, &x, width) || !repeat (mode, &y, height))
        return 0;

    if (source_opaque)
        return src_base[(ptrdiff_t)y * stride + x] | 0xff000000;

    return src_base[(ptrdiff_t)y * stride + x];
}

static force_inline uint32_t
bilinear_fetch (const uint32_t *src_base, int stride,
                int width, int height, pixman_repeat_t mode,
                pixman_fixed_t vx, pixman_fixed_t vy,
                pixman_bool_t source_opaque)
{
    int x1 = pixman_fixed_to_int (vx);
    int y1 = pixman_fixed_to_int (vy);
    int distx = pixman_fixed_to_bilinear_weight (vx);
    int disty = pixman_fixed_to_bilinear_weight (vy);
    int x2 = distx ? x1 + 1 : x1;
    int y2 = disty ? y1 + 1 : y1;
    uint32_t tl, tr, bl, br;

    tl = bilinear_sample_one (src_base, stride, width, height,
                              mode, x1, y1, source_opaque);
    tr = bilinear_sample_one (src_base, stride, width, height,
                              mode, x2, y1, source_opaque);
    bl = bilinear_sample_one (src_base, stride, width, height,
                              mode, x1, y2, source_opaque);
    br = bilinear_sample_one (src_base, stride, width, height,
                              mode, x2, y2, source_opaque);

    return bilinear_interpolation (tl, tr, bl, br, distx, disty);
}

/* Exact 2-tap specialization, used only when one bilinear axis has
 * zero fractional weight. Pixman's fixed-point weight is 7 bits; double
 * it to 8 bits, so
 *
 *   (a * (256 - 2*w) + b * (2*w)) >> 8
 *
 * equals the full Pixman four-tap bilinear_interpolation bit for bit.
 * The general case still calls Pixman's four-tap scalar interpolation.
 */
static force_inline void
bilinear_fetch_two_taps (const uint32_t *src_base, int stride,
                         int width, int height, pixman_repeat_t mode,
                         pixman_fixed_t vx, pixman_fixed_t vy,
                         pixman_bool_t source_opaque,
                         pixman_bool_t vertical,
                         uint32_t *first, uint32_t *second,
                         unsigned int *second_weight)
{
    int x = pixman_fixed_to_int (vx);
    int y = pixman_fixed_to_int (vy);
    int dist = vertical ? pixman_fixed_to_bilinear_weight (vy) :
                          pixman_fixed_to_bilinear_weight (vx);
    int next_x = vertical ? x : (dist ? x + 1 : x);
    int next_y = vertical && dist ? y + 1 : y;

    *first = bilinear_sample_one (src_base, stride, width, height, mode,
                                  x, y, source_opaque);
    *second = bilinear_sample_one (src_base, stride, width, height, mode,
                                   next_x, next_y, source_opaque);
    *second_weight = (unsigned int)dist << 1;
}

/* The same scalar premultiplied OVER calculation as Pixman's fast path. */
static force_inline void
bilinear_over_pixel (uint32_t *dst, uint32_t s)
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

void
ee_mmi_composite_bilinear_over_8888 (pixman_implementation_t *imp,
                                     pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint32_t *src_base, *dst_line;
    int src_stride, dst_stride;
    pixman_vector_t v;
    pixman_fixed_t unit_x, unit_y, vy;
    pixman_repeat_t repeat_mode;
    pixman_bool_t source_opaque, alias;
    int src_width, src_height;
    uint32_t staged[64] __attribute__((aligned(16)));
    uint32_t tap_first[16] __attribute__((aligned(16)));
    uint32_t tap_second[16] __attribute__((aligned(16)));
    uint16_t tap_weights[128] __attribute__((aligned(16)));

    PIXMAN_IMAGE_GET_LINE (src_image, 0, 0, uint32_t,
                           src_stride, src_base, 1);
    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint32_t,
                           dst_stride, dst_line, 1);

    v.vector[0] = pixman_int_to_fixed (src_x) + pixman_fixed_1 / 2;
    v.vector[1] = pixman_int_to_fixed (src_y) + pixman_fixed_1 / 2;
    v.vector[2] = pixman_fixed_1;
    if (!pixman_transform_point_3d (src_image->common.transform, &v))
        return;

    unit_x = src_image->common.transform->matrix[0][0];
    unit_y = src_image->common.transform->matrix[1][1];
    v.vector[0] -= pixman_fixed_1 / 2;
    v.vector[1] -= pixman_fixed_1 / 2;

    src_width = src_image->bits.width;
    src_height = src_image->bits.height;
    repeat_mode = src_image->common.repeat;
    source_opaque = (src_image->bits.format == PIXMAN_x8r8g8b8 ||
                     src_image->bits.format == PIXMAN_x8b8g8r8);
    alias = (src_image->bits.bits == dest_image->bits.bits);

    /* A zero-sized source cannot contribute to OVER. */
    if (src_width <= 0 || src_height <= 0)
        return;

    vy = v.vector[1];
    while (height-- > 0)
    {
        uint32_t *dst = dst_line;
        size_t n = (size_t)width;
        pixman_fixed_t vx = v.vector[0];
        int i;
        int disty = pixman_fixed_to_bilinear_weight (vy);
        pixman_bool_t two_tap_vertical =
            disty != 0 &&
            pixman_fixed_to_bilinear_weight (vx) == 0 &&
            (unit_x & (pixman_fixed_1 - 1)) == 0;
        pixman_bool_t two_tap = disty == 0 || two_tap_vertical;

        dst_line += dst_stride;

        if (n < 32 || alias)
        {
            while (n--)
            {
                uint32_t s = bilinear_fetch (src_base, src_stride,
                                             src_width, src_height,
                                             repeat_mode, vx, vy,
                                             source_opaque);
                bilinear_over_pixel (dst++, s);
                vx += unit_x;
            }
        }
        else
        {
            while (n && ((uintptr_t)dst & 15))
            {
                uint32_t s = bilinear_fetch (src_base, src_stride,
                                             src_width, src_height,
                                             repeat_mode, vx, vy,
                                             source_opaque);
                bilinear_over_pixel (dst++, s);
                vx += unit_x;
                --n;
            }

            while (n >= 4)
            {
                size_t count;

                if (two_tap)
                {
                    /* At most four quads: 2 x 64B taps + 256B
                     * coefficients, fitting in EE's small D-cache.
                     * The existing MMI OVER stage composes the result.
                     */
                    count = n >= 16 ? 16 : (n & ~(size_t)3);
                    for (i = 0; i < (int)count; ++i)
                    {
                        unsigned int w;
                        size_t index = ((size_t)i / 4) * 32 +
                                       ((i & 2) ? 8 : 0) +
                                       ((i & 1) ? 4 : 0);
                        int j;

                        bilinear_fetch_two_taps (
                            src_base, src_stride, src_width, src_height,
                            repeat_mode, vx, vy, source_opaque,
                            two_tap_vertical,
                            &tap_first[i], &tap_second[i], &w);

                        for (j = 0; j < 4; ++j)
                        {
                            tap_weights[index + j] = (uint16_t)(256 - w);
                            tap_weights[index + 16 + j] = (uint16_t)w;
                        }
                        vx += unit_x;
                    }
                    ee_mmi_bilinear_lerp_quads (
                        staged, tap_first, tap_second, tap_weights,
                        (unsigned int)(count / 4));
                }
                else
                {
                    /* Keep the bit-exact generic four-tap interpolation
                     * for arbitrary 2D scaling and edge/repeat modes.
                     */
                    count = n >= 64 ? 64 : (n & ~(size_t)3);
                    for (i = 0; i < (int)count; ++i)
                    {
                        staged[i] = bilinear_fetch (
                            src_base, src_stride, src_width, src_height,
                            repeat_mode, vx, vy, source_opaque);
                        vx += unit_x;
                    }
                }

                ee_mmi_over_quads (dst, staged, (unsigned int)(count / 4),
                                   bilinear_constants);
                dst += count;
                n -= count;
            }

            while (n--)
            {
                uint32_t s = bilinear_fetch (src_base, src_stride,
                                             src_width, src_height,
                                             repeat_mode, vx, vy,
                                             source_opaque);
                bilinear_over_pixel (dst++, s);
                vx += unit_x;
            }
        }
        vy += unit_y;
    }
}
