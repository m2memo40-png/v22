#ifndef FLUXMAX_FAT32_RO_H
#define FLUXMAX_FAT32_RO_H
#include <stddef.h>
#include <stdint.h>
#include "fs/fat32.h"

typedef struct fluxmax_fat32_ro_volume {
    uint32_t device_index;
    uint64_t partition_lba;
    fluxmax_fat32_geometry_t geometry;
} fluxmax_fat32_ro_volume_t;

typedef int (*fluxmax_fat32_ro_visit_fn)(const char *short_name, uint8_t attributes,
                                        uint32_t size, void *context);
typedef int (*fluxmax_fat32_ro_sink_fn)(const uint8_t *data, size_t length, void *context);

/* Mounts the first FAT32 partition on an ATA PIO disk, or a superfloppy BPB at LBA 0. */
int fluxmax_fat32_ro_mount(fluxmax_fat32_ro_volume_t *volume, uint32_t device_index);
/* Read-only root-directory APIs; only classic 8.3 names are supported, no LFN/subdirectories. */
int fluxmax_fat32_ro_list_root(const fluxmax_fat32_ro_volume_t *volume,
                              fluxmax_fat32_ro_visit_fn visit, void *context);
int fluxmax_fat32_ro_stream_root_file(const fluxmax_fat32_ro_volume_t *volume,
                                     const char *short_name,
                                     fluxmax_fat32_ro_sink_fn sink, void *context);
#endif
