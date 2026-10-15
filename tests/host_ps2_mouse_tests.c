#include "drivers/ps2_mouse_protocol.h"
#include <stdio.h>
#include <stdint.h>

static unsigned passed, failed;
#define CHECK(expr, name) do { \
    if (expr) { ++passed; printf("[PASS] %s\n", name); } \
    else { ++failed; printf("[FAIL] %s\n", name); } \
} while (0)

int main(void) {
    fluxmax_ps2_mouse_decoder_t d;
    fluxmax_mouse_event_t e = {0,0,0};
    fluxmax_ps2_mouse_decoder_reset(&d);
    CHECK(d.index == 0, "decoder reset clears packet index");
    CHECK(fluxmax_ps2_mouse_decode_byte(&d, 0x00, &e) == 0 && d.index == 0,
          "bytes without packet sync bit are ignored");
    CHECK(fluxmax_ps2_mouse_decode_byte(&d, 0x09, &e) == 0 && d.index == 1,
          "first packet byte starts synchronized packet");
    CHECK(fluxmax_ps2_mouse_decode_byte(&d, 0x05, &e) == 0,
          "second packet byte does not publish incomplete event");
    CHECK(fluxmax_ps2_mouse_decode_byte(&d, 0x02, &e) == 1,
          "complete packet publishes event");
    CHECK(e.dx == 5 && e.dy == -2 && e.buttons == 1,
          "positive X, inverted Y and left button decoded correctly");

    fluxmax_ps2_mouse_decoder_reset(&d);
    (void)fluxmax_ps2_mouse_decode_byte(&d, 0x0A, &e);
    (void)fluxmax_ps2_mouse_decode_byte(&d, (uint8_t)-3, &e);
    CHECK(fluxmax_ps2_mouse_decode_byte(&d, (uint8_t)-4, &e) == 1 && e.dx == -3 && e.dy == 4,
          "negative movement is sign-extended correctly");

    fluxmax_ps2_mouse_decoder_reset(&d);
    (void)fluxmax_ps2_mouse_decode_byte(&d, 0x48, &e); /* X overflow flag */
    (void)fluxmax_ps2_mouse_decode_byte(&d, 0x7F, &e);
    CHECK(fluxmax_ps2_mouse_decode_byte(&d, 0x7F, &e) == 0 && d.index == 0,
          "overflowed packet is discarded without cursor jump");

    fluxmax_ps2_mouse_decoder_reset(&d);
    (void)fluxmax_ps2_mouse_decode_byte(&d, 0x08, &e);
    (void)fluxmax_ps2_mouse_decode_byte(&d, 1, &e);
    (void)fluxmax_ps2_mouse_decode_byte(&d, 2, &e);
    CHECK(fluxmax_ps2_mouse_decode_byte(&d, 0x09, &e) == 0 && d.index == 1,
          "decoder resynchronizes on the next packet header");

    CHECK(fluxmax_ps2_mouse_decode_byte(0, 0x08, &e) == 0,
          "null decoder is safely rejected");
    CHECK(fluxmax_ps2_mouse_decode_byte(&d, 0x08, 0) == 0,
          "null event output is safely rejected");
    printf("PS/2 mouse protocol tests: %u passed, %u failed.\n", passed, failed);
    return failed ? 1 : 0;
}
