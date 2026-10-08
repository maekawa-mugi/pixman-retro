/* API-level EE MMI vs generic-C benchmark (same binary, separate runs).
 * Compile within Meson with the same Pixman used by the application.
 */
#include <pixman.h>
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
static unsigned long us(void)
{
    struct timeval tv;
    gettimeofday(&tv,0);
    return (unsigned long)tv.tv_sec*1000000ul+(unsigned long)tv.tv_usec;
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
    unsigned int loops,i,area=(unsigned int)width*(unsigned int)height;
    int format=0;
    unsigned long start,elapsed;
    uint32_t hash;
    initialize_dest();

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

    loops=131072u/area;
    if(loops<8)loops=8;
    if(loops>2048)loops=2048;
    start=us();
    for(i=0;i<loops;i++)
    {
        if(kind==B_BLT)
        {
            if(!pixman_blt(src32,dst32,W,W,32,32,0,0,0,0,width,height))
                break;
        }
        else if(kind==B_FILL)
        {
            if(!pixman_fill(dst32,W,32,0,0,width,height,0x80402010u))
                break;
        }
        else
            pixman_image_composite32(op,src,mask,dst,0,0,0,0,0,0,
                                     width,height);
    }
    elapsed=us()-start;
    hash=checksum(format);
    if(src)pixman_image_unref(src);
    if(mask)pixman_image_unref(mask);
    if(dst)pixman_image_unref(dst);
    if(i!=loops)
    {
        fprintf(stderr,"FAIL Pixman execution: %s\n",names[kind]);
        return 1;
    }
    printf("PIXMAN_BENCH,%s,%d,%d,%u,%lu,%08x\n",
           names[kind],width,height,loops,elapsed,hash);
    return 0;
}
int main(void)
{
    static const int widths[3]={16,64,256};
    static const int heights[3]={2,16,64};
    int kind,j,failed=0;
    initialize_inputs();
    puts("PIXMAN_BENCH_CSV,kernel,width,height,iterations,us,checksum");
    for(kind=0;kind<B_COUNT;kind++)
        for(j=0;j<3;j++)
            failed+=measure_case(kind,widths[j],heights[j]);
    printf("PIXMAN_BENCH_RESULT,%s,failures=%d\n",
           failed?"FAIL":"PASS",failed);
    return failed?1:0;
}
