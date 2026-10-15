#ifndef FLUXMAX_PIC_H
#define FLUXMAX_PIC_H
#include <stdint.h>
void pic_init_masked(void);
void pic_eoi(uint8_t irq);
void pic_set_mask(uint8_t irq, int masked);
#endif
