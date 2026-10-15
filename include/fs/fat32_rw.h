#ifndef FLUXMAX_FAT32_RW_H
#define FLUXMAX_FAT32_RW_H
#include <stddef.h>
#include <stdint.h>
#include "fs/fat32_ro.h"

/* Write or replace one regular file in the FAT32 root directory using an 8.3
 * short name. This is a disk-writing operation; call only on the intended disk.
 * Maximum supported file size is 16 MiB. Empty files are supported. */
int fluxmax_fat32_rw_write_root_file(const fluxmax_fat32_ro_volume_t *volume,
                                    const char *short_name,
                                    const void *data, size_t size);
#endif
