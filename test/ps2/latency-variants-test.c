/* One-run differential + latency experiment for uploaded PS2 v5 harness.
 * No dispatch changes: candidates remain isolated until proven.
 * PS2SDK n32/o32 via progress timer, native preflight with scalar models.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef PS2_TEST_HOST
#include <stdlib.h>
#endif
#include "progress.h"

#define MAX_BLOCKS 64
#define SRC_PIXELS (MAX_BLOCKS * 4)
#define GUARD 16
#define REPS 192

extern void ee_mmi_nearest_opaque_quads(uint32_t *,const uint32_t *,
                                         unsigned int,const uint32_t *);
extern void ee_mmi_nearest_opaque_u2_quads(uint32_t *,const uint32_t *,
                                            unsigned int,const uint32_t *);
extern void ee_mmi_nearest_opaque_u4_quads(uint32_t *,const uint32_t *,
                                            unsigned int,const uint32_t *);
extern void ee_mmi_bilinear_hsum_quads(uint16_t *,const uint32_t *,
                                       const uint32_t *,const uint16_t *,
                                       unsigned int);
extern void ee_mmi_bilinear_hsum_sched_quads(uint16_t *,const uint32_t *,
                                             const uint32_t *,const uint16_t *,
                                             unsigned int);
extern void ee_mmi_bilinear_hsum_predecode_quads(uint16_t *,const uint32_t *,
                                                 const uint32_t *,const uint16_t *,
                                                 unsigned int);

static uint32_t src[SRC_PIXELS+GUARD] __attribute__((aligned(16)));
static uint32_t right[SRC_PIXELS+GUARD] __attribute__((aligned(16)));
static uint32_t saved_src[SRC_PIXELS+GUARD] __attribute__((aligned(16)));
static uint32_t saved_right[SRC_PIXELS+GUARD] __attribute__((aligned(16)));
static uint32_t nearest_dst[SRC_PIXELS+GUARD] __attribute__((aligned(16)));
static uint32_t nearest_expected[SRC_PIXELS+GUARD] __attribute__((aligned(16)));
static uint16_t hsum_dst[(SRC_PIXELS+GUARD)*4] __attribute__((aligned(16)));
static uint16_t hsum_expected[(SRC_PIXELS+GUARD)*4] __attribute__((aligned(16)));
static uint16_t coeff[MAX_BLOCKS*32] __attribute__((aligned(16)));
static uint16_t saved_coeff[MAX_BLOCKS*32] __attribute__((aligned(16)));
static const uint32_t opaque[4] __attribute__((aligned(16))) =
{0xff000000u,0xff000000u,0xff000000u,0xff000000u};

static uint32_t state=0x539e13a7u;
static volatile uint32_t sink;
static uint32_t random32(void)
{
    uint32_t x=state;
    x^=x<<13; x^=x>>17; x^=x<<5;
    return state=x;
}
static void init(unsigned int n)
{
    unsigned int i,j;
    for(i=0;i<SRC_PIXELS+GUARD;i++)
    {
        src[i]=random32();
        right[i]=random32();
        nearest_dst[i]=random32();
        nearest_expected[i]=nearest_dst[i];
    }
    for(i=0;i<(SRC_PIXELS+GUARD)*4;i++)
    {
        hsum_dst[i]=(uint16_t)random32();
        hsum_expected[i]=hsum_dst[i];
    }
    for(i=0;i<MAX_BLOCKS*4;i++)
    {
        unsigned int w=(unsigned int)((i*117u+n*79u)%257u);
        unsigned int offset=(i/4)*32+((i&2)?8:0)+((i&1)?4:0);
        for(j=0;j<4;j++)
        {
            coeff[offset+j]=(uint16_t)(256-w);
            coeff[offset+16+j]=(uint16_t)w;
        }
        if(i<n*4)
        {
            uint32_t a=src[i],b=right[i];
            nearest_expected[i]=a|0xff000000u;
            for(j=0;j<4;j++)
            {
                unsigned int sh=j*8;
                hsum_expected[4*i+j]=(uint16_t)(
                    (((a>>sh)&255u)*(256-w))+(((b>>sh)&255u)*w));
            }
        }
    }
    memcpy(saved_src,src,sizeof(src));
    memcpy(saved_right,right,sizeof(right));
    memcpy(saved_coeff,coeff,sizeof(coeff));
}
static int assert_inputs(const char *what,unsigned int count)
{
    if(memcmp(src,saved_src,sizeof(src)) ||
       memcmp(right,saved_right,sizeof(right)) ||
       memcmp(coeff,saved_coeff,sizeof(coeff)))
    {
        printf("LATENCY_FAIL,%s,blocks=%u,source_or_coeff_corrupt\n",what,count);
        return 1;
    }
    return 0;
}
typedef void (*nearest_kernel_t)(uint32_t *,const uint32_t *,unsigned int,
                                  const uint32_t *);
typedef void (*hsum_kernel_t)(uint16_t *,const uint32_t *,const uint32_t *,
                               const uint16_t *,unsigned int);
static const char *const nearest_names[3]={"u1","u2","u4"};
static nearest_kernel_t const nearest_kernels[3]={
    ee_mmi_nearest_opaque_quads,
    ee_mmi_nearest_opaque_u2_quads,
    ee_mmi_nearest_opaque_u4_quads
};
static const char *const hsum_names[3]={"baseline","scheduled","predecode"};
static hsum_kernel_t const hsum_kernels[3]={
    ee_mmi_bilinear_hsum_quads,ee_mmi_bilinear_hsum_sched_quads,
    ee_mmi_bilinear_hsum_predecode_quads
};

static int verify_nearest(unsigned int n,unsigned int which)
{
    unsigned int i;
    init(n);
    nearest_kernels[which](nearest_dst,src,n,opaque);
#ifdef PS2_TEST_HOST
    if(which==2 && n==7 && getenv("PS2_LATENCY_TEST_FAULT"))
        nearest_dst[0]^=1u;
#endif
    if(memcmp(nearest_dst,nearest_expected,sizeof(nearest_dst)))
    {
        for(i=0;i<SRC_PIXELS+GUARD;i++)
            if(nearest_dst[i]!=nearest_expected[i])
            {
                printf("LATENCY_FAIL,nearest_%s,blocks=%u,pixel=%u,expected=%08lx,got=%08lx\n",
                       nearest_names[which],n,i,
                       (unsigned long)nearest_expected[i],
                       (unsigned long)nearest_dst[i]);
                break;
            }
        return 1;
    }
    return assert_inputs(nearest_names[which],n);
}
static int verify_hsum(unsigned int n,unsigned int which)
{
    unsigned int i;
    init(n);
    hsum_kernels[which](hsum_dst,src,right,coeff,n);
    if(memcmp(hsum_dst,hsum_expected,sizeof(hsum_dst)))
    {
        for(i=0;i<(SRC_PIXELS+GUARD)*4;i++)
            if(hsum_dst[i]!=hsum_expected[i])
            {
                printf("LATENCY_FAIL,hsum_%s,blocks=%u,lane=%u,expected=%u,got=%u\n",
                       hsum_names[which],n,i,
                       (unsigned int)hsum_expected[i],(unsigned int)hsum_dst[i]);
                break;
            }
        return 1;
    }
    return assert_inputs(hsum_names[which],n);
}
static uint64_t nearest_time(unsigned int n,unsigned int which)
{
    unsigned int i;
    uint64_t start;
    /* All candidates at the same size receive identical source bytes. */
    state=0x69acf0e1u ^ (n*0x9e3779b9u);
    init(n);
    nearest_kernels[which](nearest_dst,src,n,opaque); /* warm up */
    start=ps2_ticks_now();
    for(i=0;i<REPS;i++)
        nearest_kernels[which](nearest_dst,src,n,opaque);
    sink^=nearest_dst[n*4-1];
    return ps2_elapsed_us(start);
}
static uint64_t hsum_time(unsigned int n,unsigned int which)
{
    unsigned int i;
    uint64_t start;
    /* The original and scheduled PMULTH kernels see the same inputs. */
    state=0x2b31d417u ^ (n*0x9e3779b9u);
    init(n);
    hsum_kernels[which](hsum_dst,src,right,coeff,n);
    start=ps2_ticks_now();
    for(i=0;i<REPS;i++)
        hsum_kernels[which](hsum_dst,src,right,coeff,n);
    sink^=hsum_dst[n*16-1];
    return ps2_elapsed_us(start);
}

/* Called once by test/ps2/main.c; zero quads are tested but not timed. */
int ps2_latency_run(void)
{
    static const unsigned int sizes[]={4,16,64};
    unsigned int n,k,j,errors=0;
    for(n=0;n<=MAX_BLOCKS;n++)
    {
        for(k=0;k<3;k++)
            errors+=(unsigned int)verify_nearest(n,k);
        for(k=0;k<3;k++)
            errors+=(unsigned int)verify_hsum(n,k);
    }
    printf("LATENCY_VERIFY,%s,nearest=%u,hsum=%u,failures=%u\n",
           errors?"FAIL":"PASS",3u*(MAX_BLOCKS+1),
           3u*(MAX_BLOCKS+1),errors);
    if(errors)
        return 1; /* Never print performance for incorrect kernels. */

    printf("LATENCY_CSV,kernel,variant,bytes,repeats,elapsed_us\n");
    for(j=0;j<3;j++)
    {
        n=sizes[j];
        for(k=0;k<3;k++)
        {
            uint64_t elapsed=nearest_time(n,k);
            printf("LATENCY_CSV,nearest,%s,%u,%u,%lu\n",
                   nearest_names[k],n*16u,(unsigned int)REPS,
                   (unsigned long)elapsed);
        }
        for(k=0;k<3;k++)
        {
            uint64_t elapsed=hsum_time(n,k);
            printf("LATENCY_CSV,bilinear_hsum,%s,%u,%u,%lu\n",
                   hsum_names[k],n*16u,(unsigned int)REPS,
                   (unsigned long)elapsed);
        }
    }
    printf("LATENCY_RESULT,PASS,checksum=%08lx\n",(unsigned long)sink);
    return 0;
}
