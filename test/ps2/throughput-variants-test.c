/* PS2 EE MMI non-scaling A/B/C differential + throughput bench.
 * Production dispatch stays unchanged. Target linker includes original
 * assembly and test-only candidates. All data is 16B aligned.
 */
#include "progress.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CAP 4096
#define GUARD 64
#define REPEAT 256
static uint8_t src[CAP+GUARD] __attribute__((aligned(16)));
static uint8_t original_src[CAP+GUARD] __attribute__((aligned(16)));
static uint8_t dest[CAP+GUARD] __attribute__((aligned(16)));
static uint8_t oracle[CAP+GUARD] __attribute__((aligned(16)));
static const uint16_t half[8] __attribute__((aligned(16))) =
{128,128,128,128,128,128,128,128};
static const uint16_t over_c[16] __attribute__((aligned(16))) =
{255,255,255,255,255,255,255,255,128,128,128,128,128,128,128,128};
static const uint32_t c565[12] __attribute__((aligned(16))) =
{0x0000f800u,0x0000f800u,0x0000f800u,0x0000f800u,
 0x000007e0u,0x000007e0u,0x000007e0u,0x000007e0u,
 0x0000001fu,0x0000001fu,0x0000001fu,0x0000001fu};
static const uint32_t fused565_c[28] __attribute__((aligned(16))) =
{
 0x001f001fu,0x001f001fu,0x001f001fu,0x001f001fu,
 0x003f003fu,0x003f003fu,0x003f003fu,0x003f003fu,
 0x00ff00ffu,0x00ff00ffu,0x00ff00ffu,0x00ff00ffu,
 0x00800080u,0x00800080u,0x00800080u,0x00800080u,
 0x0000f800u,0x0000f800u,0x0000f800u,0x0000f800u,
 0x000007e0u,0x000007e0u,0x000007e0u,0x000007e0u,
 0x0000001fu,0x0000001fu,0x0000001fu,0x0000001fu
};
static const uint32_t fill_pattern[4] __attribute__((aligned(16))) =
{0x325a7ebdu,0x325a7ebdu,0x325a7ebdu,0x325a7ebdu};
static volatile uint32_t sink;
static uint32_t state=0x512bc393u;
static uint32_t rnd(void) {
    uint32_t v=state;v^=v<<13;v^=v>>17;v^=v<<5;return state=v;
}
static unsigned int mul255(unsigned int a,unsigned int b) {
    unsigned int v=a*b+128;return (v+(v>>8))>>8;
}
static uint32_t ref_over(uint32_t s,uint32_t d) {
    uint32_t r=0;unsigned int sh,inv=255-(s>>24);
    for(sh=0;sh<32;sh+=8) {
        unsigned int v=((s>>sh)&255)+mul255((d>>sh)&255,inv);
        if(v>255)v=255;
        r|=(uint32_t)v<<sh;
    }
    return r;
}
static uint32_t expand565(uint16_t s) {
    uint32_t d=(uint32_t)s;
    return ((d<<3)&0xf8u)|((d>>2)&7u)|
           ((d<<5)&0xfc00u)|((d>>1)&0x300u)|
           ((d<<8)&0xf80000u)|((d<<3)&0x70000u);
}
static uint16_t ref565(uint32_t p) {
    return (uint16_t)(((p>>8)&0xf800)|((p>>5)&0x07e0)|((p>>3)&0x001f));
}
typedef void (*func3)(void *,const void *,unsigned int);
typedef void (*func4)(void *,const void *,unsigned int,const void *);
extern void ee_mmi_add_quads(void *,const void *,unsigned int);
extern void ee_mmi_add_u2_quads(void *,const void *,unsigned int);
extern void ee_mmi_add_u8_quads(void *,const void *,unsigned int);
extern void ee_mmi_copy_quads(void *,const void *,unsigned int);
extern void ee_mmi_copy_u2_quads(void *,const void *,unsigned int);
extern void ee_mmi_copy_u8_quads(void *,const void *,unsigned int);
extern void ee_mmi_fill_quads(void *,const void *,unsigned int);
extern void ee_mmi_fill_u2_quads(void *,const void *,unsigned int);
extern void ee_mmi_fill_u8_quads(void *,const void *,unsigned int);
extern void ee_mmi_over_quads(void *,const void *,unsigned int,const void *);
extern void ee_mmi_over_u2_quads(void *,const void *,unsigned int,const void *);
extern void ee_mmi_in_a8_quads(void *,const void *,unsigned int,const void *);
extern void ee_mmi_in_a8_predecode_quads(void *,const void *,unsigned int,const void *);
extern void ee_mmi_8888_to_0565_quads(void *,const void *,unsigned int,const void *);
extern void ee_mmi_8888_to_0565_u2_quads(void *,const void *,unsigned int,const void *);
extern void ee_mmi_over_8888_0565_quads(void *,const void *,unsigned int,const void *);
extern void ee_mmi_over_8888_0565_u2_quads(void *,const void *,unsigned int,const void *);

enum {K_ADD,K_COPY,K_FILL,K_OVER,K_IN_A8,K_TO565,K_FUSED565,K_COUNT};
static const char *const kinds[K_COUNT]={
    "ADD","COPY","FILL","OVER_8888","IN_A8","SRC_8888_565","OVER_RGB565"
};
static const char *const labels[3]={"baseline","u2","u8"};
static const char *const labels_over[2]={"baseline","u2"};
static const char *const labels_in[2]={"baseline","predecode"};
static const func3 basic[3][3] = {
    {ee_mmi_add_quads,ee_mmi_add_u2_quads,ee_mmi_add_u8_quads},
    {ee_mmi_copy_quads,ee_mmi_copy_u2_quads,ee_mmi_copy_u8_quads},
    {ee_mmi_fill_quads,ee_mmi_fill_u2_quads,ee_mmi_fill_u8_quads}
};
static const func4 other[4][2] = {
    {ee_mmi_over_quads,ee_mmi_over_u2_quads},
    {ee_mmi_in_a8_quads,ee_mmi_in_a8_predecode_quads},
    {ee_mmi_8888_to_0565_quads,ee_mmi_8888_to_0565_u2_quads},
    {ee_mmi_over_8888_0565_quads,ee_mmi_over_8888_0565_u2_quads}
};
static unsigned int variants(int kind){return kind<=K_FILL?3u:2u;}
static const char *label(int kind,unsigned int variant){
    if(kind<=K_FILL)return labels[variant];
    if(kind==K_IN_A8)return labels_in[variant];
    return labels_over[variant];
}
static const void *constants(int kind){
    if(kind==K_FILL)return fill_pattern;
    if(kind==K_OVER)return over_c;
    if(kind==K_IN_A8)return half;
    if(kind==K_TO565)return c565;
    if(kind==K_FUSED565)return fused565_c;
    return src;
}
static void invoke(int kind,unsigned int variant,unsigned int blocks) {
    if(kind<=K_FILL)
        basic[kind][variant](dest,kind==K_FILL?fill_pattern:src,blocks);
    else
        other[kind-K_OVER][variant](dest,src,blocks,constants(kind));
}
static void prepare(int kind,unsigned int n) {
    unsigned int i;
    state=0x9a1231e5u^((uint32_t)kind*0x713ac5d3u)^(n*0x9e3779b9u);
    for(i=0;i<CAP+GUARD;i++){
        src[i]=(uint8_t)rnd();
        dest[i]=(uint8_t)rnd();
    }
    memcpy(oracle,dest,sizeof(dest));
    memcpy(original_src,src,sizeof(src));
    if(kind==K_ADD){
        for(i=0;i<16*n;i++){
            unsigned int v=dest[i]+src[i];
            oracle[i]=(uint8_t)(v>255?255:v);
        }
    }else if(kind==K_COPY)
        memcpy(oracle,src,16*n);
    else if(kind==K_FILL){
        for(i=0;i<16*n;i++)oracle[i]=((const uint8_t *)fill_pattern)[i%16];
    }else if(kind==K_OVER){
        for(i=0;i<4*n;i++)
            ((uint32_t *)oracle)[i]=ref_over(
                ((const uint32_t *)src)[i],((const uint32_t *)dest)[i]);
    }else if(kind==K_IN_A8){
        for(i=0;i<16*n;i++)
            oracle[i]=(uint8_t)mul255(src[i],dest[i]);
    }else if(kind==K_TO565){
        for(i=0;i<8*n;i++)
            ((uint16_t *)oracle)[i]=ref565(((const uint32_t *)src)[i]);
    }else if(kind==K_FUSED565){
        for(i=0;i<8*n;i++)
            ((uint16_t *)oracle)[i]=ref565(ref_over(
                 ((const uint32_t *)src)[i],
                 expand565(((const uint16_t *)dest)[i])));
    }
}
static int check(int kind,unsigned int variant,unsigned int n) {
    unsigned int i;
    prepare(kind,n);
    invoke(kind,variant,n);
#ifdef PS2_TEST_HOST
    if(getenv("PS2_THROUGHPUT_TEST_FAULT") &&
       kind==K_ADD && variant==2 && n==7)
        dest[0]^=1;
#endif
    if(memcmp(dest,oracle,sizeof(dest))){
        for(i=0;i<CAP+GUARD;i++)
            if(dest[i]!=oracle[i]){
                printf("THROUGHPUT_FAIL,%s,%s,blocks=%u,byte=%u,want=%02x,got=%02x\n",
                       kinds[kind],label(kind,variant),n,i,oracle[i],dest[i]);
                break;
            }
        return 1;
    }
    if(memcmp(src,original_src,sizeof(src))){
        printf("THROUGHPUT_FAIL,%s,%s,blocks=%u,source_corruption\n",
               kinds[kind],label(kind,variant),n);
        return 1;
    }
    return 0;
}
static uint64_t time_variant(int kind,unsigned int variant,unsigned int n) {
    unsigned int i;
    uint64_t start;
    prepare(kind,n); /* deterministic identical inputs */
    invoke(kind,variant,n);
    start=ps2_ticks_now();
    for(i=0;i<REPEAT;i++)
        invoke(kind,variant,n);
    /* Observable sink prevents dead-code removal of native scalar model. */
    sink ^= dest[(n*16-1)%(CAP+GUARD)];
    return ps2_elapsed_us(start);
}
int ps2_throughput_run(void) {
    static const unsigned int sizes[]={1,2,4,8,16,32,64};
    unsigned int kind,variant,n,index,failures=0,checks=0;
    for(kind=0;kind<K_COUNT;kind++)
        for(n=0;n<=64;n++)
            for(variant=0;variant<variants(kind);variant++){
                failures+=(unsigned int)check((int)kind,variant,n);
                checks++;
            }
    printf("THROUGHPUT_VERIFY,%s,checks=%u,failures=%u\n",
           failures?"FAIL":"PASS",checks,failures);
    if(failures)return 1; /* No performance lines for bad code. */
    /* Three independent warm-up + timing trials per size. Rotate the
     * variant order to avoid a systematic last-run hot-cache advantage.
     * Report actual input/output byte sizes (565 consumes 2x source).
     */
    puts("THROUGHPUT_CSV,kernel,variant,trial,source_bytes,dest_bytes,repetitions,elapsed_us");
    for(kind=0;kind<K_COUNT;kind++)
        for(index=0;index<sizeof(sizes)/sizeof(sizes[0]);index++){
            unsigned int trial,position,total=variants((int)kind);
            n=sizes[index];
            for(trial=0;trial<3;trial++)
                for(position=0;position<total;position++){
                    unsigned int bytes_src=kind==K_FILL?16u:
                        ((kind==K_TO565 || kind==K_FUSED565)?n*32u:n*16u);
                    unsigned int v=(position+trial+index)%total;
                    printf("THROUGHPUT_CSV,%s,%s,%u,%u,%u,%u,%lu\n",
                           kinds[kind],label((int)kind,v),trial,
                           bytes_src,n*16u,(unsigned int)REPEAT,
                           (unsigned long)time_variant((int)kind,v,n));
                }
        }
    printf("THROUGHPUT_RESULT,PASS,checksum=%08lx\n",(unsigned long)sink);
    return 0;
}
