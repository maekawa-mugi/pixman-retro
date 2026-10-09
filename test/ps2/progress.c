#include "progress.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef PS2_TEST_HOST
#include <debug.h>
#include <timer.h>
#else
#include <sys/time.h>
#define scr_printf(...) ((void)0)
#define scr_setfontcolor(c) ((void)0)
#define scr_setXY(x,y) ((void)0)
#define scr_clear() ((void)0)
#endif
#define GREEN 0x0000ff00u
#define RED 0x000000ffu
#define WHITE 0x00ffffffu
#define YELLOW 0x0000ffffu
#define SUITES 17
static const char *const names[SUITES]={
 "Core kernels","Solid CA","Solid A8","RGB565 kernels","Image A8","Image CA",
 "Nearest / 2tap","Bilinear 4tap","Large / scaled API","IN 65536 pairs",
 "A8 ADD/IN/SRC","8888 blending","Masks/solid/CA","RGB/BGR565","Scaled OVER",
 "Fill/BLT","OVER combiner"
};
/* Each row owns its result and duration; WAIT, RUNNING, completed. */
static struct cell {uint64_t us;unsigned int bad;int state;} cells[SUITES][2];
static uint64_t last_refresh;
static unsigned int checks,groups,failed_groups,batch_done,batch_total;
static int current=-1,kernel_variant;
static char stage[64];
uint64_t ps2_ticks_now(void)
{
#ifndef PS2_TEST_HOST
    return GetTimerSystemTime();
#else
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
#endif
}
uint64_t ps2_elapsed_us(uint64_t start)
{
    uint64_t delta = ps2_ticks_now() - start;
#ifndef PS2_TEST_HOST
    u32 seconds, microseconds;
    TimerBusClock2USec(delta, &seconds, &microseconds);
    return (uint64_t)seconds * 1000000 + microseconds;
#else
    return delta;
#endif
}

int ps2_kernel_variant(void){return kernel_variant;}
void ps2_kernel_set_variant(int variant){kernel_variant=variant;}
static void draw_row(int row)
{
    int side;
    scr_setfontcolor(WHITE);scr_setXY(0,5+row);
    scr_printf("%-20.20s",names[row]);
    for(side=0;side<2;++side)
    {
        struct cell *c=&cells[row][side];
        scr_setXY(20+24*side,5+row);
        if(c->state==0){scr_setfontcolor(WHITE);scr_printf("%c: WAIT                 ",'A'+side);}
        else if(c->state==1 && !c->bad)
        {scr_setfontcolor(YELLOW);scr_printf("%c: RUNNING              ",'A'+side);}
        else
        {
            scr_setfontcolor(c->bad?RED:GREEN);
            scr_printf("%c: %c (%lu.%03lu ms)     ",'A'+side,c->bad?'X':'O',
                (unsigned long)(c->us/1000),(unsigned long)(c->us%1000));
        }
    }
}
static void refresh(int force)
{
    if(!force && ps2_elapsed_us(last_refresh)<100000)return;
    last_refresh=ps2_ticks_now();
    scr_setfontcolor(WHITE);scr_setXY(0,3);scr_printf("ACTIVE %-60.60s",stage);
    scr_setXY(0,4);
    if(batch_total)scr_printf("PROGRESS %5u/%-5u  CHECKS %-8u FAIL GROUPS %-3u   ",batch_done,batch_total,checks,failed_groups);
    else scr_printf("PROGRESS %5u/%-5d  CHECKS %-8u FAIL GROUPS %-3u   ",groups,SUITES,checks,failed_groups);
    if(current>=0)draw_row(current);
#ifdef PS2_TEST_HOST
    if(getenv("PS2_PROGRESS_TRACE") && current>=0)
        printf("LIVE_SUITE,%s,A=%s,B=%s,checks=%u\n",names[current],
            cells[current][0].bad?"X":cells[current][0].state==2?"O":cells[current][0].state==1?"RUNNING":"WAIT",
            cells[current][1].bad?"X":cells[current][1].state==2?"O":cells[current][1].state==1?"RUNNING":"WAIT",checks);
#endif
}
void ps2_progress_init(void)
{
    int i;
    memset(cells,0,sizeof(cells));checks=groups=failed_groups=batch_done=batch_total=0;
    kernel_variant=0;current=-1;snprintf(stage,sizeof(stage),"Starting");last_refresh=ps2_ticks_now();
    scr_clear();scr_setfontcolor(WHITE);
    scr_setXY(0,0);scr_printf("PIXMAN RETRO | PS2 EE MMI | VALIDATION + BENCHMARK");
    scr_setXY(0,1);scr_printf("A=MMI   B=scalar C   PASS=O   FAIL=X   time=milliseconds");
    scr_setXY(0,2);scr_printf("%-20s%-24s%-24s","KERNEL / TEST","A: TEST + TIME","B: TEST + TIME");
    for(i=0;i<SUITES;++i)draw_row(i);
    refresh(1);
}
void ps2_progress_begin_group(const char *name)
{
    int i;
    for(i=0;i<SUITES;++i)if(!strcmp(name,names[i]))break;
    current=i<SUITES?i:-1;
    snprintf(stage,sizeof(stage),"%s",name);batch_done=batch_total=0;
    /* API variants are interleaved to reuse each generated input. */
    if(current>=8)cells[current][0].state=cells[current][1].state=1;
    refresh(1);
}
void ps2_progress_stage(const char *name)
{snprintf(stage,sizeof(stage),"%s",name);batch_done=batch_total=0;refresh(1);}
void ps2_progress_pulse(unsigned int done,unsigned int total)
{batch_done=done;batch_total=total;refresh(0);}
void ps2_progress_variant_begin(int variant)
{if(current>=0)cells[current][variant].state=1;batch_done=batch_total=0;refresh(1);}
void ps2_progress_variant_end(int variant,int failed,uint64_t elapsed)
{
    if(current<0)return;
    cells[current][variant].us=elapsed;cells[current][variant].bad=failed!=0;
    cells[current][variant].state=2;refresh(1);
    printf("SUITE_AB_RESULT,%s,%c,%c,%lu.%03lu ms\n",names[current],'A'+variant,failed?'X':'O',
        (unsigned long)(elapsed/1000),(unsigned long)(elapsed%1000));fflush(stdout);
}
void ps2_progress_record(uint64_t a,uint64_t b,int bad_a,int bad_b)
{
    if(current>=0)
    {
        cells[current][0].us+=a;cells[current][1].us+=b;
        cells[current][0].bad+=bad_a!=0;cells[current][1].bad+=bad_b!=0;
    }
    ++checks;refresh(bad_a||bad_b);
}
void ps2_progress_group(const char *name,int failed,uint64_t elapsed)
{
    int side;
    /* Group-level errors with no pixel mismatch used to display O/O.
     * Neither side can be attributed, so conservatively mark both X.
     */
    if (current >= 0 && failed &&
        !cells[current][0].bad && !cells[current][1].bad)
    {
        cells[current][0].bad = cells[current][1].bad = 1;
        printf("GROUP_UNATTRIBUTED_FAILURE,%s\n", name);
    }
    if(current>=0)
    {
        for(side=0;side<2;++side)
        {
            struct cell *c=&cells[current][side];
            if(c->state!=2)
            {
                c->state=2;
                printf("SUITE_AB_RESULT,%s,%c,%c,%lu.%03lu ms\n",name,'A'+side,c->bad?'X':'O',
                    (unsigned long)(c->us/1000),(unsigned long)(c->us%1000));
            }
        }
    }
    ++groups;failed_groups+=failed!=0;batch_done=batch_total;refresh(1);
    printf("GROUP_RESULT,%s,%s,%lu.%03lu ms\n",name,failed?"FAIL":"PASS",
        (unsigned long)(elapsed/1000),(unsigned long)(elapsed%1000));fflush(stdout);
}
void ps2_progress_kernel_group(const char *name,int failed,uint64_t elapsed)
{ps2_progress_group(name,failed,elapsed);}
void ps2_progress_finish(int failures)
{
    unsigned int bad_a=0,bad_b=0;int i;
    snprintf(stage,sizeof(stage),"%s",failures?"COMPLETE - FAIL":"COMPLETE - PASS");refresh(1);
    for(i=0;i<SUITES;++i){draw_row(i);bad_a+=cells[i][0].bad;bad_b+=cells[i][1].bad;}
    scr_setXY(0,23);scr_setfontcolor(failures?RED:GREEN);
    scr_printf("RESULT: %-4s | %2u/%2d GROUPS | %u CHECKS | %d FAILURES    ",
        failures?"FAIL":"PASS",groups,SUITES,checks,failures);
    printf("INTEGRATED_RESULT,%s,checks=%u,groups=%u,A_bad=%u,B_bad=%u\n",
        failures?"FAIL":"PASS",checks,groups,bad_a,bad_b);fflush(stdout);
}
