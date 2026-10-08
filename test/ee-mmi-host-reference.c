/* Portable reference backend for checking test-suite portability.
 * NOT an emulation of EE instructions, ABI, DMA or R5900 timing.
 * The PS2 assembly must still be tested separately.
 */
#include <stdint.h>
#include <string.h>
static unsigned int div255(unsigned int a,unsigned int b)
{
    unsigned int v=a*b+128;return(v+(v>>8))>>8;
}
static uint32_t blend(uint32_t src,uint32_t dst)
{
    uint32_t out=0;unsigned int sh,inv=255-(src>>24);
    for(sh=0;sh<32;sh+=8)
    {
        unsigned int v=((src>>sh)&255)+div255((dst>>sh)&255,inv);
        if(v>255)v=255;
        out|=(uint32_t)v<<sh;
    }
    return out;
}
static uint32_t blend_covered(uint32_t src,uint32_t dst,unsigned int m)
{
    uint32_t out=0;unsigned int sh,inv=255-div255(src>>24,m);
    for(sh=0;sh<32;sh+=8)
    {
        unsigned int v=div255((src>>sh)&255,m)+div255((dst>>sh)&255,inv);
        if(v>255)v=255;
        out|=(uint32_t)v<<sh;
    }
    return out;
}
static uint32_t blend_ca(uint32_t src,uint32_t dst,uint32_t mask)
{
    uint32_t out=0;unsigned int sh,a=src>>24;
    for(sh=0;sh<32;sh+=8)
    {
        unsigned int m=(mask>>sh)&255;
        unsigned int v=div255((src>>sh)&255,m)+
                       div255((dst>>sh)&255,255-div255(a,m));
        if(v>255)v=255;
        out|=(uint32_t)v<<sh;
    }
    return out;
}
static uint16_t pack565(uint32_t s)
{
    uint32_t a=(s>>3)&0x1f001f,b=s&0xfc00;
    a|=a>>5;a|=b>>5;return(uint16_t)a;
}
static uint32_t unpack565(uint16_t d)
{
    uint32_t s=d;
    return (((s<<3)&0xf8)|((s>>2)&7)|
            ((s<<5)&0xfc00)|((s>>1)&0x300)|
            ((s<<8)&0xf80000)|((s<<3)&0x70000));
}
void ee_mmi_add_quads(uint8_t *dst,const uint8_t *src,unsigned int quads)
{
    unsigned int i;
    for(i=0;i<16*quads;i++)
    {
        unsigned int v=dst[i]+src[i];
        dst[i]=(uint8_t)(v>255?255:v);
    }
}
void ee_mmi_copy_quads(uint8_t *dst,const uint8_t *src,unsigned int quads)
{
    memcpy(dst,src,16*quads);
}
void ee_mmi_fill_quads(uint8_t *dst,const uint32_t *pattern,unsigned int quads)
{
    unsigned int i;const uint8_t *p=(const uint8_t *)pattern;
    for(i=0;i<16*quads;i++)dst[i]=p[i&15];
}
void ee_mmi_in_a8_quads(uint8_t *dst,const uint8_t *src,
                        unsigned int quads,const uint16_t *rounding)
{
    unsigned int i;(void)rounding;
    for(i=0;i<16*quads;i++)dst[i]=(uint8_t)div255(dst[i],src[i]);
}
void ee_mmi_over_quads(uint32_t *dst,const uint32_t *src,
                       unsigned int quads,const uint16_t *constants)
{
    unsigned int i;(void)constants;
    for(i=0;i<4*quads;i++)dst[i]=blend(src[i],dst[i]);
}
void ee_mmi_8888_to_0565_quads(uint16_t *dst,const uint32_t *src,
                               unsigned int quads,const uint32_t *constants)
{
    unsigned int i;(void)constants;
    for(i=0;i<8*quads;i++)dst[i]=pack565(src[i]);
}
void ee_mmi_over_8888_0565_quads(uint16_t *dst,const uint32_t *src,
                                  unsigned int quads,const uint32_t *constants)
{
    unsigned int i;(void)constants;
    for(i=0;i<8*quads;i++)
        dst[i]=pack565(blend(src[i],unpack565(dst[i])));
}
void ee_mmi_ca_over_quads(uint32_t *dst,const uint32_t *prepared,
                          unsigned int quads,const uint16_t *rounding)
{
    unsigned int i,sh;(void)rounding;
    for(i=0;i<4*quads;i++)
    {
        unsigned int offset=(i/4)*8+(i&3);
        uint32_t s=prepared[offset],inv=prepared[offset+4],out=0;
        for(sh=0;sh<32;sh+=8)
        {
            unsigned int v=((s>>sh)&255)+
                           div255((dst[i]>>sh)&255,(inv>>sh)&255);
            if(v>255)v=255;
            out|=(uint32_t)v<<sh;
        }
        dst[i]=out;
    }
}
void ee_mmi_ca_over_solid_quads(uint32_t *dst,const uint32_t *mask,
                                 unsigned int quads,const uint32_t *constants)
{
    unsigned int i;
    for(i=0;i<4*quads;i++)dst[i]=blend_ca(constants[0],dst[i],mask[i]);
}
void ee_mmi_over_solid_a8_quads(uint32_t *dst,const uint8_t *mask,
                                unsigned int quads,const uint32_t *constants)
{
    unsigned int i;
    for(i=0;i<4*quads;i++)dst[i]=blend_covered(constants[0],dst[i],mask[i]);
}
void ee_mmi_over_image_a8_quads(uint32_t *dst,const uint32_t *src,
                                 const uint8_t *mask,unsigned int quads,
                                 const uint16_t *constants)
{
    unsigned int i;(void)constants;
    for(i=0;i<4*quads;i++)dst[i]=blend_covered(src[i],dst[i],mask[i]);
}

void ee_mmi_over_image_ca_quads(uint32_t *dst,const uint32_t *src,
                                 const uint32_t *mask,unsigned int blocks,
                                 const uint16_t *constants)
{
    unsigned int i;(void)constants;
    for(i=0;i<blocks*4;i++)dst[i]=blend_ca(src[i],dst[i],mask[i]);
}

/* Native testing substitutes. These do not validate R5900 instructions. */
void ee_mmi_nearest_opaque_quads(uint32_t *dst,const uint32_t *src,
                                  unsigned int quads,const uint32_t *alpha)
{
    unsigned int i;
    for(i=0;i<quads*4;i++)dst[i]=src[i]|alpha[i&3];
}
void ee_mmi_bilinear_lerp_quads(uint32_t *dst,const uint32_t *first,
                                 const uint32_t *second,
                                 const uint16_t *weights,unsigned int quads)
{
    unsigned int i,j;
    for(i=0;i<quads*4;i++)
    {
        unsigned int weight_index=(i/4)*32+((i&2)?8:0)+((i&1)?4:0);
        unsigned int w=weights[weight_index+16];
        uint32_t out=0;
        for(j=0;j<32;j+=8)
        {
            unsigned int a=(first[i]>>j)&255,b=(second[i]>>j)&255;
            out|=(uint32_t)((a*(256-w)+b*w)>>8)<<j;
        }
        dst[i]=out;
    }
}

void ee_mmi_bilinear_hsum_quads(uint16_t *out,const uint32_t *first,
                                 const uint32_t *second,
                                 const uint16_t *weights,unsigned int quads)
{
    unsigned int i,j;
    for(i=0;i<4*quads;i++)
    {
        unsigned int off=(i/4)*32+((i&2)?8:0)+((i&1)?4:0);
        unsigned int w0=weights[off],w1=weights[off+16];
        for(j=0;j<4;j++)
        {
            unsigned int sh=8*j;
            unsigned int a=(first[i]>>sh)&255,b=(second[i]>>sh)&255;
            out[i*4+j]=(uint16_t)(a*w0+b*w1);
        }
    }
}
void ee_mmi_bilinear_vfinish_quads(uint32_t *out,const uint16_t *top,
                                   const uint16_t *bottom,
                                   const uint16_t *weights,unsigned int quads)
{
    unsigned int i,j,w0=weights[0],w1=weights[8];
    for(i=0;i<4*quads;i++)
    {
        uint32_t value=0;
        for(j=0;j<4;j++)
        {
            unsigned int ch=(top[i*4+j]*w0+bottom[i*4+j]*w1)>>16;
            value|=(uint32_t)ch<<(j*8);
        }
        out[i]=value;
    }
}
