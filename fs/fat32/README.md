# FAT32 support

- `fat32_geometry.c` validates FAT32 BPB geometry and cluster bounds.
- `fat32_ro.c` mounts the first FAT32 MBR partition or a superfloppy BPB for reading via the legacy ATA PIO driver. It lists the root directory and streams regular root-level files using classic 8.3 short names.
- `fat32_rw.c` adds explicit root-file create/replace writes with mirrored FAT updates, cluster allocation, data writes and cache flush. It only supports short 8.3 names in the root directory; it is not a general mount and is not integrated into VFS.

Shell commands: `fatls [DEVICE]`, `fatcat DEVICE NAME.EXT`, `fatget DEVICE NAME.EXT /vfs/path`, `fatput DEVICE NAME.EXT /vfs/path`. `fatput` modifies the disk. Use a disposable virtual disk and back up important data. Seven host simulated-disk tests cover the writer. Hardware PIO behavior remains to be verified under QEMU with an attached disk and on compatible real hardware.
