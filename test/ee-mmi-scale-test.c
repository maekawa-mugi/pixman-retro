/* Standalone nearest opaque and exact two-tap bilinear EE MMI tests.
 * Compile with pixman-ee-mmi-scale-asm.S using the PS2 o32 toolchain.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define N 128
#define GUARD 16
extern void ee_mmi_nearest_opaque_quads (uint32_t *, const uint32_t *,
                                         unsigned int, const uint32_t *);
extern void ee_mmi_bilinear_lerp_quads (uint32_t *, const uint32_t *,
                                        const uint32_t *, const uint16_t *,
                                        unsigned int);
static uint32_t a[N+GUARD] __attribute__((aligned(16)));
static uint32_t b[N+GUARD] __attribute__((aligned(16)));
static uint32_t d[N+GUARD] __attribute__((aligned(16)));
static uint32_t expected[N+GUARD] __attribute__((aligned(16)));
static uint32_t original_a[N+GUARD] __attribute__((aligned(16)));
static uint32_t original_b[N+GUARD] __attribute__((aligned(16)));
static uint16_t weights[N*8] __attribute__((aligned(16)));
static uint16_t original_weights[N*8] __attribute__((aligned(16)));
static const uint32_t alpha[4] __attribute__((aligned(16))) =
{0xff000000u,0xff000000u,0xff000000u,0xff000000u};
static uint32_t rng_state=0x71928c4fu;
static uint32_t rnd (void)
{
    uint32_t v=rng_state;v^=v<<13;v^=v>>17;v^=v<<5;
    return rng_state=v;
}
static uint32_t interpolate(uint32_t first,uint32_t second,unsigned int w)
{
    uint32_t out=0;unsigned int j;
    for(j=0;j<32;j+=8)
    {
        unsigned int f=(first>>j)&255;
        unsigned int s=(second>>j)&255;
        out|=(uint32_t)((f*(256-w)+s*w)>>8)<<j;
    }
    return out;
}
int main(void)
{
    unsigned int it,i,j,blocks,count;
    for(it=0;it<12000;it++)
    {
        blocks=it%33;
        count=blocks*4;
        for(i=0;i<N+GUARD;i++)
        {
            a[i]=rnd();
            b[i]=rnd();
            d[i]=rnd();
            expected[i]=i<count?(a[i]|0xff000000u):d[i];
        }
        memcpy(original_a,a,sizeof(a));
        ee_mmi_nearest_opaque_quads(d,a,blocks,alpha);
        if(memcmp(d,expected,sizeof(d)) ||
           memcmp(a,original_a,sizeof(a)))
        {
            printf("FAIL nearest-opaque iter=%u quads=%u\n",it,blocks);
            return 1;
        }

        for(i=0;i<N+GUARD;i++)
        {
            a[i]=rnd();
            b[i]=rnd();
            d[i]=rnd();
        }
        /* Coefficients layout: w0[0..7],w0[8..15],
         * w1[0..7],w1[8..15] for each four pixels.
         */
        for(i=0;i<N;i++)
        {
            unsigned int w=(it+i)%257;
            unsigned int index=(i/4)*32+((i&2)?8:0)+((i&1)?4:0);
            for(j=0;j<4;j++)
            {
                weights[index+j]=(uint16_t)(256-w);
                weights[index+16+j]=(uint16_t)w;
            }
        }
        memcpy(original_a,a,sizeof(a));
        memcpy(original_b,b,sizeof(b));
        memcpy(original_weights,weights,sizeof(weights));
        memcpy(expected,d,sizeof(d));
        for(i=0;i<count;i++)
        {
            unsigned int index=(i/4)*32+((i&2)?8:0)+((i&1)?4:0);
            expected[i]=interpolate(a[i],b[i],weights[index+16]);
        }
        ee_mmi_bilinear_lerp_quads(d,a,b,weights,blocks);
        if(memcmp(d,expected,sizeof(d)) ||
           memcmp(a,original_a,sizeof(a)) ||
           memcmp(b,original_b,sizeof(b)) ||
           memcmp(weights,original_weights,sizeof(weights)))
        {
            for(i=0;i<N+GUARD;i++)
                if(d[i]!=expected[i])
                {
                    printf("FAIL bilinear-2tap iter=%u quads=%u pixel=%u exp=%08x got=%08x\n",
                           it,blocks,i,expected[i],d[i]);
                    return 1;
                }
            puts("FAIL bilinear-2tap modified input");return 1;
        }
    }
    puts("PASS nearest/2tap: 12000 cases per kernel, blocks 0..32, weights 0..256");
    return 0;
}
