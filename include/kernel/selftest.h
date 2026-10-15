#ifndef FLUXMAX_KERNEL_SELFTEST_H
#define FLUXMAX_KERNEL_SELFTEST_H
#include <stdint.h>
/* Run non-destructive kernel service tests. Returns the number of failed checks. */
uint32_t fluxmax_selftest_run(void);
#endif
