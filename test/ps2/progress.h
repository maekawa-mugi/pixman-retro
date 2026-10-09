#ifndef PS2_TEST_PROGRESS_H
#define PS2_TEST_PROGRESS_H
#include <stdint.h>
uint64_t ps2_ticks_now(void);
uint64_t ps2_elapsed_us(uint64_t start);
void ps2_progress_init(void);
void ps2_progress_begin_group(const char *name);
void ps2_progress_variant_begin(int variant);
void ps2_progress_variant_end(int variant, int failed, uint64_t elapsed);
int ps2_kernel_variant(void);
void ps2_kernel_set_variant(int variant);
void ps2_progress_stage(const char *name);
void ps2_progress_record(uint64_t a, uint64_t b, int bad_a, int bad_b);
void ps2_progress_pulse(unsigned int done, unsigned int total);
void ps2_progress_kernel_group(const char *name, int failed, uint64_t elapsed);
void ps2_progress_group(const char *name, int failed, uint64_t elapsed);
void ps2_progress_finish(int failures);
#endif
