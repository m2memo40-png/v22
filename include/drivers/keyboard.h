#ifndef FLUXMAX_KEYBOARD_H
#define FLUXMAX_KEYBOARD_H
#include <stdint.h>
int ps2_keyboard_init(void);
void ps2_keyboard_irq(void);
int ps2_keyboard_read_char(char *out);
#endif
