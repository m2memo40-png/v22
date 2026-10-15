#ifndef FLUXMAX_DRIVERS_PCI_H
#define FLUXMAX_DRIVERS_PCI_H
#include <stdint.h>
typedef struct { uint8_t bus, device, function; uint16_t vendor_id, device_id; uint8_t class_code, subclass, programming_interface, revision; } fluxmax_pci_device_t;
typedef uint32_t (*fluxmax_pci_cfg_read_fn)(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, void *context);
typedef int (*fluxmax_pci_visit_fn)(const fluxmax_pci_device_t *device, void *context);
int fluxmax_pci_scan(fluxmax_pci_cfg_read_fn read_config, fluxmax_pci_visit_fn visit, void *context);
#endif
