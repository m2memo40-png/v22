# Storage drivers

`ata_pio.c` contains polling-based legacy IDE/ATA PIO identification, 512-byte LBA28 sector reads/writes and cache flush. Timeouts are bounded. FAT32 root-file read/write helpers are available through explicit shell commands.

Limitations: no AHCI, NVMe, USB mass storage, ATAPI/CD-ROM, DMA or LBA48; no general block-device registry or mounted filesystem integration; the FAT32 helper accepts root-directory short 8.3 names only. Hardware-controller behavior must be checked in QEMU with an appropriate IDE disk and on supported hardware.
