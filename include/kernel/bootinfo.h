#ifndef FLUXMAX_BOOTINFO_H
#define FLUXMAX_BOOTINFO_H
#include <stdint.h>
#include "limine_min.h"
extern struct limine_memmap_response *g_memmap;
extern struct limine_hhdm_response *g_hhdm;
extern struct limine_framebuffer *g_framebuffer;
void bootinfo_init(void);
uint64_t boot_hhdm_offset(void);
#endif
