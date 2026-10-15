#ifndef FLUXMAX_GDT_H
#define FLUXMAX_GDT_H
#include <stdint.h>
void gdt_init(void);
void tss_set_rsp0(uint64_t stack_top);
#endif
