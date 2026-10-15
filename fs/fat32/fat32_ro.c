#include "fs/fat32_ro.h"
#include "drivers/storage/ata_pio.h"
#include "lib/mem.h"

#define FAT32_MBR_OFFSET 446u
#define FAT32_MBR_ENTRY_SIZE 16u
#define FAT32_ENTRY_SIZE 32u
#define FAT32_ATTR_DIRECTORY 0x10u
#define FAT32_ATTR_VOLUME_ID 0x08u
#define FAT32_ATTR_LONG_NAME 0x0Fu
#define FAT32_EOC_MIN 0x0FFFFFF8u
#define FAT32_BAD_CLUSTER 0x0FFFFFF7u
#define FAT32_CLUSTER_MASK 0x0FFFFFFFu

typedef struct fat32_dirent {
    char name[13];
    uint8_t attributes;
    uint32_t first_cluster;
    uint32_t size;
} fat32_dirent_t;

static uint16_t get_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static uint32_t get_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static int read_abs(uint32_t device_index, uint64_t lba, uint8_t out[512]) {
    const fluxmax_ata_device_t *device = fluxmax_ata_get_device(device_index);
    if (!device || lba >= device->sector_count) return -1;
    return fluxmax_ata_read_sector(device_index, lba, out);
}
static int read_relative(const fluxmax_fat32_ro_volume_t *v, uint64_t relative, uint8_t out[512]) {
    const fluxmax_ata_device_t *d;
    if (!v || relative >= v->geometry.total_sectors) return -1;
    d = fluxmax_ata_get_device(v->device_index);
    if (!d || v->partition_lba >= d->sector_count || relative > d->sector_count - v->partition_lba - 1u) return -1;
    return read_abs(v->device_index, v->partition_lba + relative, out);
}
static int parse_at_partition(fluxmax_fat32_ro_volume_t *v, uint32_t device_index,
                              uint64_t start_lba, uint64_t partition_length) {
    uint8_t sector[512];
    fluxmax_fat32_geometry_t geometry;
    const fluxmax_ata_device_t *d = fluxmax_ata_get_device(device_index);
    if (!d || start_lba >= d->sector_count || read_abs(device_index, start_lba, sector) != 0) return -1;
    if (fluxmax_fat32_parse_boot_sector(sector, &geometry) != 0) return -1;
    if (geometry.total_sectors > d->sector_count - start_lba) return -1;
    if (partition_length && geometry.total_sectors > partition_length) return -1;
    if (!fluxmax_fat32_cluster_valid(&geometry, geometry.root_cluster)) return -1;
    v->device_index = device_index;
    v->partition_lba = start_lba;
    v->geometry = geometry;
    return 0;
}

int fluxmax_fat32_ro_mount(fluxmax_fat32_ro_volume_t *volume, uint32_t device_index) {
    uint8_t mbr[512];
    const fluxmax_ata_device_t *d = fluxmax_ata_get_device(device_index);
    if (!volume || !d || read_abs(device_index, 0, mbr) != 0) return -1;
    if (mbr[510] == 0x55u && mbr[511] == 0xAAu) {
        for (uint32_t i = 0; i < 4u; ++i) {
            const uint8_t *entry = mbr + FAT32_MBR_OFFSET + i * FAT32_MBR_ENTRY_SIZE;
            uint8_t type = entry[4];
            uint32_t start = get_le32(entry + 8);
            uint32_t length = get_le32(entry + 12);
            if (!start || !length) continue;
            if (type != 0x0Bu && type != 0x0Cu && type != 0x1Bu && type != 0x1Cu) continue;
            if (parse_at_partition(volume, device_index, start, length) == 0) return 0;
        }
    }
    /* Also support a FAT32 superfloppy without an MBR partition table. */
    return parse_at_partition(volume, device_index, 0, d->sector_count);
}

static uint64_t cluster_lba(const fluxmax_fat32_ro_volume_t *v, uint32_t cluster) {
    uint64_t relative = fluxmax_fat32_cluster_lba(&v->geometry, cluster);
    if (relative == UINT64_MAX) return UINT64_MAX;
    return relative;
}
static int next_cluster(const fluxmax_fat32_ro_volume_t *v, uint32_t cluster, uint32_t *next) {
    uint8_t sector[512];
    uint64_t byte_offset = (uint64_t)cluster * 4u;
    uint64_t fat_sector = (uint64_t)v->geometry.reserved_sector_count + byte_offset / 512u;
    uint32_t offset = (uint32_t)(byte_offset % 512u);
    if (!next || fat_sector >= (uint64_t)v->geometry.reserved_sector_count + v->geometry.sectors_per_fat) return -1;
    if (read_relative(v, fat_sector, sector) != 0) return -1;
    *next = get_le32(sector + offset) & FAT32_CLUSTER_MASK;
    if (*next == FAT32_BAD_CLUSTER || *next < 2u) return -1;
    return 0;
}
static void format_short_name(const uint8_t entry[FAT32_ENTRY_SIZE], char out[13]) {
    size_t n = 0;
    for (size_t i = 0; i < 8u && entry[i] != ' '; ++i) {
        uint8_t c = entry[i] == 0x05u ? 0xE5u : entry[i];
        out[n++] = (c >= 32u && c < 127u) ? (char)c : '?';
    }
    for (size_t i = 8u; i < 11u && entry[i] != ' '; ++i) {
        uint8_t c = entry[i];
        if (i == 8u && c) out[n++] = '.';
        out[n++] = (c >= 32u && c < 127u) ? (char)c : '?';
    }
    out[n] = '\0';
}
static int decode_entry(const uint8_t entry[FAT32_ENTRY_SIZE], fat32_dirent_t *out) {
    if (!entry || !out || entry[0] == 0u || entry[0] == 0xE5u) return 0;
    if (entry[11] == FAT32_ATTR_LONG_NAME || (entry[11] & FAT32_ATTR_VOLUME_ID)) return 0;
    format_short_name(entry, out->name);
    out->attributes = entry[11];
    out->first_cluster = ((uint32_t)get_le16(entry + 20) << 16) | get_le16(entry + 26);
    out->size = get_le32(entry + 28);
    return 1;
}
static int scan_root(const fluxmax_fat32_ro_volume_t *v,
                     int (*visit_entry)(const fat32_dirent_t *, void *), void *context) {
    uint8_t sector[512];
    uint32_t cluster;
    uint64_t max_clusters;
    if (!v || !visit_entry || !fluxmax_fat32_cluster_valid(&v->geometry, v->geometry.root_cluster)) return -1;
    cluster = v->geometry.root_cluster;
    max_clusters = v->geometry.total_sectors / v->geometry.sectors_per_cluster + 1u;
    for (uint64_t chain = 0; chain < max_clusters; ++chain) {
        uint64_t first = cluster_lba(v, cluster);
        if (first == UINT64_MAX) return -1;
        for (uint32_t s = 0; s < v->geometry.sectors_per_cluster; ++s) {
            if (read_relative(v, first + s, sector) != 0) return -1;
            for (uint32_t off = 0; off < 512u; off += FAT32_ENTRY_SIZE) {
                const uint8_t *raw = sector + off;
                fat32_dirent_t decoded;
                if (raw[0] == 0u) return 0;
                if (!decode_entry(raw, &decoded)) continue;
                int rc = visit_entry(&decoded, context);
                if (rc) return rc;
            }
        }
        uint32_t next;
        if (next_cluster(v, cluster, &next) != 0) return -1;
        if (next >= FAT32_EOC_MIN) return 0;
        if (!fluxmax_fat32_cluster_valid(&v->geometry, next)) return -1;
        cluster = next;
    }
    return -1; /* The chain is cyclic or implausibly long. */
}
typedef struct list_context { fluxmax_fat32_ro_visit_fn visit; void *context; } list_context_t;
static int visit_list_entry(const fat32_dirent_t *entry, void *opaque) {
    list_context_t *ctx = (list_context_t *)opaque;
    return ctx->visit(entry->name, entry->attributes, entry->size, ctx->context);
}
int fluxmax_fat32_ro_list_root(const fluxmax_fat32_ro_volume_t *v,
                              fluxmax_fat32_ro_visit_fn visit, void *context) {
    list_context_t ctx;
    if (!visit) return -1;
    ctx.visit = visit; ctx.context = context;
    return scan_root(v, visit_list_entry, &ctx);
}

static int equal_ascii_fold(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'a' && ca <= 'z') ca = (char)(ca - 'a' + 'A');
        if (cb >= 'a' && cb <= 'z') cb = (char)(cb - 'a' + 'A');
        if (ca != cb) return 0;
        ++a; ++b;
    }
    return *a == '\0' && *b == '\0';
}
typedef struct find_context { const char *name; fat32_dirent_t found; int matched; } find_context_t;
static int visit_find_entry(const fat32_dirent_t *entry, void *opaque) {
    find_context_t *ctx = (find_context_t *)opaque;
    if (equal_ascii_fold(entry->name, ctx->name)) { ctx->found = *entry; ctx->matched = 1; return 1; }
    return 0;
}
int fluxmax_fat32_ro_stream_root_file(const fluxmax_fat32_ro_volume_t *v,
                                     const char *name, fluxmax_fat32_ro_sink_fn sink, void *context) {
    find_context_t ctx;
    uint8_t sector[512];
    uint32_t cluster;
    uint64_t remaining;
    if (!v || !name || !sink || !*name) return -1;
    for (const char *p = name; *p; ++p) if (*p == '/' || *p == '\\') return -1;
    memset(&ctx, 0, sizeof(ctx)); ctx.name = name;
    if (scan_root(v, visit_find_entry, &ctx) < 0 || !ctx.matched) return -1;
    if (ctx.found.attributes & FAT32_ATTR_DIRECTORY) return -1;
    remaining = ctx.found.size;
    if (!remaining) return 0;
    cluster = ctx.found.first_cluster;
    uint64_t max_clusters = v->geometry.total_sectors / v->geometry.sectors_per_cluster + 1u;
    if (!fluxmax_fat32_cluster_valid(&v->geometry, cluster)) return -1;
    for (uint64_t chain = 0; chain < max_clusters && remaining; ++chain) {
        uint64_t first = cluster_lba(v, cluster);
        if (first == UINT64_MAX) return -1;
        for (uint32_t s = 0; s < v->geometry.sectors_per_cluster && remaining; ++s) {
            if (read_relative(v, first + s, sector) != 0) return -1;
            size_t bytes = remaining > 512u ? 512u : (size_t)remaining;
            if (sink(sector, bytes, context) != 0) return -1;
            remaining -= bytes;
        }
        if (!remaining) return 0;
        uint32_t next;
        if (next_cluster(v, cluster, &next) != 0 || next >= FAT32_EOC_MIN ||
            !fluxmax_fat32_cluster_valid(&v->geometry, next)) return -1;
        cluster = next;
    }
    return -1;
}
