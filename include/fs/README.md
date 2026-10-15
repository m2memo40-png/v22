# VFS and persistent storage interfaces

`fs/vfs.c` implements an in-memory tree with absolute-path lookup, create, list, read, write, and unlink. It uses the kernel heap and is initialized during boot. RAM VFS data is not persistent across reboot.

Separately, `fs/fat32/fat32_ro.c` can read the root directory and 8.3-name files from a FAT32 partition on a legacy IDE/ATA disk using `drivers/storage/ata_pio.c`. This is read-only and currently exposed through shell commands rather than mounted into the VFS.
