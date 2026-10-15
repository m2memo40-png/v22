#include "fs/fat32.h"

static uint16_t read_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int fluxmax_fat32_parse_boot_sector(const uint8_t sector[512], fluxmax_fat32_geometry_t *out) {
    if (!sector || !out || sector[510] != 0x55 || sector[511] != 0xAA) return -1;
    fluxmax_fat32_geometry_t g;
    g.bytes_per_sector = read_le16(sector + 11);
    g.sectors_per_cluster = sector[13];
    g.reserved_sector_count = read_le16(sector + 14);
    g.fat_count = sector[16];
    g.sectors_per_fat = read_le32(sector + 36);
    g.total_sectors = read_le16(sector + 19);
    if (!g.total_sectors) g.total_sectors = read_le32(sector + 32);
    g.root_cluster = read_le32(sector + 44);
    if (g.bytes_per_sector != 512 || !g.sectors_per_cluster ||
        (g.sectors_per_cluster & (g.sectors_per_cluster - 1u)) ||
        !g.reserved_sector_count || !g.fat_count || !g.sectors_per_fat ||
        g.root_cluster < 2 || !g.total_sectors) return -1;
    uint64_t first = (uint64_t)g.reserved_sector_count +
                     (uint64_t)g.fat_count * g.sectors_per_fat;
    if (first >= g.total_sectors || first > UINT32_MAX) return -1;
    g.first_data_sector = (uint32_t)first;
    /* Refuse BPBs whose root cluster or FAT capacity points outside the volume. */
    if (!fluxmax_fat32_cluster_valid(&g, g.root_cluster)) return -1;
    uint64_t data_sectors = (uint64_t)g.total_sectors - g.first_data_sector;
    uint64_t cluster_count = data_sectors / g.sectors_per_cluster;
    uint64_t fat_entries = ((uint64_t)g.sectors_per_fat * g.bytes_per_sector) / 4u;
    if (fat_entries < cluster_count + 2u) return -1;
    *out = g;
    return 0;
}

int fluxmax_fat32_cluster_valid(const fluxmax_fat32_geometry_t *g, uint32_t cluster) {
    if (!g || cluster < 2) return 0;
    uint64_t data_sectors = g->total_sectors - g->first_data_sector;
    uint64_t cluster_count = data_sectors / g->sectors_per_cluster;
    return (uint64_t)cluster < cluster_count + 2u;
}

uint64_t fluxmax_fat32_cluster_lba(const fluxmax_fat32_geometry_t *g, uint32_t cluster) {
    if (!g || !fluxmax_fat32_cluster_valid(g, cluster)) return UINT64_MAX;
    return (uint64_t)g->first_data_sector +
           (uint64_t)(cluster - 2u) * g->sectors_per_cluster;
}
