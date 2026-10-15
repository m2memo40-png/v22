#ifndef FLUXMAX_PIT_H
#define FLUXMAX_PIT_H
#include <stdint.h>
void pit_init(uint32_t hz);
uint64_t pit_ticks(void);
void pit_tick_increment(void);
#endif
