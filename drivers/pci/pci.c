#include "drivers/pci/pci.h"

static uint32_t cfg(fluxmax_pci_cfg_read_fn read_config, uint8_t b, uint8_t d, uint8_t f, uint8_t offset, void *ctx) {
    return read_config(b, d, f, offset, ctx);
}

static int scan_function(fluxmax_pci_cfg_read_fn read_config, fluxmax_pci_visit_fn visit,
                         void *ctx, uint8_t bus, uint8_t device, uint8_t function) {
    uint32_t id = cfg(read_config, bus, device, function, 0, ctx);
    uint16_t vendor = (uint16_t)id;
    uint16_t product = (uint16_t)(id >> 16);
    if (vendor == 0xFFFFu) return 0;
    uint32_t class_reg = cfg(read_config, bus, device, function, 8, ctx);
    fluxmax_pci_device_t entry = {
        .bus = bus, .device = device, .function = function,
        .vendor_id = vendor, .device_id = product,
        .revision = (uint8_t)class_reg,
        .programming_interface = (uint8_t)(class_reg >> 8),
        .subclass = (uint8_t)(class_reg >> 16),
        .class_code = (uint8_t)(class_reg >> 24)
    };
    return visit(&entry, ctx);
}

int fluxmax_pci_scan(fluxmax_pci_cfg_read_fn read_config, fluxmax_pci_visit_fn visit, void *context) {
    if (!read_config || !visit) return -1;
    for (uint16_t bus = 0; bus < 256; ++bus) {
        for (uint8_t device = 0; device < 32; ++device) {
            uint32_t header = cfg(read_config, (uint8_t)bus, device, 0, 0x0C, context);
            (void)header;
            uint32_t id = cfg(read_config, (uint8_t)bus, device, 0, 0, context);
            if ((uint16_t)id == 0xFFFFu) continue;
            uint32_t header_type = cfg(read_config, (uint8_t)bus, device, 0, 0x0C, context);
            uint8_t count = ((header_type >> 23) & 1u) ? 8u : 1u;
            for (uint8_t function = 0; function < count; ++function) {
                int rc = scan_function(read_config, visit, context, (uint8_t)bus, device, function);
                if (rc) return rc;
            }
        }
    }
    return 0;
}
