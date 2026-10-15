#ifndef FLUXMAX_E1000_H
#define FLUXMAX_E1000_H
#include <stddef.h>
#include <stdint.h>
typedef void (*fluxmax_e1000_rx_fn)(const uint8_t *frame, size_t length, void *context);
/* Polling-mode driver for legacy Intel 8254x/E1000-compatible PCI adapters. */
int e1000_probe(void);
void e1000_set_rx_handler(fluxmax_e1000_rx_fn handler, void *context);
void e1000_poll(void);
int e1000_send_frame(const uint8_t *frame, size_t length);
int e1000_is_up(void);
const uint8_t *e1000_mac_address(void);
uint64_t e1000_rx_count(void);
uint64_t e1000_tx_count(void);
#endif
