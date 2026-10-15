# Filesystems

`fs/vfs.c` implements a RAM-only VFS tree. `fs/fat32/` parses FAT32 geometry, reads root-level 8.3 files from legacy ATA PIO devices and supports explicit root-file writes. `fatget` and `fatput` bridge single files between the RAM VFS and the persistent root directory.

The current FAT32 helper is not a general mounted VFS, has no LFN or subdirectory support, and writes are only host-tested against a simulated disk.
