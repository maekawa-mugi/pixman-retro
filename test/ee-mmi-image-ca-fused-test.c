/* Differential actual EE fused image + component-alpha OVER test (o32). */
#ifdef PS2_TEST_PROGRESS
#include "ps2/progress.h"
#endif
#include <stdint.h>
#include <stdio.h>
#include <string.h>
extern void ee_mmi_over_image_ca_quads(uint32_t *,const uint32_t *,
                                       const uint32_t *,unsigned int,
                                       const uint16_t *);
#define N 128
#define G 8
static uint32_t src[N+G] __attribute__((aligned(16)));
static uint32_t mask[N+G] __attribute__((aligned(16)));
static uint32_t dst[N+G] __attribute__((aligned(16)));
static uint32_t save_src[N+G] __attribute__((aligned(16)));
static uint32_t save_mask[N+G] __attribute__((aligned(16)));
static uint32_t expected[N+G] __attribute__((aligned(16)));
static const uint16_t cv[16] __attribute__((aligned(16))) =
{255,255,255,255,255,255,255,255,128,128,128,128,128,128,128,128};
static uint32_t state=0x617ed321u;
static uint32_t rnd(void)
{
    uint32_t v=state;v^=v<<13;v^=v>>17;v^=v<<5;return state=v;
}
static unsigned int mul(unsigned int a,unsigned int b)
{
    unsigned int x=a*b+128;return(x+(x>>8))>>8;
}
static uint32_t reference(uint32_t d,uint32_t s,uint32_t maskword)
{
    uint32_t result=0;unsigned int sh,sa=s>>24;
    for(sh=0;sh<32;sh+=8)
    {
        unsigned int m=(maskword>>sh)&255;
        unsigned int v=mul((s>>sh)&255,m)+mul((d>>sh)&255,255-mul(sa,m));
        if(v>255)v=255;
        result|=(uint32_t)v<<sh;
    }
    return result;
}
int main(void)
{
    state=0x617ed321u;
    unsigned int it,i,blocks,n;
    for(it=0;it<16000;it++)
    {
#ifdef PS2_TEST_PROGRESS
        if ((it & 255) == 0) ps2_progress_pulse(it, 16000);
#endif
        blocks=it%33;n=blocks*4;
        for(i=0;i<N+G;i++)
        {
            uint32_t s=rnd(),m=rnd();
            if(it%13==0)s=0;
            else if(it%13==1)s=0xffffffffu;
            else if(it%13==2)s&=0x00ffffffu;
            else if(it%13==3)s=(s&0x00ffffffu)|0x80000000u;
            if(it%11==0)m=0;
            else if(it%11==1)m=0xffffffffu;
            else if(it%11==2)m=0x00ff8001u;
            else if(it%11==3)m=0xff008080u;
            src[i]=s;mask[i]=m;dst[i]=rnd();
            expected[i]=i<n?reference(dst[i],s,m):dst[i];
        }
        memcpy(save_src,src,sizeof(src));
        memcpy(save_mask,mask,sizeof(mask));
        ee_mmi_over_image_ca_quads(dst,src,mask,blocks,cv);
        if(memcmp(dst,expected,sizeof(dst)) ||
           memcmp(src,save_src,sizeof(src)) ||
           memcmp(mask,save_mask,sizeof(mask)))
        {
            for(i=0;i<N+G;i++)if(dst[i]!=expected[i])
            {printf("FAIL imageCA iter=%u quads=%u pixel=%u expected=%08x got=%08x\n",
                    it,blocks,i,expected[i],dst[i]);return 1;}
            puts("FAIL imageCA source/mask modified");return 1;
        }
    }
    puts("PASS imageCA: 16000 cases, 0..32 quads and per-channel masks");
    return 0;
}
