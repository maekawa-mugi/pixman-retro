/* Standalone PS2 EE fused image + A8 OVER differential test.
 * For o32 fifth-argument (16(sp)) plus pixel and memory-canary checks.
 */
#ifdef PS2_TEST_PROGRESS
#include "ps2/progress.h"
#endif
#include <stdint.h>
#include <stdio.h>
#include <string.h>
extern void ee_mmi_over_image_a8_quads (uint32_t *,const uint32_t *,
                                        const uint8_t *,unsigned int,
                                        const uint16_t *);
#define N 128
#define GUARD 16
static uint32_t src[N+GUARD] __attribute__((aligned(16)));
static uint32_t src_old[N+GUARD] __attribute__((aligned(16)));
static uint32_t dst[N+GUARD] __attribute__((aligned(16)));
static uint32_t expected[N+GUARD] __attribute__((aligned(16)));
static uint8_t mask[N+32] __attribute__((aligned(16)));
static uint8_t mask_old[N+32] __attribute__((aligned(16)));
static const uint16_t cv[16] __attribute__((aligned(16))) =
{255,255,255,255,255,255,255,255,128,128,128,128,128,128,128,128};
static uint32_t state=0x41ad798bu;
static uint32_t rnd(void)
{
    uint32_t x=state; x^=x<<13; x^=x>>17; x^=x<<5;
    return state=x;
}
static unsigned int mul255(unsigned int a,unsigned int b)
{
    unsigned int x=a*b+128;return(x+(x>>8))>>8;
}
static uint32_t reference(uint32_t d,uint32_t s,unsigned int m)
{
    uint32_t v=0;unsigned int sh,ia=255-mul255(s>>24,m);
    for(sh=0;sh<32;sh+=8)
    {
        unsigned int p=mul255((s>>sh)&255,m)+mul255((d>>sh)&255,ia);
        if(p>255)p=255;
        v|=(uint32_t)p<<sh;
    }
    return v;
}
int main(void)
{
    state=0x41ad798bu;
    unsigned int it,i,n,offset,blocks;
    for(it=0;it<16000;it++)
    {
#ifdef PS2_TEST_PROGRESS
        if ((it & 255) == 0) ps2_progress_pulse(it, 16000);
#endif
        offset=it%16;
        blocks=it<256?32:(it/16)%33;
        n=blocks*4;
        for(i=0;i<N+GUARD;i++)
        {
            uint32_t s=rnd();
            if(it%13==0)s=0;
            else if(it%13==1)s=0xffffffffu;
            else if(it%13==2)s&=0x00ffffffu;
            else if(it%13==3)s=(s&0x00ffffffu)|0x80000000u;
            src[i]=s;dst[i]=rnd();
        }
        for(i=0;i<N+32;i++)mask[i]=it<256?(uint8_t)it:(uint8_t)rnd();
        memcpy(src_old,src,sizeof(src));
        memcpy(mask_old,mask,sizeof(mask));
        memcpy(expected,dst,sizeof(dst));
        for(i=0;i<n;i++)
            expected[i]=reference(dst[i],src[i],mask[offset+i]);
        ee_mmi_over_image_a8_quads(dst,src,mask+offset,blocks,cv);
        if(memcmp(dst,expected,sizeof(dst)) ||
           memcmp(src,src_old,sizeof(src)) ||
           memcmp(mask,mask_old,sizeof(mask)))
        {
            for(i=0;i<N+GUARD;i++)
                if(dst[i]!=expected[i])
                {
                    printf("FAIL image+A8 case=%u offset=%u quads=%u pixel=%u expected=%08x actual=%08x\n",
                           it,offset,blocks,i,expected[i],dst[i]);
                    return 1;
                }
            puts("FAIL image+A8 source/mask modified");
            return 1;
        }
    }
    puts("PASS image+A8 fused: 16000 cases / 256 masks / 16 byte offsets");
    return 0;
}
