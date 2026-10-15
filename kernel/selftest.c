#include "kernel/selftest.h"
#include "kernel/log.h"
#include "kernel/feature_ledger.h"
#include "mm/heap.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "kernel/bootinfo.h"
#include "kernel/syscall.h"
#include "user/elf.h"
#include "user/builtin.h"
#include "fs/vfs.h"
#include "fs/fat32.h"
#include "net/ipv4.h"
#include "lib/mem.h"
#include <stddef.h>
#include <stdint.h>

static uint32_t checks_passed;
static uint32_t checks_failed;

static void check_result(const char *name, int ok) {
    serial_write(ok ? "[SELFTEST PASS] " : "[SELFTEST FAIL] ");
    serial_write(name);
    serial_write("\n");
    if (ok) ++checks_passed;
    else ++checks_failed;
}

static int bytes_equal(const uint8_t *a, const uint8_t *b, size_t n) {
    for (size_t i = 0; i < n; ++i) if (a[i] != b[i]) return 0;
    return 1;
}

static void put_le16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void test_heap(void) {
    size_t before = heap_bytes_used();
    uint8_t *a = (uint8_t *)kmalloc(37);
    uint8_t *b = (uint8_t *)kmalloc(513);
    int ok = a != 0 && b != 0;
    if (ok) {
        for (size_t i = 0; i < 37; ++i) a[i] = (uint8_t)(i ^ 0x5Au);
        for (size_t i = 0; i < 513; ++i) b[i] = (uint8_t)(i * 7u);
        for (size_t i = 0; i < 37; ++i) if (a[i] != (uint8_t)(i ^ 0x5Au)) ok = 0;
        for (size_t i = 0; i < 513; ++i) if (b[i] != (uint8_t)(i * 7u)) ok = 0;
    }
    if (a) kfree(a);
    if (b) kfree(b);
    if (heap_bytes_used() != before) ok = 0;
    check_result("heap allocate/write/read/free accounting", ok);
}

static void test_vfs(void) {
    static const char path[] = "/tmp/.fluxmax-selftest-file";
    static const char payload[] = "Fluxmax VFS self-test";
    static const char patch[] = "PASS";
    uint8_t actual[sizeof(payload) + 8u];
    int ok = 1;
    vfs_node_t *old = vfs_lookup(path);
    if (old) {
        if (old->type != VFS_NODE_FILE || vfs_unlink(path) != 0) ok = 0;
    }
    vfs_node_t *file = ok ? vfs_create(path, VFS_NODE_FILE) : 0;
    if (!file) ok = 0;
    if (ok && vfs_write(file, 0, payload, sizeof(payload) - 1u) != (int64_t)(sizeof(payload) - 1u)) ok = 0;
    if (ok && vfs_write(file, sizeof(payload) + 2u, patch, sizeof(patch) - 1u) != (int64_t)(sizeof(patch) - 1u)) ok = 0;
    memset(actual, 0xFF, sizeof(actual));
    if (ok && vfs_read(file, 0, actual, sizeof(actual)) != (int64_t)(sizeof(payload) + 2u + sizeof(patch) - 1u)) ok = 0;
    if (ok && !bytes_equal(actual, (const uint8_t *)payload, sizeof(payload) - 1u)) ok = 0;
    if (ok && (actual[sizeof(payload) - 1u] != 0 || actual[sizeof(payload)] != 0)) ok = 0;
    if (ok && !bytes_equal(actual + sizeof(payload) + 2u, (const uint8_t *)patch, sizeof(patch) - 1u)) ok = 0;
    if (file && vfs_unlink(path) != 0) ok = 0;
    if (vfs_lookup(path) != 0) ok = 0;
    check_result("RAM VFS create/read/write/sparse-write/unlink", ok);

    check_result("VFS path normalization for dot and parent", 
                 vfs_lookup("/tmp/./../etc") == vfs_lookup("/etc"));
}

static void test_ipv4(void) {
    fluxmax_ipv4_addr_t a;
    fluxmax_ipv4_addr_t b;
    int parsed = fluxmax_ipv4_parse("192.168.1.200", &a) == 0;
    check_result("IPv4 strict dotted-decimal parser", parsed &&
                  a.octet[0] == 192 && a.octet[1] == 168 &&
                  a.octet[2] == 1 && a.octet[3] == 200 &&
                  fluxmax_ipv4_to_u32(a) == 0xC0A801C8u);
    b = fluxmax_ipv4_from_u32(0xC0A801C8u);
    check_result("IPv4 integer/address round trip", b.octet[0] == 192 &&
                  b.octet[1] == 168 && b.octet[2] == 1 && b.octet[3] == 200);
    check_result("IPv4 prefix match boundaries", 
                  fluxmax_ipv4_prefix_match(a, b, 32) &&
                  fluxmax_ipv4_prefix_match(a, fluxmax_ipv4_from_u32(0xC0A80102u), 24) &&
                  !fluxmax_ipv4_prefix_match(a, fluxmax_ipv4_from_u32(0xC0A80201u), 24) &&
                  fluxmax_ipv4_prefix_match(a, b, 0) &&
                  !fluxmax_ipv4_prefix_match(a, b, 33));
    check_result("IPv4 parser rejects malformed address", 
                  fluxmax_ipv4_parse("192.168.1.256", &b) != 0 &&
                  fluxmax_ipv4_parse("192.168.1", &b) != 0 &&
                  fluxmax_ipv4_parse("1..2.3", &b) != 0 &&
                  fluxmax_ipv4_parse("1.2.3.4x", &b) != 0);
    uint8_t header[20] = {0x45,0x00,0x00,0x73,0x00,0x00,0x40,0x00,0x40,0x11,
                          0x00,0x00,0xC0,0xA8,0x00,0x01,0xC0,0xA8,0x00,0xC7};
    uint16_t sum = fluxmax_ipv4_checksum(header, sizeof(header));
    header[10] = (uint8_t)(sum >> 8);
    header[11] = (uint8_t)sum;
    check_result("IPv4 header checksum known vector", sum == 0xB861u &&
                  fluxmax_ipv4_checksum(header, sizeof(header)) == 0);
}

static void test_fat32_geometry(void) {
    uint8_t sector[512];
    fluxmax_fat32_geometry_t g;
    memset(sector, 0, sizeof(sector));
    put_le16(sector + 11, 512);
    sector[13] = 1;
    put_le16(sector + 14, 32);
    sector[16] = 2;
    put_le16(sector + 19, 0);
    put_le32(sector + 32, 70000);
    put_le32(sector + 36, 600);
    put_le32(sector + 44, 2);
    sector[510] = 0x55;
    sector[511] = 0xAA;
    int ok = fluxmax_fat32_parse_boot_sector(sector, &g) == 0 &&
             g.bytes_per_sector == 512 && g.first_data_sector == 1232 &&
             fluxmax_fat32_cluster_valid(&g, 2) &&
             fluxmax_fat32_cluster_valid(&g, 68769) &&
             !fluxmax_fat32_cluster_valid(&g, 68770) &&
             fluxmax_fat32_cluster_lba(&g, 2) == 1232 &&
             fluxmax_fat32_cluster_lba(&g, 68769) == 69999 &&
             fluxmax_fat32_cluster_lba(&g, 68770) == UINT64_MAX;
    check_result("FAT32 BPB geometry and cluster bounds", ok);

    put_le32(sector + 44, 70000);
    check_result("FAT32 parser rejects out-of-range root cluster", 
                 fluxmax_fat32_parse_boot_sector(sector, &g) != 0);
    put_le32(sector + 44, 2);
    put_le16(sector + 11, 4096);
    check_result("FAT32 parser rejects unsupported sector size", 
                 fluxmax_fat32_parse_boot_sector(sector, &g) != 0);
}


static void test_user_address_space(void) {
    uint64_t before = pmm_free_pages();
    vmm_address_space_t *space = vmm_space_create();
    void *page = pmm_alloc_page();
    int ok = space != 0 && page != 0 && boot_hhdm_offset() != 0;
    uint64_t physical = 0, translated = 0, flags = 0;
    static const char expected[] = "USER-SPACE-COPY-OK";
    char actual[sizeof(expected)];
    if (ok) {
        uint64_t ptr = (uint64_t)(uintptr_t)page;
        uint64_t hhdm = boot_hhdm_offset();
        if (ptr < hhdm) ok = 0;
        else physical = ptr - hhdm;
    }
    if (ok && vmm_space_map(space, 0x0000000000500000ULL, physical,
                            VMM_PRESENT | VMM_USER | VMM_WRITABLE | VMM_NO_EXECUTE) != 0) ok = 0;
    if (ok && vmm_space_copy_to(space, 0x0000000000500003ULL, expected, sizeof(expected)) != 0) ok = 0;
    memset(actual, 0, sizeof(actual));
    if (ok && vmm_space_copy_from(space, actual, 0x0000000000500003ULL, sizeof(actual)) != 0) ok = 0;
    if (ok && !bytes_equal((const uint8_t *)actual, (const uint8_t *)expected, sizeof(expected))) ok = 0;
    if (ok && vmm_space_translate(space, 0x0000000000500007ULL, &translated, &flags) != 0) ok = 0;
    if (ok && (translated != physical + 7u || !(flags & VMM_USER) ||
               !(flags & VMM_WRITABLE) || !(flags & VMM_NO_EXECUTE))) ok = 0;
    if (space) vmm_space_destroy(space);
    if (page) pmm_free_page(page);
    if (pmm_free_pages() != before) ok = 0;
    check_result("isolated user address space map/copy/permissions/reclaim", ok);
}

static void test_builtin_elf_loader(void) {
    fluxmax_user_image_t image;
    memset(&image, 0, sizeof(image));
    size_t size = fluxmax_builtin_hello_elf_size();
    int ok = size >= 64u && fluxmax_elf_load(fluxmax_builtin_hello_elf_start, size, &image) == 0;
    uint64_t physical = 0, flags = 0;
    if (ok && (image.entry == 0 || image.stack_pointer == 0 || !image.space)) ok = 0;
    if (ok && (vmm_space_translate(image.space, image.entry, &physical, &flags) != 0 ||
               !(flags & VMM_USER) || (flags & VMM_NO_EXECUTE))) ok = 0;
    if (ok && (vmm_space_translate(image.space, image.stack_pointer - 1u, &physical, &flags) != 0 ||
               !(flags & VMM_USER) || !(flags & VMM_WRITABLE) || !(flags & VMM_NO_EXECUTE))) ok = 0;
    if (image.space) fluxmax_user_image_destroy(&image);
    check_result("embedded static ELF loader maps executable entry and user stack", ok);
}

static void test_syscall_abi(void) {
    int ok = fluxmax_syscall_init() == 0;
    const fluxmax_syscall_entry_t *exit_call = fluxmax_syscall_lookup(FLUXMAX_SYS_EXIT);
    const fluxmax_syscall_entry_t *write_call = fluxmax_syscall_lookup(FLUXMAX_SYS_WRITE);
    const fluxmax_syscall_entry_t *pid_call = fluxmax_syscall_lookup(FLUXMAX_SYS_GETPID);
    const fluxmax_syscall_entry_t *yield_call = fluxmax_syscall_lookup(FLUXMAX_SYS_YIELD);
    const fluxmax_syscall_entry_t *uptime_call = fluxmax_syscall_lookup(FLUXMAX_SYS_UPTIME);
    if (!exit_call || !write_call || !pid_call || !yield_call || !uptime_call) ok = 0;
    if (ok && (exit_call->argument_count != 1 || write_call->argument_count != 3 ||
               pid_call->argument_count != 0 || yield_call->argument_count != 0 ||
               uptime_call->argument_count != 0)) ok = 0;
    check_result("Ring-3 syscall ABI registration and argument metadata", ok);
}

uint32_t fluxmax_selftest_run(void) {
    checks_passed = 0;
    checks_failed = 0;
    serial_write("\nFluxmax kernel self-test starting. Tests use temporary RAM data and do not write disk.\n");
    test_heap();
    test_vfs();
    test_ipv4();
    test_fat32_geometry();
    test_user_address_space();
    test_builtin_elf_loader();
    test_syscall_abi();
    serial_write("SELFTEST SUMMARY: ");
    /* Keep formatting independent from libc/printf in the freestanding kernel. */
    char digits[11];
    uint32_t n = checks_passed;
    size_t used = 0;
    do { digits[used++] = (char)('0' + n % 10u); n /= 10u; } while (n && used < sizeof(digits));
    while (used) serial_write_char(digits[--used]);
    serial_write(" passed, ");
    n = checks_failed;
    used = 0;
    do { digits[used++] = (char)('0' + n % 10u); n /= 10u; } while (n && used < sizeof(digits));
    while (used) serial_write_char(digits[--used]);
    serial_write(" failed.\n");
    return checks_failed;
}
