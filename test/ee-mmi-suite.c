/* One-shot EE MMI correctness and benchmark suite.
 * Individual fused kernel regressions are linked with their main renamed.
 * Use test/Makefile.ee-mmi; no Pixman headers or libraries required.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern int ee_test_ca(void);
extern int ee_test_solid_a8(void);
extern int ee_test_565(void);
extern int ee_test_image_a8(void);
extern int ee_test_image_ca(void);
extern int ee_test_scale(void);
extern int ee_test_bilinear4(void);
extern void ee_mmi_bench_run(void);

extern void ee_mmi_add_quads(uint8_t *, const uint8_t *, unsigned int);
extern void ee_mmi_copy_quads(uint8_t *, const uint8_t *, unsigned int);
extern void ee_mmi_fill_quads(uint8_t *, const uint32_t *, unsigned int);
extern void ee_mmi_in_a8_quads(uint8_t *, const uint8_t *, unsigned int, const uint16_t *);
extern void ee_mmi_over_quads(uint32_t *, const uint32_t *, unsigned int, const uint16_t *);
extern void ee_mmi_8888_to_0565_quads(uint16_t *, const uint32_t *, unsigned int, const uint32_t *);

#define GUARD 64
#define MAX_QUADS 128
#define BYTES (MAX_QUADS * 16)
static uint8_t input[BYTES + GUARD] __attribute__((aligned(16)));
static uint8_t output[BYTES + GUARD] __attribute__((aligned(16)));
static uint8_t expected[BYTES + GUARD] __attribute__((aligned(16)));
static uint32_t pattern[4] __attribute__((aligned(16)));
static const uint16_t rounding[8] __attribute__((aligned(16))) =
{128,128,128,128,128,128,128,128};
static const uint16_t over_constants[16] __attribute__((aligned(16))) =
{255,255,255,255,255,255,255,255,128,128,128,128,128,128};
static const uint32_t convert_masks[12] __attribute__((aligned(16))) =
{
    0x0000f800,0x0000f800,0x0000f800,0x0000f800,
    0x000007e0,0x000007e0,0x000007e0,0x000007e0,
    0x0000001f,0x0000001f,0x0000001f,0x0000001f
};
static uint32_t seed=0x92754aa1u;
static uint32_t rnd(void)
{
    uint32_t x=seed; x^=x<<13; x^=x>>17; x^=x<<5; return seed=x;
}
static unsigned int mul255(unsigned int a,unsigned int b)
{
    unsigned int x=a*b+128; return (x+(x>>8))>>8;
}
static uint32_t over_reference(uint32_t dst,uint32_t src)
{
    uint32_t r=0;
    unsigned int a=255-(src>>24), shift;
    for(shift=0;shift<32;shift+=8)
    {
        unsigned int v=((dst>>shift)&255);
        unsigned int s=((src>>shift)&255);
        unsigned int t=s+mul255(v,a);
        if(t>255)t=255;
        r|=(uint32_t)t<<shift;
    }
    return r;
}
static uint16_t convert_reference(uint32_t src)
{
    uint32_t a=(src>>3)&0x1f001f,b=src&0xfc00;
    a|=a>>5; a|=b>>5; return (uint16_t)a;
}
static int core_test(void)
{
    unsigned int n,k,j,bytes;
    for(n=0;n<=MAX_QUADS;n++)
    {
        bytes=n*16;
        for(k=0;k<BYTES+GUARD;k++)
        {
            input[k]=(uint8_t)rnd();
            output[k]=(uint8_t)rnd();
        }

        memcpy(expected,output,sizeof(output));
        for(k=0;k<bytes;k++)
        {
            unsigned int val=(unsigned int)output[k]+input[k];
            expected[k]=(uint8_t)(val>255?255:val);
        }
        ee_mmi_add_quads(output,input,n);
        if(memcmp(output,expected,sizeof(output)))
        {printf("FAIL add quads=%u\n",n);return 1;}

        memcpy(output,input,sizeof(output));
        memcpy(expected,input,sizeof(input));
        ee_mmi_add_quads(output,output,n);
        for(k=0;k<bytes;k++)
        {
            unsigned int val=(unsigned int)input[k]*2;
            expected[k]=(uint8_t)(val>255?255:val);
        }
        if(memcmp(output,expected,sizeof(output)))
        {printf("FAIL add alias quads=%u\n",n);return 1;}

        for(k=0;k<BYTES+GUARD;k++)output[k]=(uint8_t)rnd();
        memcpy(expected,output,sizeof(output));
        memcpy(expected,input,bytes);
        ee_mmi_copy_quads(output,input,n);
        if(memcmp(output,expected,sizeof(output)))
        {printf("FAIL copy quads=%u\n",n);return 1;}

        for(j=0;j<4;j++)pattern[j]=rnd();
        for(k=0;k<BYTES+GUARD;k++)output[k]=(uint8_t)rnd();
        memcpy(expected,output,sizeof(output));
        for(k=0;k<bytes;k++)
            expected[k]=((const uint8_t *)pattern)[k%16];
        ee_mmi_fill_quads(output,pattern,n);
        if(memcmp(output,expected,sizeof(output)))
        {printf("FAIL fill quads=%u\n",n);return 1;}

        for(k=0;k<BYTES+GUARD;k++)output[k]=(uint8_t)rnd();
        memcpy(expected,output,sizeof(output));
        for(k=0;k<bytes;k++)
            expected[k]=(uint8_t)mul255(output[k],input[k]);
        ee_mmi_in_a8_quads(output,input,n,rounding);
        if(memcmp(output,expected,sizeof(output)))
        {printf("FAIL A8 IN quads=%u\n",n);return 1;}

        for(k=0;k<BYTES+GUARD;k++)output[k]=(uint8_t)rnd();
        memcpy(expected,output,sizeof(output));
        for(k=0;k<bytes/4;k++)
        {
            uint32_t src,old,result;
            memcpy(&src,input+4*k,4);
            memcpy(&old,output+4*k,4);
            result=over_reference(old,src);
            memcpy(expected+4*k,&result,4);
        }
        ee_mmi_over_quads((uint32_t *)output,(const uint32_t *)input,n,over_constants);
        if(memcmp(output,expected,sizeof(output)))
        {printf("FAIL 8888 OVER quads=%u\n",n);return 1;}

        /* Eight pixels per 565 block: write at most 16 * n bytes,
         * read 32 * n bytes (limited to MAX_QUADS/2 here).
         */
        if(n<=MAX_QUADS/2)
        {
            for(k=0;k<BYTES+GUARD;k++)output[k]=(uint8_t)rnd();
            memcpy(expected,output,sizeof(output));
            for(k=0;k<8*n;k++)
            {
                uint32_t src;
                uint16_t v;
                memcpy(&src,input+4*k,4);
                v=convert_reference(src);
                memcpy(expected+2*k,&v,2);
            }
            ee_mmi_8888_to_0565_quads((uint16_t *)output,
                                       (const uint32_t *)input,n,convert_masks);
            if(memcmp(output,expected,sizeof(output)))
            {printf("FAIL 8888->565 quads=%u\n",n);return 1;}
        }
    }
    printf("PASS core: %u block counts; ADD alias, COPY, FILL, A8 IN, OVER, 565 SRC\n",
           MAX_QUADS+1);
    return 0;
}
int main(int argc,char **argv)
{
    int fail=0,do_test=1,do_bench=1;
    if(argc>1)
    {
        if(!strcmp(argv[1],"--test-only"))do_bench=0;
        else if(!strcmp(argv[1],"--bench-only"))do_test=0;
        else
        {puts("usage: ee-mmi-suite [--test-only | --bench-only]");return 2;}
    }
    if(do_test)
    {
        puts("EE_MMI_SUITE_V1");
        fail+=core_test();
        fail+=ee_test_ca();
        fail+=ee_test_solid_a8();
        fail+=ee_test_565();
        fail+=ee_test_image_a8();
        fail+=ee_test_image_ca();
        fail+=ee_test_scale();
        fail+=ee_test_bilinear4();
    }
    if(do_bench)
        ee_mmi_bench_run();
    printf("EE_MMI_SUITE_RESULT,%s,failures=%d\n",fail?"FAIL":"PASS",fail);
    return fail?1:0;
}
