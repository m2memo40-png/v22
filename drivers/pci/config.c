#include "drivers/pci/pci.h"
#include "arch/x86_64/io.h"
#include <stdint.h>

#define PCI_CONFIG_ADDRESS 0x0CF8u
#define PCI_CONFIG_DATA    0x0CFCu
uint32_t fluxmax_pci_read_config32(uint8_t bus,uint8_t device,uint8_t function,uint8_t offset,void *context) {
    (void)context;
    if(device>=32u || function>=8u) return 0xFFFFFFFFu;
    uint32_t address=0x80000000u | ((uint32_t)bus<<16) | ((uint32_t)device<<11) |
                     ((uint32_t)function<<8) | ((uint32_t)offset & 0xFCu);
    outl(PCI_CONFIG_ADDRESS,address);
    return inl(PCI_CONFIG_DATA);
}

int fluxmax_pci_write_config32(uint8_t bus,uint8_t device,uint8_t function,uint8_t offset,uint32_t value,void *context) {
    (void)context;
    if(device>=32u || function>=8u) return -1;
    uint32_t address=0x80000000u | ((uint32_t)bus<<16) | ((uint32_t)device<<11) |
                     ((uint32_t)function<<8) | ((uint32_t)offset & 0xFCu);
    outl(PCI_CONFIG_ADDRESS,address);
    outl(PCI_CONFIG_DATA,value);
    return 0;
}
