# Drivers

Implemented subset: PCI config mechanism #1 enumeration, PS/2 keyboard input, legacy ATA PIO LBA28 reads/writes/flush, Limine framebuffer and COM1.

Not implemented: AHCI/NVMe/USB, ACPI/APIC management, complete NIC coverage (only initial RTL8139 and legacy E1000 polling paths), VirtIO networking, mouse HID, audio/GPU acceleration and broad device compatibility.
