#ifndef FLUXMAX_PS2_KEYMAP_H
#define FLUXMAX_PS2_KEYMAP_H
#include <stdint.h>

typedef struct {
    uint8_t left_shift;
    uint8_t right_shift;
    uint8_t caps_lock;
    uint8_t caps_down;
    uint8_t extended_prefix;
    uint8_t pause_bytes;
} fluxmax_ps2_keymap_state_t;

void fluxmax_ps2_keymap_reset(fluxmax_ps2_keymap_state_t *state);
/* Feed one set-1 scan byte. Returns 1 iff a text character was produced. */
int fluxmax_ps2_keymap_feed(fluxmax_ps2_keymap_state_t *state, uint8_t scan_code, char *out);
#endif
