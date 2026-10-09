/* Native-only scalar substitutes for experimental R5900 variants.
 * These check harness routing, never prove instruction correctness.
 */
#include <stdint.h>
void ee_mmi_nearest_opaque_u2_quads(uint32_t *dst,const uint32_t *src,
                                     unsigned int quads,const uint32_t *alpha)
{
    unsigned int i;
    for(i=0;i<quads*4;i++)dst[i]=src[i]|alpha[i&3];
}
void ee_mmi_nearest_opaque_u4_quads(uint32_t *dst,const uint32_t *src,
                                     unsigned int quads,const uint32_t *alpha)
{
    unsigned int i;
    for(i=0;i<quads*4;i++)dst[i]=src[i]|alpha[i&3];
}
void ee_mmi_bilinear_hsum_sched_quads(uint16_t *dst,const uint32_t *a,
                                      const uint32_t *b,const uint16_t *w,
                                      unsigned int quads)
{
    unsigned int i,k;
    for(i=0;i<quads*4;i++)
    {
        unsigned int off=(i/4)*32+((i&2)?8:0)+((i&1)?4:0);
        unsigned int x=w[off],y=w[off+16];
        for(k=0;k<4;k++)
        {
            unsigned int sh=k*8;
            dst[i*4+k]=(uint16_t)((((a[i]>>sh)&255u)*x)+
                                    (((b[i]>>sh)&255u)*y));
        }
    }
}

void ee_mmi_bilinear_hsum_predecode_quads(uint16_t *dst,const uint32_t *a,
                                          const uint32_t *b,const uint16_t *w,
                                          unsigned int quads)
{
    unsigned int i,k;
    for(i=0;i<quads*4;i++)
    {
        unsigned int off=(i/4)*32+((i&2)?8:0)+((i&1)?4:0);
        unsigned int left=w[off],right=w[off+16];
        for(k=0;k<4;k++)
        {
            unsigned int sh=k*8;
            dst[i*4+k]=(uint16_t)((((a[i]>>sh)&255u)*left)+
                                    (((b[i]>>sh)&255u)*right));
        }
    }
}
