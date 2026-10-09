/* PS2 EE regression runner. C reference and real MMI live in one ELF. */
#include "progress.h"
#include <pixman-config.h>
#include "pixman-private.h"
#include <stdlib.h>
#ifndef PS2_TEST_HOST
#include <debug.h>
#include <kernel.h>
#else
#define scr_printf(...) ((void)0)
#endif

#define STRIDE 560
#define ROWS 4
#define STORAGE (STRIDE * ROWS + 64)
#define TEST_ALIGNED __attribute__((aligned(16)))
static uint8_t source[STORAGE] TEST_ALIGNED, mask_bits[STORAGE] TEST_ALIGNED;
static uint8_t b_actual[STORAGE] TEST_ALIGNED;
static uint8_t actual[STORAGE] TEST_ALIGNED, expected[STORAGE] TEST_ALIGNED;
static pixman_implementation_t *reference, *accelerated, *generic_c;
static uint64_t case_a_us, case_b_us;
static int compare_b, case_a_error, case_b_error;
static unsigned int cases, kernel_calls[16];
static uint32_t seed = 0x5eed1234;

/* Linker wrappers prove public tests reach assembly rather than only C tails. */
#define WRAP3(name, index) \
    void __real_##name(void *, const void *, unsigned int); \
    void b_##name(void *, const void *, unsigned int); \
    void __wrap_##name(void *d, const void *s, unsigned int n) \
    { if (ps2_kernel_variant()) b_##name(d,s,n); else { if(n)++kernel_calls[index]; __real_##name(d,s,n); } }
#define WRAP4(name, index) \
    void __real_##name(void *, const void *, unsigned int, const void *); \
    void b_##name(void *, const void *, unsigned int, const void *); \
    void __wrap_##name(void *d, const void *s, unsigned int n, const void *c) \
    { if(ps2_kernel_variant())b_##name(d,s,n,c);else {if(n)++kernel_calls[index];__real_##name(d,s,n,c);} }
WRAP3(ee_mmi_add_quads, 0)
WRAP3(ee_mmi_copy_quads, 1)
WRAP3(ee_mmi_fill_quads, 2)
WRAP4(ee_mmi_in_a8_quads, 3)
WRAP4(ee_mmi_over_quads, 4)
WRAP4(ee_mmi_8888_to_0565_quads, 5)
WRAP4(ee_mmi_ca_over_quads, 6)

#define WRAP5N4(name, index) \
    void __real_##name(void *, const void *, const void *, unsigned int, const void *); \
    void b_##name(void *, const void *, const void *, unsigned int, const void *); \
    void __wrap_##name(void *d, const void *s, const void *m, unsigned int n, const void *c) \
    {if(ps2_kernel_variant())b_##name(d,s,m,n,c);else {if(n)++kernel_calls[index];__real_##name(d,s,m,n,c);} }
#define WRAP5N5(name, index) \
    void __real_##name(void *, const void *, const void *, const void *, unsigned int); \
    void b_##name(void *, const void *, const void *, const void *, unsigned int); \
    void __wrap_##name(void *d, const void *s, const void *m, const void *c, unsigned int n) \
    {if(ps2_kernel_variant())b_##name(d,s,m,c,n);else {if(n)++kernel_calls[index];__real_##name(d,s,m,c,n);} }
WRAP4(ee_mmi_ca_over_solid_quads, 7)
WRAP4(ee_mmi_over_solid_a8_quads, 8)
WRAP5N4(ee_mmi_over_image_a8_quads, 9)
WRAP5N4(ee_mmi_over_image_ca_quads, 10)
WRAP4(ee_mmi_over_8888_0565_quads, 11)
WRAP4(ee_mmi_nearest_opaque_quads, 12)
WRAP5N5(ee_mmi_bilinear_lerp_quads, 13)
WRAP5N5(ee_mmi_bilinear_hsum_quads, 14)
WRAP5N5(ee_mmi_bilinear_vfinish_quads, 15)


static uint32_t random_word(void)
{
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
    return seed;
}

static uint32_t pixel(unsigned int i)
{
    static const unsigned int alpha[] = {0, 1, 127, 254, 255};
    unsigned int a = alpha[i % 5];
    uint32_t v = random_word();
    return (a << 24) | (((v >> 16) & 255) * a / 255 << 16) |
           (((v >> 8) & 255) * a / 255 << 8) | ((v & 255) * a / 255);
}

static void prepare(void)
{
    unsigned int i;
    for (i = 0; i < STORAGE / 4; ++i)
    {
        ((uint32_t *)source)[i] = pixel(i);
        ((uint32_t *)actual)[i] = pixel(i + 2);
        ((uint32_t *)mask_bits)[i] = random_word();
    }
    memcpy(expected, actual, STORAGE);
    memcpy(b_actual, actual, STORAGE);
    case_a_us=case_b_us=0; compare_b=0; case_a_error=case_b_error=0;
}

static int compare_buffer(const char *name, const char *side, const uint8_t *bits)
{
    unsigned int i;
    for(i=0;i<STORAGE;++i)
        if(bits[i]!=expected[i])
        {
            printf("%s side=%s case=%u byte=%u got=%02x expected=%02x seed=%08lx\n",
                name,side,cases,i,bits[i],expected[i],(unsigned long)seed);
            fflush(stdout); return 1;
        }
    return 0;
}
static int compare(const char *name)
{
    int bad_a, bad_b;
    ++cases;
    bad_a=case_a_error || compare_buffer(name,"A",actual);
    bad_b=case_b_error || (compare_b && compare_buffer(name,"B",b_actual));
    ps2_progress_record(case_a_us,case_b_us,bad_a,bad_b);
    return bad_a || bad_b;
}

/* Directly call general C to avoid the global fast-path cache reusing an
 * accelerated function when switching implementations. Test rectangles stay
 * inside the destination; there are no user clip regions in this runner. */
static void composite_reference(pixman_op_t op, pixman_image_t *src,
                                pixman_image_t *mask, pixman_image_t *dst,
                                int width)
{
    pixman_composite_info_t info;
    memset(&info, 0, sizeof(info));
    _pixman_image_validate(src);
    if (mask) _pixman_image_validate(mask);
    _pixman_image_validate(dst);
    info.op = op; info.src_image = src; info.mask_image = mask;
    info.dest_image = dst; info.width = width; info.height = ROWS;
    info.src_flags = src->common.flags;
    info.mask_flags = mask ? mask->common.flags :
                            FAST_PATH_IS_OPAQUE | FAST_PATH_NO_ALPHA_MAP;
    info.dest_flags = dst->common.flags;
    reference->fast_paths[0].func(reference, &info);
}

static int composite_case(pixman_op_t op, pixman_format_code_t sf,
                          pixman_format_code_t df, int width, int offset,
                          int negative, int mask_kind, int solid, int scale)
{
    int stride = negative ? -STRIDE : STRIDE;
    int base = 16 + offset + (negative ? STRIDE * (ROWS - 1) : 0);
    pixman_color_t color = {0xaaaa, 0x5555, 0x1111, 0xcccc};
    pixman_image_t *s, *m = NULL, *a, *e, *b;
    uint64_t start;
    pixman_transform_t transform;
    int result;
    prepare();
    s = solid ? pixman_image_create_solid_fill(&color) :
        pixman_image_create_bits(sf, 128, ROWS, (uint32_t *)(source + base), stride);
    a = pixman_image_create_bits(df, 128, ROWS, (uint32_t *)(actual + base), stride);
    e = pixman_image_create_bits(df, 128, ROWS, (uint32_t *)(expected + base), stride);
    b = pixman_image_create_bits(df,128,ROWS,(uint32_t *)(b_actual + base),stride);
    if (mask_kind)
    {
        m = pixman_image_create_bits(mask_kind == 1 ? PIXMAN_a8 : sf,
                                    128, ROWS, (uint32_t *)(mask_bits + base), stride);
        if (m && mask_kind == 2) pixman_image_set_component_alpha(m, TRUE);
    }
    if (!s || !a || !e || !b || (mask_kind && !m))
    {
        printf("image allocation failed\n");
        result = 1;
        goto out;
    }
    if (scale)
    {
        pixman_transform_init_identity(&transform);
        transform.matrix[0][0] = scale & 8 ? -pixman_fixed_1 / 2 : pixman_fixed_1 / 2;
        transform.matrix[0][2] = -pixman_fixed_1 / 4;
        pixman_image_set_transform(s, &transform);
        pixman_image_set_repeat(s, (pixman_repeat_t)((scale - 1) & 3));
        pixman_image_set_filter(s, scale & 4 ? PIXMAN_FILTER_BILINEAR :
                                            PIXMAN_FILTER_NEAREST, NULL, 0);
    }
    composite_reference(op, s, m, e, width);
    global_implementation = accelerated;
    start=ps2_ticks_now();
    pixman_image_composite32(op,s,m,a,0,0,0,0,0,0,width,ROWS);
    case_a_us=ps2_elapsed_us(start);
    global_implementation = generic_c;
    start=ps2_ticks_now();
    pixman_image_composite32(op,s,m,b,0,0,0,0,0,0,width,ROWS);
    case_b_us=ps2_elapsed_us(start);
    global_implementation = accelerated;
    compare_b=1;
    /* The stored X byte is undefined for XRGB/XBGR. Compare every other
     * byte, including all row padding and guards outside the rectangle. */
    if (df == PIXMAN_x8r8g8b8 || df == PIXMAN_x8b8g8r8)
    {
        int row, x;
        for (row = 0; row < ROWS; ++row)
        for (x = 0; x < width; ++x)
        {
            int at=base+row*stride+x*4+3;
            expected[at]=actual[at]; b_actual[at]=actual[at];
        }
    }
    result = compare("composite");
    if (result)
        printf("op=%d sf=%08x df=%08x width=%d offset=%d neg=%d mask=%d solid=%d scale=%d\n",
               op, sf, df, width, offset, negative, mask_kind, solid, scale);
out:
    if (s) pixman_image_unref(s);
    if (m) pixman_image_unref(m);
    if (a) pixman_image_unref(a);
    if (e) pixman_image_unref(e);
    if (b) pixman_image_unref(b);
    return result;
}

static int composites(int group)
{
    static const int widths[] = {1, 2, 3, 4, 15, 16, 17, 63, 64, 65, 127};
    static const pixman_format_code_t formats[] = {
        PIXMAN_a8r8g8b8, PIXMAN_a8b8g8r8, PIXMAN_x8r8g8b8, PIXMAN_x8b8g8r8
    };
    int w, o, n, f, k, s;
    for (w = 0; w < 11; ++w)
    for (o = 0; o < 16; o += 4)
    for (n = 0; n < 2; ++n)
    {
        if (group == 0)
        {
            static const pixman_op_t ops[] = {PIXMAN_OP_ADD, PIXMAN_OP_IN, PIXMAN_OP_SRC};
            for (k = 0; k < 3; ++k)
                if (composite_case(ops[k], PIXMAN_a8, PIXMAN_a8, widths[w], o, n, 0, 0, 0)) return 1;
        }
        else for (f = 0; f < 4; ++f)
        {
            pixman_format_code_t sf = formats[f];
            if (group == 1)
            {
                static const pixman_op_t ops[] = {PIXMAN_OP_ADD, PIXMAN_OP_SRC, PIXMAN_OP_OVER};
                for (k = 0; k < 3; ++k)
                {
                    if (k == 0 && f > 1) continue;
                    if (composite_case(ops[k], sf, sf, widths[w], o, n, 0, 0, 0)) return 1;
                }
            }
            else if (group == 2)
            {
                for (s = 0; s < 2; ++s)
                for (k = 0; k < 3; ++k)
                {
                    if (k == 2 && (!s || f > 1)) continue;
                    if (composite_case(PIXMAN_OP_OVER, sf, sf, widths[w], o, n, k, s, 0)) return 1;
                }
            }
            else if (group == 3)
            {
                pixman_format_code_t df = f & 1 ? PIXMAN_b5g6r5 : PIXMAN_r5g6b5;
                if (composite_case(PIXMAN_OP_SRC, sf, df, widths[w], o, n, 0, 0, 0)) return 1;
                if (f < 2 && composite_case(PIXMAN_OP_OVER, sf, df, widths[w], o, n, 0, 0, 0)) return 1;
                if (composite_case(PIXMAN_OP_SRC, df, df, widths[w], o, n, 0, 0, 0)) return 1;
            }
            else
                for (k = 1; k <= 16; ++k)
                    if (composite_case(PIXMAN_OP_OVER, sf, sf, widths[w], o, n, 0, 0, k)) return 1;
        }
    }
    return 0;
}

static int in_exhaustive(void)
{
    extern void ee_mmi_in_a8_quads(uint8_t *, const uint8_t *, unsigned int, const uint16_t *);
    static const uint16_t rounding[8] TEST_ALIGNED = {128,128,128,128,128,128,128,128};
    unsigned int s, d;
    uint64_t start;
    for (s = 0; s < 256; ++s)
    {
        prepare();
        for (d = 0; d < 256; ++d)
        {
            source[16 + d] = s; actual[16 + d] = d;
        }
        start=ps2_ticks_now();
        for(d=0;d<256;++d) expected[16+d]=(s*d+127)/255;
        case_b_us=ps2_elapsed_us(start);
        start=ps2_ticks_now();
        ee_mmi_in_a8_quads(actual + 16, source + 16, 16, rounding);
        case_a_us=ps2_elapsed_us(start);
        if (compare("IN exhaustive")) return 1;
    }
    return 0;
}

static int copy_with_fallback(pixman_implementation_t *imp, uint8_t *src,
    uint8_t *dst, int stride, int bpp, int sx, int sy, int dx, int dy, int width, int height)
{
    int row, step=1, first=0, last=height;
    if(_pixman_implementation_blt(imp,(uint32_t *)src,(uint32_t *)dst,
        stride/4,stride/4,bpp,bpp,sx,sy,dx,dy,width,height))return 1;
    if(src==dst && dy>sy){first=height-1;last=-1;step=-1;}
    for(row=first;row!=last;row+=step)
        memmove(dst+(dy+row)*stride+dx*(bpp/8),
                src+(sy+row)*stride+sx*(bpp/8),width*(bpp/8));
    return 1;
}
static int transfers(void)
{
    int bpp,o,n,w,row;
    uint64_t start;
    static const int widths[]={1,15,16,17,63,64,65,127};
    for(bpp=8;bpp<=32;bpp*=2)
    for(o=0;o<16;++o)
    for(n=0;n<2;++n)
    for(w=0;w<8;++w)
    {
        int stride=n?-STRIDE:STRIDE;
        int base=16+(n?STRIDE*(ROWS-1):0);
        uint32_t color=0xa5c3b17f;
        prepare();
        for(row=0;row<ROWS;++row)
        {
            int x;
            for(x=0;x<widths[w];++x)
                memcpy(expected+base+row*stride+(x+o)*(bpp/8),&color,bpp/8);
        }
        start=ps2_ticks_now();
        case_a_error=!_pixman_implementation_fill(accelerated,(uint32_t *)(actual+base),
            stride/4,bpp,o,0,widths[w],ROWS,color);
        case_a_us=ps2_elapsed_us(start);
        start=ps2_ticks_now();
        case_b_error=!_pixman_implementation_fill(generic_c,(uint32_t *)(b_actual+base),
            stride/4,bpp,o,0,widths[w],ROWS,color);
        case_b_us=ps2_elapsed_us(start);compare_b=1;
        if(compare("fill"))return 1;
        prepare();
        for(row=0;row<ROWS;++row)
            memcpy(expected+base+row*stride+o*(bpp/8),source+base+row*stride+o*(bpp/8),widths[w]*(bpp/8));
        start=ps2_ticks_now();
        copy_with_fallback(accelerated,source+base,actual+base,stride,bpp,o,0,o,0,widths[w],ROWS);
        case_a_us=ps2_elapsed_us(start);
        start=ps2_ticks_now();
        copy_with_fallback(generic_c,source+base,b_actual+base,stride,bpp,o,0,o,0,widths[w],ROWS);
        case_b_us=ps2_elapsed_us(start);compare_b=1;
        if(compare("blt"))return 1;
        prepare();
        for(row=ROWS-2;row>=0;--row)
            memmove(expected+base+(row+1)*stride+(o+1)*(bpp/8),
                    expected+base+row*stride+o*(bpp/8),widths[w]*(bpp/8));
        start=ps2_ticks_now();
        copy_with_fallback(accelerated,actual+base,actual+base,stride,bpp,o,0,o+1,1,widths[w],ROWS-1);
        case_a_us=ps2_elapsed_us(start);
        start=ps2_ticks_now();
        copy_with_fallback(generic_c,b_actual+base,b_actual+base,stride,bpp,o,0,o+1,1,widths[w],ROWS-1);
        case_b_us=ps2_elapsed_us(start);compare_b=1;
        if(compare("blt overlap"))return 1;
    }
    return 0;
}

static int combiner(void)
{
    int n, o, m;
    uint64_t start;
    for (n = 1; n <= 128; ++n)
    for (o = 0; o < 4; ++o)
    for (m = 0; m < 2; ++m)
    {
        pixman_combine_32_func_t c = reference->combine_32[PIXMAN_OP_OVER];
        pixman_combine_32_func_t ee = accelerated->combine_32[PIXMAN_OP_OVER];
        prepare();
        c(reference, PIXMAN_OP_OVER, (uint32_t *)expected + o, (uint32_t *)source + o,
          m ? (uint32_t *)mask_bits : NULL, n);
        start=ps2_ticks_now();
        ee(accelerated,PIXMAN_OP_OVER,(uint32_t *)actual+o,(uint32_t *)source+o,
           m?(uint32_t *)mask_bits:NULL,n);
        case_a_us=ps2_elapsed_us(start);
        {
            pixman_combine_func_t f=_pixman_implementation_lookup_combiner(generic_c,PIXMAN_OP_OVER,FALSE,TRUE);
            start=ps2_ticks_now();
            f.f32(generic_c,PIXMAN_OP_OVER,(uint32_t *)b_actual+o,(uint32_t *)source+o,
                  m?(uint32_t *)mask_bits:NULL,n);
            case_b_us=ps2_elapsed_us(start);compare_b=1;
        }
        if (compare("combiner")) return 1;
    }
    return 0;
}

static int run(const char *name,int group,int (*test)(void))
{
    unsigned int before=cases;
    uint64_t start;
    int fail;
    ps2_progress_begin_group(name);
    printf("%s RUNNING\n",name);fflush(stdout);
    start=ps2_ticks_now();
    fail=test?test():composites(group);
#ifdef PS2_TEST_HOST
    /* Negative control: fail after every pixel comparison has passed. */
    if (getenv("PS2_PROGRESS_FORCE_GROUP_FAILURE") &&
        !strcmp(name, "Scaled OVER"))
        fail = 1;
#endif
    ps2_progress_group(name,fail,ps2_elapsed_us(start));
    printf("%s %s cases=%u\n",name,fail?"FAIL":"PASS",cases-before);
    fflush(stdout);
    return fail;
}
extern int ps2_benchmark_run(pixman_implementation_t *accelerated);
static int timed_api_cases(void)
{ return ps2_benchmark_run(accelerated); }

unsigned int ps2_mmi_call_count(void)
{
    unsigned int total = 0;
    int i;
    for (i = 0; i < 16; ++i) total += kernel_calls[i];
    return total;
}
extern int ee_kernel_suite_main(int argc, char **argv);
extern int ps2_latency_run(void);
extern int ps2_throughput_run(void);

int main(void)
{
    int failures = 0, i;
#ifndef PS2_TEST_HOST
    init_scr();
#endif
    ps2_progress_init();
    {
        char *args[] = {"ee-mmi-suite", "--test-only", NULL};

        i = ee_kernel_suite_main(2, args);
        failures += i;

    }
    /* A/B/C experiment does not alter production kernel dispatch. */
    failures += ps2_latency_run();
    failures += ps2_throughput_run();
    /* Keep two entirely separate chains: reference always uses C combiners. */
    reference = _pixman_implementation_create_general();
    _pixman_implementation_create_noop(_pixman_implementation_create_fast_path(reference));
    accelerated = _pixman_implementation_create_ee_mmi(
        _pixman_implementation_create_fast_path(_pixman_implementation_create_general()));
    generic_c=_pixman_implementation_create_noop(
        _pixman_implementation_create_fast_path(_pixman_implementation_create_general()));
    if (!reference || !accelerated || !generic_c) return 1;
    global_implementation = accelerated;
    failures += run("Large / scaled API",0,timed_api_cases);
    failures += run("IN 65536 pairs", 0, in_exhaustive);
    failures += run("A8 ADD/IN/SRC", 0, NULL);
    failures += run("8888 blending", 1, NULL);
    failures += run("Masks/solid/CA", 2, NULL);
    failures += run("RGB/BGR565", 3, NULL);
    failures += run("Scaled OVER", 4, NULL);
    failures += run("Fill/BLT", 0, transfers);
    failures += run("OVER combiner", 0, combiner);
    for (i = 0; i < 16; ++i)
    {
        printf("MMI kernel %d calls=%u\n", i, kernel_calls[i]);
        /* Kernel 6 is the legacy staged CA path, replaced by fused CA. */
        if (i != 6 && !kernel_calls[i]) ++failures;
    }
    ps2_progress_finish(failures);
    printf("TEST: %s! failures=%d cases=%u\n", failures ? "FAIL" : "OK", failures, cases);
    fflush(stdout);
#ifndef PS2_TEST_HOST
    SleepThread();
#endif
    return failures != 0;
}
