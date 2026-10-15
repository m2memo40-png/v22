#ifndef FLUXMAX_FS_FAT32_H
#define FLUXMAX_FS_FAT32_H
#include <stdint.h>
typedef struct {
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t fat_count;
    uint32_t sectors_per_fat;
    uint32_t total_sectors;
    uint32_t root_cluster;
    uint32_t first_data_sector;
} fluxmax_fat32_geometry_t;
int fluxmax_fat32_parse_boot_sector(const uint8_t sector[512], fluxmax_fat32_geometry_t *out);
uint64_t fluxmax_fat32_cluster_lba(const fluxmax_fat32_geometry_t *g, uint32_t cluster);
int fluxmax_fat32_cluster_valid(const fluxmax_fat32_geometry_t *g, uint32_t cluster);
#endif
