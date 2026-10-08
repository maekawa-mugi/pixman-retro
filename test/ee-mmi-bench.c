/* Comparable C/MMI microbenchmarks for EE. Uses gettimeofday() for
 * old PS2 Linux libc compatibility; timings <1us are reported as N/A.
 * These are hot-loop kernel timings, not Pixman end-to-end speed.
 * For 565 kernels the bytes field means output bytes; source uses 2*bytes.
 */
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include <string.h>
extern void ee_mmi_add_quads(uint8_t *,const uint8_t *,unsigned int);
extern void ee_mmi_copy_quads(uint8_t *,const uint8_t *,unsigned int);
extern void ee_mmi_fill_quads(uint8_t *,const uint32_t *,unsigned int);
extern void ee_mmi_in_a8_quads(uint8_t *,const uint8_t *,unsigned int,const uint16_t *);
extern void ee_mmi_over_quads(uint32_t *,const uint32_t *,unsigned int,const uint16_t *);
extern void ee_mmi_ca_over_solid_quads(uint32_t *,const uint32_t *,unsigned int,const uint32_t *);
extern void ee_mmi_over_solid_a8_quads(uint32_t *,const uint8_t *,unsigned int,const uint32_t *);
extern void ee_mmi_over_image_a8_quads(uint32_t *,const uint32_t *,const uint8_t *,unsigned int,const uint16_t *);
extern void ee_mmi_over_image_ca_quads(uint32_t *,const uint32_t *,const uint32_t *,unsigned int,const uint16_t *);
extern void ee_mmi_over_8888_0565_quads(uint16_t *,const uint32_t *,unsigned int,const uint32_t *);
extern void ee_mmi_8888_to_0565_quads(uint16_t *,const uint32_t *,unsigned int,const uint32_t *);
#define CAP 32768
static uint8_t dest[CAP] __attribute__((aligned(16)));
static uint8_t source[CAP] __attribute__((aligned(16)));
static uint8_t mask_bytes[CAP] __attribute__((aligned(16)));
static const uint32_t fill_pattern[4] __attribute__((aligned(16))) =
{0x395a7b9cu,0x395a7b9cu,0x395a7b9cu,0x395a7b9cu};
static const uint16_t half[8] __attribute__((aligned(16))) =
{128,128,128,128,128,128,128,128};
static const uint16_t over_const[16] __attribute__((aligned(16))) =
{255,255,255,255,255,255,255,255,128,128,128,128,128,128};
static const uint32_t solid_const[16] __attribute__((aligned(16))) =
{
    0x80402010u,0x80402010u,0x80402010u,0x80402010u,
    0x00800080u,0x00800080u,0x00800080u,0x00800080u,
    0x00800080u,0x00800080u,0x00800080u,0x00800080u,
    0x00ff00ffu,0x00ff00ffu,0x00ff00ffu,0x00ff00ffu
};
static const uint32_t over565_constants[28] __attribute__((aligned(16))) =
{
    0x001f001fu,0x001f001fu,0x001f001fu,0x001f001fu,
    0x003f003fu,0x003f003fu,0x003f003fu,0x003f003fu,
    0x00ff00ffu,0x00ff00ffu,0x00ff00ffu,0x00ff00ffu,
    0x00800080u,0x00800080u,0x00800080u,0x00800080u,
    0x0000f800u,0x0000f800u,0x0000f800u,0x0000f800u,
    0x000007e0u,0x000007e0u,0x000007e0u,0x000007e0u,
    0x0000001fu,0x0000001fu,0x0000001fu,0x0000001fu
};
static const uint32_t convert_masks[12] __attribute__((aligned(16))) =
{
    0x0000f800u,0x0000f800u,0x0000f800u,0x0000f800u,
    0x000007e0u,0x000007e0u,0x000007e0u,0x000007e0u,
    0x0000001fu,0x0000001fu,0x0000001fu,0x0000001fu
};
static volatile uint32_t checksum=0;
static unsigned int scale(unsigned int x,unsigned int y)
{
    unsigned int z=x*y+128;return(z+(z>>8))>>8;
}
static uint32_t blend(uint32_t s,uint32_t d)
{
    uint32_t o=0; unsigned int k,inv=255-(s>>24);
    for(k=0;k<32;k+=8)
    {
        unsigned int v=((s>>k)&255)+scale((d>>k)&255,inv);
        if(v>255)v=255;
        o|=(uint32_t)v<<k;
    }
    return o;
}
static uint32_t ca_blend(uint32_t s,uint32_t d,uint32_t mask)
{
    uint32_t out=0;
    unsigned int k,a=s>>24;
    for(k=0;k<32;k+=8)
    {
        unsigned int coverage=(mask>>k)&255;
        unsigned int source=(s>>k)&255;
        unsigned int target=(d>>k)&255;
        unsigned int inv=255-scale(a,coverage);
        unsigned int ch=scale(source,coverage)+scale(target,inv);
        if(ch>255)ch=255;
        out|=(uint32_t)ch<<k;
    }
    return out;
}
static uint32_t covered_blend(uint32_t s,uint32_t d,unsigned int coverage)
{
    uint32_t out=0;
    unsigned int k,inv=255-scale(s>>24,coverage);
    for(k=0;k<32;k+=8)
    {
        unsigned int ch=scale((s>>k)&255,coverage)+
                        scale((d>>k)&255,inv);
        if(ch>255)ch=255;
        out|=(uint32_t)ch<<k;
    }
    return out;
}
static uint32_t expand565(uint16_t s)
{
    return ((((uint32_t)s<<3)&0xf8)|(((uint32_t)s>>2)&7)|
            (((uint32_t)s<<5)&0xfc00)|(((uint32_t)s>>1)&0x300)|
            (((uint32_t)s<<8)&0xf80000)|(((uint32_t)s<<3)&0x70000));
}
static uint16_t reduce565(uint32_t s)
{
    uint32_t a=(s>>3)&0x1f001f,b=s&0xfc00;
    a|=a>>5; a|=b>>5; return(uint16_t)a;
}
static void run_kernel(int id,int mmi,unsigned int bytes)
{
    unsigned int i,n=bytes/16;
    if(mmi)
    {
        switch(id)
        {
        case 0:ee_mmi_add_quads(dest,source,n);break;
        case 1:ee_mmi_copy_quads(dest,source,n);break;
        case 2:ee_mmi_fill_quads(dest,fill_pattern,n);break;
        case 3:ee_mmi_in_a8_quads(dest,source,n,half);break;
        case 4:ee_mmi_over_quads((uint32_t *)dest,(const uint32_t *)source,n,over_const);break;
        case 5:ee_mmi_ca_over_solid_quads((uint32_t *)dest,
                                           (const uint32_t *)source,n,solid_const);break;
        case 6:ee_mmi_over_solid_a8_quads((uint32_t *)dest,mask_bytes,n,solid_const);break;
        case 7:ee_mmi_over_image_a8_quads((uint32_t *)dest,
                                          (const uint32_t *)source,mask_bytes+3,n,over_const);break;
        case 8:ee_mmi_over_8888_0565_quads((uint16_t *)dest,
                                            (const uint32_t *)source,n,over565_constants);break;
        case 9:ee_mmi_8888_to_0565_quads((uint16_t *)dest,
                                          (const uint32_t *)source,n,convert_masks);break;
        case 10:ee_mmi_over_image_ca_quads((uint32_t *)dest,
                                            (const uint32_t *)source,
                                            (const uint32_t *)mask_bytes,n,over_const);break;
        }
    }
    else
    {
        switch(id)
        {
        case 0:
            for(i=0;i<bytes;i++){unsigned int x=dest[i]+source[i];dest[i]=(uint8_t)(x>255?255:x);}
            break;
        case 1:memcpy(dest,source,bytes);break;
        case 2:
            for(i=0;i<bytes;i++)dest[i]=((const uint8_t *)fill_pattern)[i&15];
            break;
        case 3:
            for(i=0;i<bytes;i++)dest[i]=(uint8_t)scale(dest[i],source[i]);
            break;
        case 4:
            for(i=0;i<bytes/4;i++)
            {
                uint32_t x=((const uint32_t *)source)[i];
                uint32_t y=((uint32_t *)dest)[i];
                ((uint32_t *)dest)[i]=blend(x,y);
            }
            break;
        case 5:
            for(i=0;i<bytes/4;i++)
            {
                uint32_t m=((const uint32_t *)source)[i];
                uint32_t d=((uint32_t *)dest)[i];
                ((uint32_t *)dest)[i]=ca_blend(0x80402010u,d,m);
            }
            break;
        case 6:
            for(i=0;i<bytes/4;i++)
            {
                uint32_t d=((uint32_t *)dest)[i];
                ((uint32_t *)dest)[i]=covered_blend(0x80402010u,d,mask_bytes[i]);
            }
            break;
        case 7:
            for(i=0;i<bytes/4;i++)
            {
                uint32_t d=((uint32_t *)dest)[i];
                ((uint32_t *)dest)[i]=covered_blend(((uint32_t *)source)[i],
                                                     d,mask_bytes[3+i]);
            }
            break;
        case 8:
            for(i=0;i<bytes/2;i++)
            {
                uint16_t *d=(uint16_t *)dest;
                uint32_t s=((uint32_t *)source)[i];
                d[i]=reduce565(blend(s,expand565(d[i])));
            }
            break;
        case 9:
            for(i=0;i<bytes/2;i++)
                ((uint16_t *)dest)[i]=reduce565(((uint32_t *)source)[i]);
            break;
        case 10:
            for(i=0;i<bytes/4;i++)
                ((uint32_t *)dest)[i]=ca_blend(((uint32_t *)source)[i],
                                               ((uint32_t *)dest)[i],
                                               ((uint32_t *)mask_bytes)[i]);
            break;
        }
    }
}
static unsigned long now_us(void)
{
    struct timeval t;gettimeofday(&t,0);
    return (unsigned long)t.tv_sec*1000000ul+(unsigned long)t.tv_usec;
}
static unsigned long measure(int id,int use_mmi,unsigned int bytes,unsigned int loops)
{
    unsigned int i;
    unsigned long start,end;
    for(i=0;i<CAP;i++)
    {
        source[i]=(uint8_t)((i*73+31)&255);
        dest[i]=(uint8_t)((i*17+49)&255);
        mask_bytes[i]=(uint8_t)((i*103+7)&255);
    }
    run_kernel(id,use_mmi,bytes); /* warmup, same for both paths */
    start=now_us();
    for(i=0;i<loops;i++)run_kernel(id,use_mmi,bytes);
    end=now_us();
    checksum^=((uint32_t *)dest)[(bytes/8)&((CAP/4)-1)];
    return end>=start?end-start:0;
}
void ee_mmi_bench_run(void)
{
    static const char *names[]=
    {"ADD","COPY","FILL","A8_IN","OVER_8888",
     "OVER_SOLID_CA","OVER_SOLID_A8","OVER_IMAGE_A8",
     "OVER_RGB565","SRC_8888_565","OVER_IMAGE_CA"};
    static const unsigned int sizes[]={64,256,1024,4096,16384};
    unsigned int kind,k,loops,bytes;
    unsigned long ctime,mtime;
    puts("BENCH_CSV,kernel,bytes,iterations,c_us,mmi_us,speedup");
    for(kind=0;kind<11;kind++)
        for(k=0;k<5;k++)
        {
            bytes=sizes[k];
            loops=1048576u/bytes;
            if(loops<32)loops=32;
            ctime=measure((int)kind,0,bytes,loops);
            mtime=measure((int)kind,1,bytes,loops);
            if(ctime && mtime)
                printf("BENCH_CSV,%s,%u,%u,%lu,%lu,%lu.%02lu\n",
                       names[kind],bytes,loops,ctime,mtime,
                       ctime/mtime,(ctime%mtime)*100ul/mtime);
            else
                printf("BENCH_CSV,%s,%u,%u,%lu,%lu,N/A\n",
                       names[kind],bytes,loops,ctime,mtime);
        }
    printf("BENCH_CHECKSUM,%08x\n",(unsigned int)checksum);
}
