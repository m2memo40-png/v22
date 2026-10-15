#include "drivers/ps2_mouse_protocol.h"

void fluxmax_ps2_mouse_decoder_reset(fluxmax_ps2_mouse_decoder_t *decoder) {
    if (!decoder) return;
    decoder->packet[0] = decoder->packet[1] = decoder->packet[2] = 0;
    decoder->index = 0;
}

int fluxmax_ps2_mouse_decode_byte(fluxmax_ps2_mouse_decoder_t *decoder,
                                 uint8_t byte, fluxmax_mouse_event_t *event) {
    if (!decoder || !event) return 0;
    if (decoder->index == 0) {
        /* Bit 3 is always one in the first byte, so it is our resync marker. */
        if ((byte & 0x08u) == 0) return 0;
        decoder->packet[0] = byte;
        decoder->index = 1;
        return 0;
    }
    if (decoder->index >= 3u) decoder->index = 0;
    decoder->packet[decoder->index++] = byte;
    if (decoder->index != 3u) return 0;
    decoder->index = 0;

    /* Standard PS/2 packets flag X/Y overflow in bits 6/7. Never turn an
     * overflowed displacement into a huge cursor jump. The button state is
     * intentionally only published with a complete valid movement packet. */
    if (decoder->packet[0] & 0xC0u) return 0;
    event->dx = (int8_t)decoder->packet[1];
    event->dy = (int16_t)-(int16_t)(int8_t)decoder->packet[2];
    event->buttons = (uint8_t)(decoder->packet[0] & 0x07u);
    return 1;
}
