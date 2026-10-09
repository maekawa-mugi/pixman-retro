/* Same-ELF, benchmark-only EE scratchpad placement tournament.
 * Genuine Pixman MMI ADD/COPY/FILL assembler; not production dispatch.
 * Owns 0x70000000..0x70003fff (16 KiB SPR) exclusively. No DMA.
 */
#include "progress.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SPR_HALF 8192u
#define SPR_REPS 32u
#define SPR_SAMPLES 6u

/* Linker --wrap rewrites __real_* references to genuine assembler. */
void __real_ee_mmi_copy_quads(void *, const void *, unsigned);
void __real_ee_mmi_add_quads(void *, const void *, unsigned);
void __real_ee_mmi_fill_quads(void *, const void *, unsigned);
typedef void (*spr_kernel)(void *, const void *, unsigned);

static const spr_kernel kernels[3] = {
    __real_ee_mmi_copy_quads, __real_ee_mmi_add_quads,
    __real_ee_mmi_fill_quads
};
static const char *const names[3] = {"COPY","ADD","FILL"};
static const char *const modes[5] = {
    "ram","spr_src","spr_dst","spr_both","spr_xfer"
};
static uint8_t ram_src[SPR_HALF] __attribute__((aligned(16)));
static uint8_t ram_dst[SPR_HALF] __attribute__((aligned(16)));
static uint8_t initial[SPR_HALF] __attribute__((aligned(16)));
static uint8_t expected[SPR_HALF] __attribute__((aligned(16)));
static volatile uint32_t escape_sink;

static uint64_t median6(const uint64_t values[SPR_SAMPLES])
{
    uint64_t a[SPR_SAMPLES], v;
    unsigned i, j;
    for (i = 0; i < SPR_SAMPLES; ++i) a[i] = values[i];
    for (i = 1; i < SPR_SAMPLES; ++i) {
        v = a[i]; j = i;
        while (j && a[j-1] > v) { a[j] = a[j-1]; --j; }
        a[j] = v;
    }
    return (a[2] + a[3]) / 2u;
}

static uint64_t run_case(unsigned kernel, unsigned mode, unsigned bytes,
                         unsigned reps)
{
    uint8_t *ss = (uint8_t *)(uintptr_t)0x70000000u;
    uint8_t *sd = (uint8_t *)(uintptr_t)0x70002000u;
    const uint8_t *src = (mode == 1 || mode == 3) ? ss : ram_src;
    uint8_t *dst = (mode == 2 || mode == 3) ? sd : ram_dst;
    uint64_t start, elapsed;
    unsigned i;
    memcpy(ram_dst, initial, bytes);
    if (mode == 1 || mode == 3) memcpy(ss, ram_src, bytes);
    if (mode == 2 || mode == 3) memcpy(sd, initial, bytes);
    start = ps2_ticks_now();
    for (i = 0; i < reps; ++i) {
        if (mode == 4) {
            /* Account for full ingress and egress on every invocation.
             * Destination is preserved between repetitions for ADD. */
            memcpy(ss, ram_src, bytes);
            memcpy(sd, ram_dst, bytes);
            kernels[kernel](sd, ss, bytes / 16u);
            memcpy(ram_dst, sd, bytes);
        } else {
            kernels[kernel](dst, src, bytes / 16u);
        }
    }
    elapsed = ps2_ticks_now() - start;
    if (mode == 2 || mode == 3) memcpy(ram_dst, sd, bytes);
    escape_sink ^= ram_dst[bytes - 1u];
    return elapsed;
}

int ps2_spr_benchmark(void)
{
    static const unsigned lengths[] = {64u,256u,1024u,4096u,8192u};
    uint64_t raw[5][SPR_SAMPLES], med[5];
    unsigned k, z, m, s, step, i;
    unsigned pass = 0;
    for (i = 0; i < SPR_HALF; ++i) {
        ram_src[i] = (uint8_t)(i * 47u + (i >> 3) + 23u);
        initial[i] = (uint8_t)(i * 13u + 9u);
    }
    puts("PIXMAN_SPR_META,R5900,ADD_COPY_FILL,SPR16K,6samples,32reps");
    for (k = 0; k < 3; ++k)
        for (z = 0; z < sizeof(lengths)/sizeof(lengths[0]); ++z) {
            unsigned bytes=lengths[z];
            /* Full-output check before any timing for this workload. */
            run_case(k,0,bytes,SPR_REPS);
            memcpy(expected,ram_dst,bytes);
            for (m = 1; m < 5; ++m) {
                run_case(k,m,bytes,SPR_REPS);
                if (memcmp(expected,ram_dst,bytes)) {
                    printf("PIXMAN_SPR_FAIL,%s,%u,%s,output\n",
                           names[k],bytes,modes[m]);
                    return 1;
                }
            }
            for (s = 0; s < SPR_SAMPLES; ++s)
                for (step = 0; step < 5; ++step) {
                    m = (s + step) % 5u;
                    raw[m][s] = run_case(k,m,bytes,SPR_REPS);
                    if (!raw[m][s] || memcmp(expected,ram_dst,bytes)) {
                        printf("PIXMAN_SPR_FAIL,%s,%u,%s,sample%u\n",
                               names[k],bytes,modes[m],s);
                        return 1;
                    }
                }
            for (m = 0; m < 5; ++m) {
                med[m] = median6(raw[m]);
                printf("PIXMAN_SPR,%s,%u,%s,%llu,%.5f\n",
                       names[k],bytes,modes[m],
                       (unsigned long long)med[m],
                       (double)med[0]/(double)med[m]);
            }
            ++pass;
        }
    printf("PIXMAN_SPR_RESULT,PASS,cases=%u,sink=%lu\n",
           pass,(unsigned long)escape_sink);
    fflush(stdout);
    return 0;
}
