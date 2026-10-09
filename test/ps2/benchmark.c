/* API-level EE MMI vs generic-C benchmark (same binary, separate runs).
 * Compile within Meson with the same Pixman used by the application.
 */
#include "progress.h"
#include <pixman-config.h>
#include "pixman-private.h"
#include <stdlib.h>
#include <string.h>
#ifndef PS2_TEST_HOST
#include <debug.h>
#include <timer.h>
#else
#define scr_printf(...) ((void)0)
#define scr_setfontcolor(c) ((void)0)
#define scr_setXY(x,y) ((void)0)
#define scr_clear() ((void)0)
#endif
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>

#define W 256
#define H 64
#define COUNT (W*H)
static uint32_t src32[COUNT] __attribute__((aligned(16)));
static uint32_t dst32[COUNT] __attribute__((aligned(16)));
static uint32_t ca32[COUNT] __attribute__((aligned(16)));
static uint16_t dst565[COUNT] __attribute__((aligned(16)));
static uint8_t mask8[COUNT] __attribute__((aligned(16)));
static uint8_t src8[COUNT] __attribute__((aligned(16)));
static uint8_t dst8[COUNT] __attribute__((aligned(16)));
static uint32_t seed=0x57d318cdu;
static uint32_t rng(void)
{
    uint32_t x=seed;x^=x<<13;x^=x>>17;x^=x<<5;return seed=x;
}
extern unsigned int ps2_mmi_call_count(void);
static pixman_implementation_t *impl_a, *impl_b, *oracle;
static uint8_t correct_bytes[COUNT * 4];

static unsigned int bad_rows[2], completed;
static unsigned int false_mmi_calls;
static size_t result_size(int format)
{ return COUNT * (format == 1 ? 2 : format == 2 ? 1 : 4); }
static void *result_bits(int format)
{ return format == 1 ? (void *)dst565 : format == 2 ? (void *)dst8 : (void *)dst32; }

/* Oracle bypasses API fast-path lookup and owns a C-only combiner chain. */
static void oracle_composite(pixman_op_t op, pixman_image_t *src,
                             pixman_image_t *mask, pixman_image_t *dst,
                             int width, int height)
{
    pixman_composite_info_t info;
    memset(&info, 0, sizeof(info));
    _pixman_image_validate(src);
    if (mask) _pixman_image_validate(mask);
    _pixman_image_validate(dst);
    info.op = op; info.src_image = src; info.mask_image = mask;
    info.dest_image = dst; info.width = width; info.height = height;
    info.src_flags = src->common.flags;
    info.mask_flags = mask ? mask->common.flags :
        FAST_PATH_IS_OPAQUE | FAST_PATH_NO_ALPHA_MAP;
    info.dest_flags = dst->common.flags;
    oracle->fast_paths[0].func(oracle, &info);
}
static void initialize_inputs(void)
{
    unsigned int i;
    for(i=0;i<COUNT;i++)
    {
        uint32_t v=rng();
        unsigned int a=(v>>24)&255,r=(v>>16)&255,g=(v>>8)&255,b=v&255;
        src32[i]=((uint32_t)a<<24)|((uint32_t)(r*a/255)<<16)|
                 ((uint32_t)(g*a/255)<<8)|(uint32_t)(b*a/255);
        ca32[i]=rng();
        mask8[i]=(uint8_t)rng();
        src8[i]=(uint8_t)rng();
    }
}
static void initialize_dest(void)
{
    unsigned int i;
    for(i=0;i<COUNT;i++)
    {
        uint32_t v=(uint32_t)(i*2654435761u+0x1234567u);
        dst32[i]=v;
        dst565[i]=(uint16_t)(v^(v>>16));
        dst8[i]=(uint8_t)v;
    }
}
static uint32_t checksum(int format)
{
    uint32_t hash=2166136261u;unsigned int i;
    for(i=0;i<COUNT;i++)
    {
        uint32_t x=format==1?dst565[i]:format==2?dst8[i]:dst32[i];
        hash=(hash^x)*16777619u;
    }
    return hash;
}
enum {
    B_OVER8888, B_IMAGE_A8, B_SOLID_A8, B_IMAGE_CA, B_SOLID_CA,
    B_OVER565, B_SRC565, B_ADD, B_SRC, B_IN_A8, B_FILL, B_BLT,
    B_NEAREST, B_BILINEAR,
    B_NEAREST_1X, B_NEAREST_XRGB_1X,
    B_BILINEAR_X_ONLY, B_BILINEAR_Y_ONLY,
    B_COUNT
};
static const char *const names[B_COUNT]={
    "OVER_8888","OVER_IMAGE_A8","OVER_SOLID_A8","OVER_IMAGE_CA",
    "OVER_SOLID_CA","OVER_RGB565","SRC_8888_565","ADD_8888",
    "SRC_8888","IN_A8","FILL_8888","BLT_8888",
    "NEAREST_OVER","BILINEAR_OVER",
    "NEAREST_1X","NEAREST_XRGB_1X",
    "BILINEAR_X_ONLY","BILINEAR_Y_ONLY"
};
static int measure_case(int kind,int width,int height)
{
    pixman_image_t *src=0,*mask=0,*dst=0;
    pixman_color_t solid_color={0x4040,0x2020,0x1010,0x8080};
    pixman_transform_t scale;
    pixman_op_t op=PIXMAN_OP_OVER;
    unsigned int loops,i;
    int format=0;
    uint64_t start, elapsed, times[2] = {0,0};
    int wrong_side[2] = {0,0};
    uint32_t hash;
    int side;
    unsigned int calls_before;
    if(kind==B_OVER565 || kind==B_SRC565)
        format=1;
    else if(kind==B_IN_A8)
        format=2;

    if(kind!=B_BLT && kind!=B_FILL)
    {
        if(kind==B_SOLID_A8 || kind==B_SOLID_CA)
            src=pixman_image_create_solid_fill(&solid_color);
        else if(kind==B_IN_A8)
            src=pixman_image_create_bits(PIXMAN_a8,W,H,(uint32_t *)src8,W);
        else
            src=pixman_image_create_bits(
                kind==B_NEAREST_XRGB_1X ? PIXMAN_x8r8g8b8 :
                                           PIXMAN_a8r8g8b8,
                W,H,src32,W*4);

        if(kind==B_IMAGE_A8 || kind==B_SOLID_A8)
            mask=pixman_image_create_bits(PIXMAN_a8,W,H,(uint32_t *)mask8,W);
        else if(kind==B_IMAGE_CA || kind==B_SOLID_CA)
        {
            mask=pixman_image_create_bits(PIXMAN_a8r8g8b8,W,H,ca32,W*4);
            if(mask) pixman_image_set_component_alpha(mask,1);
        }

        if(kind==B_OVER565 || kind==B_SRC565)
            dst=pixman_image_create_bits(PIXMAN_r5g6b5,W,H,
                                          (uint32_t *)dst565,W*2);
        else if(kind==B_IN_A8)
            dst=pixman_image_create_bits(PIXMAN_a8,W,H,(uint32_t *)dst8,W);
        else
            dst=pixman_image_create_bits(PIXMAN_a8r8g8b8,W,H,dst32,W*4);

        if(!src || !dst ||
           ((kind==B_IMAGE_A8 || kind==B_SOLID_A8 ||
             kind==B_IMAGE_CA || kind==B_SOLID_CA) && !mask))
        {
            fprintf(stderr,"FAIL Pixman allocation: %s\n",names[kind]);
            if(src)pixman_image_unref(src);
            if(mask)pixman_image_unref(mask);
            if(dst)pixman_image_unref(dst);
            return 1;
        }
        if(kind==B_NEAREST || kind==B_BILINEAR ||
           kind==B_NEAREST_1X || kind==B_NEAREST_XRGB_1X ||
           kind==B_BILINEAR_X_ONLY || kind==B_BILINEAR_Y_ONLY)
        {
            pixman_fixed_t sx=pixman_fixed_1,sy=pixman_fixed_1;
            if(kind==B_NEAREST || kind==B_BILINEAR)
                sx=sy=pixman_fixed_1/2;
            else if(kind==B_BILINEAR_X_ONLY)
                sx=pixman_fixed_1/2;
            else if(kind==B_BILINEAR_Y_ONLY)
                sy=pixman_fixed_1/2;

            pixman_transform_init_scale(&scale,sx,sy);
            pixman_image_set_transform(src,&scale);
            pixman_image_set_filter(src,
                kind==B_NEAREST || kind==B_NEAREST_1X ||
                kind==B_NEAREST_XRGB_1X ?
                PIXMAN_FILTER_NEAREST:PIXMAN_FILTER_BILINEAR,0,0);
            pixman_image_set_repeat(src,PIXMAN_REPEAT_PAD);
        }
        if(kind==B_ADD)op=PIXMAN_OP_ADD;
        else if(kind==B_SRC || kind==B_SRC565)op=PIXMAN_OP_SRC;
        else if(kind==B_IN_A8)op=PIXMAN_OP_IN;
    }

    /* Correctness and timing consume the same output; no repeated benchmark pass. */
    loops=1;
    /* Identical initial destination and iteration count for oracle, A and B. */
    for (side = -1; side < 2; ++side)
    {
        initialize_dest();
        global_implementation = side == 0 ? impl_a : impl_b;
        calls_before = ps2_mmi_call_count();
        start = ps2_ticks_now();
        for (i = 0; i < loops; ++i)
        {
            if (kind == B_BLT)
            {
                if (side == -1)
                {
                    int row;
                    for (row = 0; row < height; ++row)
                        memcpy(dst32 + row * W, src32 + row * W, width * 4);
                }
                else if (!pixman_blt(src32, dst32, W, W, 32, 32,
                                     0, 0, 0, 0, width, height))
                {
                    /* Generic Pixman can decline BLT; compare its C fallback. */
                    int row;
                    for (row = 0; row < height; ++row)
                        memcpy(dst32 + row * W, src32 + row * W, width * 4);
                }
            }
            else if (kind == B_FILL)
            {
                if (side == -1)
                {
                    int row, x;
                    for (row = 0; row < height; ++row)
                    for (x = 0; x < width; ++x)
                        dst32[row * W + x] = 0x80402010u;
                }
                else if (!pixman_fill(dst32, W, 32, 0, 0,
                                      width, height, 0x80402010u)) break;
            }
            else if (side == -1)
                oracle_composite(op, src, mask, dst, width, height);
            else
                pixman_image_composite32(op, src, mask, dst,
                                         0, 0, 0, 0, 0, 0, width, height);
        }
        elapsed = ps2_elapsed_us(start);
        if (side != 0 && ps2_mmi_call_count() != calls_before)
        {
            ++false_mmi_calls;
            fprintf(stderr, "FAIL C chain invoked MMI: %s side=%d\n", names[kind], side);
        }
        if (side == -1)
            memcpy(correct_bytes, result_bits(format), result_size(format));
        else
        {
            int wrong;
#ifdef PS2_TEST_HOST
            /* Host verification exercises red X without corrupting target builds. */
            const char *fault = getenv("PS2_BENCH_TEST_FAULT");
            if (fault && *fault == 'A' + side && kind == 0 && width == 16)
                ((uint8_t *)result_bits(format))[0] ^= 1;
#endif
            wrong = i != loops || memcmp(correct_bytes, result_bits(format), result_size(format)) != 0;
            if (wrong) ++bad_rows[side];
            times[side] = elapsed;
            wrong_side[side] = wrong;
            hash = checksum(format);
            printf("PIXMAN_AB,%c,%s,%d,%d,%u,%lu.%03lu,%c,%08lx\n",
                   'A' + side, names[kind], width, height, loops,
                   (unsigned long)(elapsed / 1000), (unsigned long)(elapsed % 1000),
                   wrong ? 'X' : 'O', (unsigned long)hash);
        }
    }
    ++completed;
    ps2_progress_record(times[0],times[1],wrong_side[0],wrong_side[1]);
    fflush(stdout);
    if(src)pixman_image_unref(src);
    if(mask)pixman_image_unref(mask);
    if(dst)pixman_image_unref(dst);
    return 0;
}
int ps2_benchmark_run(pixman_implementation_t *accelerated)

{
    static const int widths[3]={16,64,256};
    static const int heights[3]={2,16,64};
    int kind,j,failed=0;
    impl_a = accelerated;
    impl_b = _pixman_implementation_create_noop(
        _pixman_implementation_create_fast_path(_pixman_implementation_create_general()));
    oracle = _pixman_implementation_create_general();
    if (!impl_a || !impl_b || !oracle) return 1;
    _pixman_implementation_create_noop(_pixman_implementation_create_fast_path(oracle));
    seed = 0x57d318cdu;

    memset(bad_rows, 0, sizeof(bad_rows));
    completed = false_mmi_calls = 0;
    initialize_inputs();
    puts("PIXMAN_AB_CSV,side,workload,width,height,iterations,ms,result,checksum");
    for(kind=0;kind<B_COUNT;kind++)
        for(j=0;j<3;j++)
        {
            char label[64];
            snprintf(label,sizeof(label),"API %2d/54 %s %dx%d",kind*3+j+1,names[kind],widths[j],heights[j]);
            ps2_progress_stage(label);
            failed += measure_case(kind,widths[j],heights[j]);
        }
    if (failed || false_mmi_calls || completed != 54)
        ++bad_rows[0], ++bad_rows[1];

    global_implementation = impl_a;
    printf("PIXMAN_AB_RESULT,%s,A_bad=%u,B_bad=%u,C_mmi_calls=%u\n",
           bad_rows[0] || bad_rows[1] ? "FAIL" : "PASS",
           bad_rows[0], bad_rows[1], false_mmi_calls);
    fflush(stdout);
    return bad_rows[0] || bad_rows[1];
}
