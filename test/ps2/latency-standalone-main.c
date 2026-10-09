/* Native-only unit-test driver; uses deterministic reference models.
 * Not a substitute for MMI assembly, PS2SDK ABI, or real timing.
 */
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
extern int ps2_latency_run(void);
uint64_t ps2_ticks_now(void)
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (uint64_t)tv.tv_sec * 1000000u + (uint64_t)tv.tv_usec;
}
uint64_t ps2_elapsed_us(uint64_t start)
{
    return ps2_ticks_now() - start;
}
int main(void)
{
    int failure = ps2_latency_run();
    printf("LATENCY_HOST_RESULT,%s\n", failure ? "FAIL" : "PASS");
    return failure ? 1 : 0;
}
