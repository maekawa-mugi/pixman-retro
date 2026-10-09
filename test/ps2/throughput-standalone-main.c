/* Native-only throughput test driver; no EE instruction or ABI inference. */
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
extern int ps2_throughput_run(void);
uint64_t ps2_ticks_now(void) {
    struct timeval tv; gettimeofday(&tv,0);
    return (uint64_t)tv.tv_sec*1000000u+(uint64_t)tv.tv_usec;
}
uint64_t ps2_elapsed_us(uint64_t start) {
    return ps2_ticks_now()-start;
}
int main(void) {
    int errors=ps2_throughput_run();
    printf("THROUGHPUT_HOST_RESULT,%s\n",errors?"FAIL":"PASS");
    return errors?1:0;
}
