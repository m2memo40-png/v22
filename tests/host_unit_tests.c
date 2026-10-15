#include "net/ipv4.h"
#include "fs/fat32.h"
#include "fs/vfs.h"
#include "mm/heap.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned passed;
static unsigned failed;

/* Host-only allocator adapters let tests exercise the real VFS source safely. */
void *kmalloc(size_t size) { return size ? malloc(size) : NULL; }
void kfree(void *ptr) { free(ptr); }

static void check(const char *name, int ok) {
    printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
    if (ok) ++passed; else ++failed;
}
static void put16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void put32(uint8_t *p, uint32_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24); }
static int count_child(const vfs_node_t *node, void *context) {
    (void)node;
    ++*(unsigned *)context;
    return 0;
}

static void test_vfs(void) {
    static const char base[] = "alpha";
    static const char tail[] = "end";
    const char *path = "/home/test/note.txt";
    uint8_t contents[16];
    vfs_node_t *file;
    unsigned children = 0;
    check("VFS initialization and root lookup", vfs_init() == 0 && vfs_root() == vfs_lookup("/"));
    check("VFS directory and nested file creation", vfs_mkdir("/home") == 0 && vfs_mkdir("/home/test") == 0 && (file = vfs_create(path, VFS_NODE_FILE)) != NULL);
    file = vfs_lookup(path);
    check("VFS duplicate and relative path rejection", vfs_create(path, VFS_NODE_FILE) == NULL && vfs_lookup("home/test/note.txt") == NULL);
    check("VFS write and sparse extension", file && vfs_write(file, 0, base, sizeof(base)-1u) == (int64_t)(sizeof(base)-1u) && vfs_write(file, 8, tail, sizeof(tail)-1u) == (int64_t)(sizeof(tail)-1u) && file->size == 11);
    memset(contents, 0xFF, sizeof(contents));
    check("VFS read preserves bytes and zero-fills holes", file && vfs_read(file, 0, contents, sizeof(contents)) == 11 && memcmp(contents, base, sizeof(base)-1u) == 0 && contents[5] == 0 && contents[6] == 0 && contents[7] == 0 && memcmp(contents+8, tail, sizeof(tail)-1u) == 0);
    check("VFS dot/parent normalization and list", vfs_lookup("/home/test/../test/note.txt") == file && vfs_list(vfs_root(), count_child, &children) == 0 && children == 1);
    check("VFS refuses growth beyond per-file safety limit", file && vfs_write(file, 1024u*1024u, "x", 1) < 0);
    check("VFS refuses non-empty directory removal", vfs_unlink("/home") != 0 && vfs_unlink(path) == 0 && vfs_unlink("/home/test") == 0 && vfs_unlink("/home") == 0 && vfs_lookup("/home") == NULL);
}

int main(void) {
    fluxmax_ipv4_addr_t a = {{0}}, b = {{0}};
    check("IPv4 valid address parsing", fluxmax_ipv4_parse("192.168.1.200", &a) == 0 && fluxmax_ipv4_to_u32(a) == 0xC0A801C8u);
    check("IPv4 rejects malformed inputs", fluxmax_ipv4_parse("192.168.1.256", &b) != 0 && fluxmax_ipv4_parse("1.2.3", &b) != 0 && fluxmax_ipv4_parse("1..3.4", &b) != 0 && fluxmax_ipv4_parse("1.2.3.4junk", &b) != 0 && fluxmax_ipv4_parse("", &b) != 0);
    check("IPv4 to/from integer round trip", fluxmax_ipv4_to_u32(fluxmax_ipv4_from_u32(0x7F000001u)) == 0x7F000001u);
    check("IPv4 prefixes 0/24/32 and invalid 33", fluxmax_ipv4_prefix_match(a, fluxmax_ipv4_from_u32(0xC0A80102u), 24) && fluxmax_ipv4_prefix_match(a,b,0) && fluxmax_ipv4_prefix_match(a,a,32) && !fluxmax_ipv4_prefix_match(a,a,33));
    uint8_t hdr[20] = {0x45,0x00,0x00,0x73,0x00,0x00,0x40,0x00,0x40,0x11,0x00,0x00,0xC0,0xA8,0x00,0x01,0xC0,0xA8,0x00,0xC7};
    uint16_t sum = fluxmax_ipv4_checksum(hdr, sizeof(hdr)); hdr[10]=(uint8_t)(sum>>8); hdr[11]=(uint8_t)sum;
    check("IPv4 checksum RFC-style known vector", sum == 0xB861u && fluxmax_ipv4_checksum(hdr, sizeof(hdr)) == 0);

    uint8_t boot[512]; fluxmax_fat32_geometry_t g;
    memset(boot, 0, sizeof(boot)); put16(boot+11,512); boot[13]=1; put16(boot+14,32); boot[16]=2;
    put32(boot+32,70000); put32(boot+36,600); put32(boot+44,2); boot[510]=0x55; boot[511]=0xAA;
    check("FAT32 valid BPB and computed data offset", fluxmax_fat32_parse_boot_sector(boot,&g)==0 && g.first_data_sector==1232 && g.total_sectors==70000);
    check("FAT32 valid boundary clusters", fluxmax_fat32_cluster_valid(&g,2) && fluxmax_fat32_cluster_valid(&g,68769) && !fluxmax_fat32_cluster_valid(&g,68770) && fluxmax_fat32_cluster_lba(&g,68769)==69999 && fluxmax_fat32_cluster_lba(&g,68770)==UINT64_MAX);
    put32(boot+44,70000);
    check("FAT32 rejects root cluster outside volume", fluxmax_fat32_parse_boot_sector(boot,&g)!=0);
    put32(boot+44,2); put16(boot+11,4096);
    check("FAT32 rejects unsupported logical sector size", fluxmax_fat32_parse_boot_sector(boot,&g)!=0);
    put16(boot+11,512); put32(boot+36,1);
    check("FAT32 rejects FAT too small for volume", fluxmax_fat32_parse_boot_sector(boot,&g)!=0);
    boot[511]=0;
    check("FAT32 rejects missing boot signature", fluxmax_fat32_parse_boot_sector(boot,&g)!=0);

    test_vfs();
    printf("\nHost unit tests: %u passed, %u failed.\n", passed, failed);
    return failed ? 1 : 0;
}
