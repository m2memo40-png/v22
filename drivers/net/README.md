# Network drivers

`rtl8139.c` implements a polling driver for PCI vendor/device `10EC:8139`: PCI I/O-space and bus-master enabling, controller reset, MAC readout, physically contiguous 32-bit DMA RX/TX buffers, an 8 KiB receive ring, bounded receive polling, and frame submission using four transmit descriptors.

`e1000.c` adds polling-mode support for Intel legacy 8254x/E1000-compatible devices, including the QEMU `e1000` (82540EM) model. It uses PCI BAR0 MMIO, bounded reset and transmit waits, legacy RX/TX descriptor rings, packet buffers checked for physical contiguity, MAC-register/EEPROM lookup, and interrupt masking. `net/net.c` prefers E1000 when present, then falls back to RTL8139.

Both drivers feed the same Ethernet/ARP/IPv4/ICMP/UDP/DHCP/DNS/TCP stack. They use bounded polling and assume DMA-visible memory. Neither driver supports Wi-Fi, USB networking, VirtIO-net, modern e1000e-only devices, or every vendor-specific NIC. Native hardware confirmation is required; the GitHub workflow now includes QEMU smoke tests for both emulated NIC models.
