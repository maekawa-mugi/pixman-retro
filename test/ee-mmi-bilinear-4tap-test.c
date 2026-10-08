/* Standalone R5900 exact 4-tap bilinear MMI regression.
 * Link with pixman-ee-mmi-scale-asm.S using the PS2 Linux o32 compiler.
 * Verify horizontal 16-bit numerators, vertical output and guards.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define N 128
#define GUARD 16

extern void ee_mmi_bilinear_hsum_quads(uint16_t *,const uint32_t *,
                                       const uint32_t *,const uint16_t *,
                                       unsigned int);
extern void ee_mmi_bilinear_vfinish_quads(uint32_t *,const uint16_t *,
                                          const uint16_t *,const uint16_t *,
                                          unsigned int);
static uint32_t tl[N+GUARD] __attribute__((aligned(16)));
static uint32_t tr[N+GUARD] __attribute__((aligned(16)));
static uint32_t bl[N+GUARD] __attribute__((aligned(16)));
static uint32_t br[N+GUARD] __attribute__((aligned(16)));
static uint32_t dest[N+GUARD] __attribute__((aligned(16)));
static uint32_t expected[N+GUARD] __attribute__((aligned(16)));
static uint32_t tl_save[N+GUARD] __attribute__((aligned(16)));
static uint32_t tr_save[N+GUARD] __attribute__((aligned(16)));
static uint32_t bl_save[N+GUARD] __attribute__((aligned(16)));
static uint32_t br_save[N+GUARD] __attribute__((aligned(16)));
static uint16_t top_h[(N+GUARD)*4] __attribute__((aligned(16)));
static uint16_t bot_h[(N+GUARD)*4] __attribute__((aligned(16)));
static uint16_t top_ref[(N+GUARD)*4] __attribute__((aligned(16)));
static uint16_t bot_ref[(N+GUARD)*4] __attribute__((aligned(16)));
static uint16_t xweights[N*8] __attribute__((aligned(16)));
static uint16_t xweights_save[N*8] __attribute__((aligned(16)));
static uint16_t yweights[24] __attribute__((aligned(16)));
static uint32_t seed=0x571afcc3u;
static uint32_t rnd(void)
{
    uint32_t x=seed;x^=x<<13;x^=x>>17;x^=x<<5;return seed=x;
}
static uint32_t reference_pixel(uint32_t a,uint32_t b,uint32_t c,
                                 uint32_t d,unsigned int dx,unsigned int dy)
{
    unsigned int w0=(128-dx)*(128-dy);
    unsigned int w1=dx*(128-dy);
    unsigned int w2=(128-dx)*dy;
    unsigned int w3=dx*dy;
    unsigned int j;uint32_t out=0;
    for(j=0;j<32;j+=8)
    {
        unsigned int value=(((a>>j)&255)*w0+((b>>j)&255)*w1+
                             ((c>>j)&255)*w2+((d>>j)&255)*w3)>>14;
        out|=(uint32_t)value<<j;
    }
    return out;
}
int main(void)
{
    unsigned int iter,blocks,count,i,j,dy;
    for(iter=0;iter<5000;iter++)
    {
        blocks=iter%33;
        count=blocks*4;
        dy=iter%128;
        for(i=0;i<8;i++)
        {
            yweights[i]=(uint16_t)(256-2*dy);
            yweights[i+8]=(uint16_t)(2*dy);
            yweights[i+16]=255;
        }
        for(i=0;i<N+GUARD;i++)
        {
            tl[i]=iter%11==0?0:iter%11==1?0xffffffffu:rnd();
            tr[i]=rnd();
            bl[i]=rnd();
            br[i]=iter%13==0?0xffffffffu:rnd();
            dest[i]=rnd();
            expected[i]=dest[i];
        }
        for(i=0;i<(N+GUARD)*4;i++)
        {
            top_h[i]=(uint16_t)rnd();
            bot_h[i]=(uint16_t)rnd();
        }
        memcpy(top_ref,top_h,sizeof(top_h));
        memcpy(bot_ref,bot_h,sizeof(bot_h));
        for(i=0;i<N;i++)
        {
            unsigned int dx=(iter*13+i*31)%128;
            unsigned int w=2*dx;
            unsigned int offset=(i/4)*32+((i&2)?8:0)+((i&1)?4:0);
            for(j=0;j<4;j++)
            {
                xweights[offset+j]=(uint16_t)(256-w);
                xweights[offset+16+j]=(uint16_t)w;
                if(i<count)
                {
                    unsigned int shift=j*8;
                    top_ref[i*4+j]=(uint16_t)((((tl[i]>>shift)&255)*(256-w))+
                                              (((tr[i]>>shift)&255)*w));
                    bot_ref[i*4+j]=(uint16_t)((((bl[i]>>shift)&255)*(256-w))+
                                              (((br[i]>>shift)&255)*w));
                }
            }
            if(i<count)
                expected[i]=reference_pixel(tl[i],tr[i],bl[i],br[i],dx,dy);
        }
        memcpy(tl_save,tl,sizeof(tl));
        memcpy(tr_save,tr,sizeof(tr));
        memcpy(bl_save,bl,sizeof(bl));
        memcpy(br_save,br,sizeof(br));
        memcpy(xweights_save,xweights,sizeof(xweights));

        ee_mmi_bilinear_hsum_quads(top_h,tl,tr,xweights,blocks);
        ee_mmi_bilinear_hsum_quads(bot_h,bl,br,xweights,blocks);
        if(memcmp(top_h,top_ref,sizeof(top_h)) ||
           memcmp(bot_h,bot_ref,sizeof(bot_h)))
        {
            printf("FAIL 4tap horizontal iter=%u quads=%u\n",iter,blocks);
            return 1;
        }

        ee_mmi_bilinear_vfinish_quads(dest,top_h,bot_h,yweights,blocks);
        if(memcmp(dest,expected,sizeof(dest)) ||
           memcmp(tl,tl_save,sizeof(tl)) ||
           memcmp(tr,tr_save,sizeof(tr)) ||
           memcmp(bl,bl_save,sizeof(bl)) ||
           memcmp(br,br_save,sizeof(br)) ||
           memcmp(xweights,xweights_save,sizeof(xweights)))
        {
            for(i=0;i<N+GUARD;i++)
                if(dest[i]!=expected[i])
                {
                    printf("FAIL 4tap vertical iter=%u quad=%u pixel=%u ref=%08x actual=%08x\n",
                           iter,blocks,i,expected[i],dest[i]);
                    return 1;
                }
            puts("FAIL 4tap modified input");return 1;
        }
    }
    puts("PASS 4tap bilinear: 5000 cases, all 7-bit Y weights, block counts 0..32");
    return 0;
}
