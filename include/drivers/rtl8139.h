#ifndef FLUXMAX_RTL8139_H
#define FLUXMAX_RTL8139_H
#include <stddef.h>
#include <stdint.h>
typedef void (*fluxmax_rtl8139_rx_fn)(const uint8_t *frame, size_t length, void *context);
/* Polling-mode RTL8139 driver. It is deliberately scoped to legacy RTL8139 hardware. */
int rtl8139_probe(void);
int rtl8139_init(uint8_t bus, uint8_t device, uint8_t function);
void rtl8139_set_rx_handler(fluxmax_rtl8139_rx_fn handler, void *context);
void rtl8139_poll(void);
int rtl8139_send_frame(const uint8_t *frame, size_t length);
int rtl8139_is_up(void);
const uint8_t *rtl8139_mac_address(void);
uint64_t rtl8139_rx_count(void);
uint64_t rtl8139_tx_count(void);
#endif
