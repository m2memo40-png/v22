#ifndef FLUXMAX_PCI_CONFIG_H
#define FLUXMAX_PCI_CONFIG_H
#include <stdint.h>
uint32_t fluxmax_pci_read_config32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, void *context);
int fluxmax_pci_write_config32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value, void *context);
#endif
