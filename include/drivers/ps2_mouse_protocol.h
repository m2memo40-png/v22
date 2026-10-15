#ifndef FLUXMAX_PS2_MOUSE_PROTOCOL_H
#define FLUXMAX_PS2_MOUSE_PROTOCOL_H
#include <stdint.h>

typedef struct {
    int16_t dx;
    int16_t dy;
    uint8_t buttons; /* bit 0: left, bit 1: right, bit 2: middle */
} fluxmax_mouse_event_t;

typedef struct {
    uint8_t packet[3];
    uint8_t index;
} fluxmax_ps2_mouse_decoder_t;

void fluxmax_ps2_mouse_decoder_reset(fluxmax_ps2_mouse_decoder_t *decoder);
/* Feed one byte from a standard three-byte PS/2 mouse packet. Returns 1 when
 * a complete, non-overflow packet is decoded, 0 while collecting/for invalid data. */
int fluxmax_ps2_mouse_decode_byte(fluxmax_ps2_mouse_decoder_t *decoder,
                                 uint8_t byte, fluxmax_mouse_event_t *event);
#endif
