#ifndef FLUXMAX_PS2_MOUSE_H
#define FLUXMAX_PS2_MOUSE_H
#include "drivers/ps2_mouse_protocol.h"

/* Returns 0 when a standard PS/2 auxiliary mouse was initialized. */
int ps2_mouse_init(void);
void ps2_mouse_irq(void);
int ps2_mouse_is_available(void);
/* Single-producer interrupt queue / single-consumer foreground API. */
int ps2_mouse_read_event(fluxmax_mouse_event_t *event);
#endif
