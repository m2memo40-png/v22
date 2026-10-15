#include "mm/vmm.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "kernel/bootinfo.h"
#include "lib/mem.h"
#include <stddef.h>

#define PT_PRESENT   (1ULL << 0)
#define PT_WRITE     (1ULL << 1)
#define PT_USER      (1ULL << 2)
#define PT_PWT       (1ULL << 3)
#define PT_PCD       (1ULL << 4)
#define PT_ACCESSED  (1ULL << 5)
#define PT_DIRTY     (1ULL << 6)
#define PT_LARGE     (1ULL << 7)
#define PT_GLOBAL    (1ULL << 8)
#define PT_ADDR      0x000FFFFFFFFFF000ULL
#define PT_ADDR_2M   0x000FFFFFFFE00000ULL
#define PT_ADDR_1G   0x000FFFFFC0000000ULL
#define PT_NX        (1ULL << 63)
#define PT_PAT_4K    (1ULL << 7)
#define PT_PAT_LARGE (1ULL << 12)
#define VMM_SPACE_TABLE_LIMIT 1024u
#define USER_TOP     0x0000800000000000ULL

struct vmm_address_space {
    uint64_t root_phys;
    uint64_t *root;
    uint32_t private_count;
    uint32_t reserved;
    uint64_t private_phys[VMM_SPACE_TABLE_LIMIT];
};

static uint64_t *root_table;
static uint64_t hhdm_offset;
static int initialized;
static int nx_enabled;

static int canonical(uint64_t va) {
    uint64_t high = va >> 48;
    return ((va & (1ULL << 47)) != 0) ? high == 0xFFFFu : high == 0;
}
static int is_user_va(uint64_t va) { return canonical(va) && va >= 0x1000ULL && va < USER_TOP; }
static uint64_t *table_from_phys(uint64_t physical) {
    return (uint64_t *)(uintptr_t)(physical + hhdm_offset);
}
static uint64_t phys_from_table(const void *table) {
    return (uint64_t)(uintptr_t)table - hhdm_offset;
}
static uint64_t read_cr3(void) {
    uint64_t value; __asm__ volatile("mov %%cr3,%0" : "=r"(value)); return value & PT_ADDR;
}
static void write_cr3(uint64_t physical) {
    __asm__ volatile("mov %0,%%cr3" : : "r"(physical & PT_ADDR) : "memory");
}
static void invalidate_page(uint64_t va) {
    __asm__ volatile("invlpg (%0)" : : "r"((void *)(uintptr_t)va) : "memory");
}
static int canonical_page(uint64_t va) { return canonical(va) && !(va & 0xFFFu); }

static uint64_t *next_table(uint64_t *parent, uint16_t index, int create, uint64_t leaf_flags) {
    uint64_t entry = parent[index];
    if (!(entry & PT_PRESENT)) {
        if (!create) return 0;
        void *page = pmm_alloc_page();
        if (!page) return 0;
        memset(page, 0, 4096);
        uint64_t flags = PT_PRESENT | PT_WRITE;
        if (leaf_flags & VMM_USER) flags |= PT_USER;
        parent[index] = (phys_from_table(page) & PT_ADDR) | flags;
        return (uint64_t *)page;
    }
    if (leaf_flags & VMM_USER) parent[index] |= PT_USER;
    if (leaf_flags & VMM_WRITABLE) parent[index] |= PT_WRITE;
    return table_from_phys(entry & PT_ADDR);
}

int vmm_init(void) {
    uint64_t cr3 = read_cr3();
    hhdm_offset = boot_hhdm_offset();
    if (!hhdm_offset || !(cr3 & PT_ADDR)) return -1;
    root_table = table_from_phys(cr3);
    initialized = 1;

    /* Enable NX on CPUs advertising the architectural NX bit. */
    uint32_t eax = 0x80000000u, ebx, ecx, edx;
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(eax), "c"(0));
    if (eax >= 0x80000001u) {
        eax = 0x80000001u;
        __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(eax), "c"(0));
        if (edx & (1u << 20)) {
            uint32_t lo, hi;
            __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0xC0000080u));
            lo |= (1u << 11);
            __asm__ volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(0xC0000080u));
            nx_enabled = 1;
        }
    }
    return 0;
}

int vmm_map(uint64_t va, uint64_t pa, uint64_t flags) {
    if (!initialized || !canonical_page(va) || ((va | pa) & 0xFFFu)) return -1;
    if ((pa & ~PT_ADDR) != 0) return -1;
    uint16_t pml4_i = (uint16_t)((va >> 39) & 0x1FFu);
    uint16_t pdpt_i = (uint16_t)((va >> 30) & 0x1FFu);
    uint16_t pd_i   = (uint16_t)((va >> 21) & 0x1FFu);
    uint16_t pt_i   = (uint16_t)((va >> 12) & 0x1FFu);
    uint64_t *pdpt = next_table(root_table, pml4_i, 1, flags);
    if (!pdpt) return -2;
    if ((pdpt[pdpt_i] & (PT_PRESENT | PT_LARGE)) == (PT_PRESENT | PT_LARGE)) return -2;
    uint64_t *pd = next_table(pdpt, pdpt_i, 1, flags);
    if (!pd) return -2;
    if (pd[pd_i] & PT_LARGE) return -2;
    uint64_t *pt = next_table(pd, pd_i, 1, flags);
    if (!pt) return -2;
    if (pt[pt_i] & PT_PRESENT) return -3;
    uint64_t entry_flags = flags & ~(PT_ADDR | PT_LARGE);
    entry_flags |= VMM_PRESENT;
    if (!nx_enabled) entry_flags &= ~VMM_NO_EXECUTE;
    pt[pt_i] = (pa & PT_ADDR) | entry_flags;
    invalidate_page(va);
    return 0;
}

int vmm_unmap(uint64_t va) {
    if (!initialized || !canonical_page(va)) return -1;
    uint16_t a = (uint16_t)((va >> 39) & 0x1FFu);
    uint16_t b = (uint16_t)((va >> 30) & 0x1FFu);
    uint16_t c = (uint16_t)((va >> 21) & 0x1FFu);
    uint16_t d = (uint16_t)((va >> 12) & 0x1FFu);
    uint64_t e = root_table[a];
    if (!(e & PT_PRESENT)) return -2;
    uint64_t *pdpt = table_from_phys(e & PT_ADDR);
    e = pdpt[b];
    if (!(e & PT_PRESENT) || (e & PT_LARGE)) return -2;
    uint64_t *pd = table_from_phys(e & PT_ADDR);
    e = pd[c];
    if (!(e & PT_PRESENT) || (e & PT_LARGE)) return -2;
    uint64_t *pt = table_from_phys(e & PT_ADDR);
    if (!(pt[d] & PT_PRESENT)) return -2;
    pt[d] = 0;
    invalidate_page(va);
    return 0;
}

int vmm_translate(uint64_t va, uint64_t *pa_out) {
    if (!initialized || !pa_out || !canonical(va)) return -1;
    uint64_t e = root_table[(va >> 39) & 0x1FFu];
    if (!(e & PT_PRESENT)) return -2;
    uint64_t *pdpt = table_from_phys(e & PT_ADDR);
    e = pdpt[(va >> 30) & 0x1FFu];
    if (!(e & PT_PRESENT)) return -2;
    if (e & PT_LARGE) { *pa_out = (e & PT_ADDR_1G) | (va & 0x3FFFFFFFu); return 0; }
    uint64_t *pd = table_from_phys(e & PT_ADDR);
    e = pd[(va >> 21) & 0x1FFu];
    if (!(e & PT_PRESENT)) return -2;
    if (e & PT_LARGE) { *pa_out = (e & PT_ADDR_2M) | (va & 0x1FFFFFu); return 0; }
    uint64_t *pt = table_from_phys(e & PT_ADDR);
    e = pt[(va >> 12) & 0x1FFu];
    if (!(e & PT_PRESENT)) return -2;
    *pa_out = (e & PT_ADDR) | (va & 0xFFFu);
    return 0;
}

static int space_has_private(const vmm_address_space_t *space, uint64_t physical) {
    for (uint32_t i = 0; i < space->private_count; ++i)
        if (space->private_phys[i] == physical) return 1;
    return 0;
}
static int space_track_table(vmm_address_space_t *space, uint64_t physical) {
    if (space->private_count >= VMM_SPACE_TABLE_LIMIT) return -1;
    space->private_phys[space->private_count++] = physical;
    return 0;
}
static uint64_t *space_new_table(vmm_address_space_t *space) {
    if (space->private_count >= VMM_SPACE_TABLE_LIMIT) return 0;
    void *page = pmm_alloc_page();
    if (!page) return 0;
    memset(page, 0, 4096);
    if (space_track_table(space, phys_from_table(page)) != 0) {
        pmm_free_page(page); return 0;
    }
    return (uint64_t *)page;
}
static uint64_t split_attrs(uint64_t entry) {
    return entry & (PT_PRESENT | PT_WRITE | PT_USER | PT_PWT | PT_PCD |
                    PT_ACCESSED | PT_DIRTY | PT_GLOBAL | PT_NX);
}
static uint64_t *space_split_large(vmm_address_space_t *space, uint64_t *parent,
                                   uint16_t index, int parent_level) {
    uint64_t old = parent[index];
    uint64_t *child = space_new_table(space);
    if (!child) return 0;
    uint64_t flags = split_attrs(old);
    if (parent_level == 3) {
        uint64_t base = old & PT_ADDR_1G;
        uint64_t large_pat = old & PT_PAT_LARGE;
        for (uint64_t i = 0; i < 512; ++i)
            child[i] = ((base + i * 0x200000ULL) & PT_ADDR_2M) | flags | PT_LARGE | large_pat;
    } else if (parent_level == 2) {
        uint64_t base = old & PT_ADDR_2M;
        int pat = (old & PT_PAT_LARGE) != 0;
        for (uint64_t i = 0; i < 512; ++i) {
            uint64_t f = flags;
            if (pat) f |= PT_PAT_4K;
            child[i] = ((base + i * 0x1000ULL) & PT_ADDR) | f;
        }
    } else {
        pmm_free_page(child);
        if (space->private_count) --space->private_count;
        return 0;
    }
    uint64_t pointer_flags = PT_PRESENT | PT_WRITE;
    if (old & PT_USER) pointer_flags |= PT_USER;
    parent[index] = (phys_from_table(child) & PT_ADDR) | pointer_flags;
    return child;
}

/* Return a private child page table. Existing shared tables are copied before
 * mutation, and 1 GiB/2 MiB leaves are split to preserve the previous mapping. */
static uint64_t *space_private_child(vmm_address_space_t *space, uint64_t *parent,
                                     uint16_t index, int parent_level, uint64_t leaf_flags) {
    uint64_t e = parent[index];
    uint64_t *child;
    if (!(e & PT_PRESENT)) {
        child = space_new_table(space);
        if (!child) return 0;
        uint64_t f = PT_PRESENT | PT_WRITE;
        if (leaf_flags & VMM_USER) f |= PT_USER;
        parent[index] = (phys_from_table(child) & PT_ADDR) | f;
    } else if (e & PT_LARGE) {
        child = space_split_large(space, parent, index, parent_level);
        if (!child) return 0;
    } else {
        uint64_t p = e & PT_ADDR;
        child = table_from_phys(p);
        if (!space_has_private(space, p)) {
            uint64_t *copy = space_new_table(space);
            if (!copy) return 0;
            memcpy(copy, child, 4096);
            parent[index] = (phys_from_table(copy) & PT_ADDR) | (e & ~PT_ADDR);
            child = copy;
        }
    }
    if (leaf_flags & VMM_USER) parent[index] |= PT_USER;
    if (leaf_flags & VMM_WRITABLE) parent[index] |= PT_WRITE;
    return child;
}

vmm_address_space_t *vmm_space_create(void) {
    if (!initialized) return 0;
    vmm_address_space_t *space = (vmm_address_space_t *)kmalloc(sizeof(*space));
    if (!space) return 0;
    memset(space, 0, sizeof(*space));
    void *root = pmm_alloc_page();
    if (!root) { kfree(space); return 0; }
    memset(root, 0, 4096);
    memcpy(root, root_table, 4096);
    space->root = (uint64_t *)root;
    space->root_phys = phys_from_table(root);
    if (space_track_table(space, space->root_phys) != 0) {
        pmm_free_page(root); kfree(space); return 0;
    }
    return space;
}

void vmm_space_destroy(vmm_address_space_t *space) {
    if (!space) return;
    if (read_cr3() == space->root_phys) write_cr3(phys_from_table(root_table));
    for (uint32_t i = space->private_count; i > 0; --i) {
        uint64_t physical = space->private_phys[i - 1];
        pmm_free_page(table_from_phys(physical));
    }
    kfree(space);
}

int vmm_space_map(vmm_address_space_t *space, uint64_t va, uint64_t pa, uint64_t flags) {
    if (!space || !is_user_va(va) || ((va | pa) & 0xFFFu) || (pa & ~PT_ADDR)) return -1;
    uint16_t a = (uint16_t)((va >> 39) & 0x1FFu);
    uint16_t b = (uint16_t)((va >> 30) & 0x1FFu);
    uint16_t c = (uint16_t)((va >> 21) & 0x1FFu);
    uint16_t d = (uint16_t)((va >> 12) & 0x1FFu);
    uint64_t *pdpt = space_private_child(space, space->root, a, 4, flags);
    if (!pdpt) return -2;
    uint64_t *pd = space_private_child(space, pdpt, b, 3, flags);
    if (!pd) return -2;
    uint64_t *pt = space_private_child(space, pd, c, 2, flags);
    if (!pt) return -2;
    uint64_t old = pt[d];
    if ((old & PT_PRESENT) && (!(flags & VMM_USER) || (old & PT_USER))) return -3;
    uint64_t entry_flags = flags & ~(PT_ADDR | PT_LARGE);
    entry_flags |= PT_PRESENT;
    if (!nx_enabled) entry_flags &= ~VMM_NO_EXECUTE;
    pt[d] = (pa & PT_ADDR) | entry_flags;
    if (read_cr3() == space->root_phys) invalidate_page(va);
    return 0;
}

int vmm_space_unmap(vmm_address_space_t *space, uint64_t va) {
    if (!space || !is_user_va(va) || (va & 0xFFFu)) return -1;
    uint64_t *root = space->root;
    uint64_t e = root[(va >> 39) & 0x1FFu]; if (!(e & PT_PRESENT)) return -2;
    uint64_t *pdpt = table_from_phys(e & PT_ADDR);
    e = pdpt[(va >> 30) & 0x1FFu]; if (!(e & PT_PRESENT) || (e & PT_LARGE)) return -2;
    uint64_t *pd = table_from_phys(e & PT_ADDR);
    e = pd[(va >> 21) & 0x1FFu]; if (!(e & PT_PRESENT) || (e & PT_LARGE)) return -2;
    uint64_t *pt = table_from_phys(e & PT_ADDR);
    if (!(pt[(va >> 12) & 0x1FFu] & PT_PRESENT)) return -2;
    pt[(va >> 12) & 0x1FFu] = 0;
    if (read_cr3() == space->root_phys) invalidate_page(va);
    return 0;
}

int vmm_space_translate(vmm_address_space_t *space, uint64_t va,
                        uint64_t *pa_out, uint64_t *flags_out) {
    if (!space || !pa_out || !canonical(va)) return -1;
    uint64_t permissions = PT_PRESENT | PT_WRITE | PT_USER;
    uint64_t e = space->root[(va >> 39) & 0x1FFu]; if (!(e & PT_PRESENT)) return -2;
    permissions &= e;
    uint64_t *pdpt = table_from_phys(e & PT_ADDR);
    e = pdpt[(va >> 30) & 0x1FFu]; if (!(e & PT_PRESENT)) return -2;
    permissions &= e;
    if (e & PT_LARGE) {
        *pa_out = (e & PT_ADDR_1G) | (va & 0x3FFFFFFFULL);
        if (flags_out) *flags_out = (e & ~(PT_ADDR_1G | PT_LARGE)) | (permissions & (PT_WRITE | PT_USER));
        return 0;
    }
    uint64_t *pd = table_from_phys(e & PT_ADDR);
    e = pd[(va >> 21) & 0x1FFu]; if (!(e & PT_PRESENT)) return -2;
    permissions &= e;
    if (e & PT_LARGE) {
        *pa_out = (e & PT_ADDR_2M) | (va & 0x1FFFFFULL);
        if (flags_out) *flags_out = (e & ~(PT_ADDR_2M | PT_LARGE)) | (permissions & (PT_WRITE | PT_USER));
        return 0;
    }
    uint64_t *pt = table_from_phys(e & PT_ADDR);
    e = pt[(va >> 12) & 0x1FFu]; if (!(e & PT_PRESENT)) return -2;
    permissions &= e;
    *pa_out = (e & PT_ADDR) | (va & 0xFFFu);
    if (flags_out) {
        uint64_t f = e & ~PT_ADDR;
        f &= ~(PT_WRITE | PT_USER);
        f |= permissions & (PT_WRITE | PT_USER);
        if ((space->root[(va >> 39) & 0x1FFu] | 0) & PT_NX ||
            (pdpt[(va >> 30) & 0x1FFu] & PT_NX) ||
            (pd[(va >> 21) & 0x1FFu] & PT_NX) || (e & PT_NX)) f |= PT_NX;
        *flags_out = f;
    }
    return 0;
}

static int space_copy(vmm_address_space_t *space, uint64_t user_va, void *kernel_buf,
                      size_t length, int copy_to_user) {
    if (!space || (!kernel_buf && length)) return -1;
    if (length && (!is_user_va(user_va) || user_va > USER_TOP - length)) return -1;
    size_t done = 0;
    while (done < length) {
        uint64_t va = user_va + done, phys, flags;
        if (vmm_space_translate(space, va, &phys, &flags) != 0 || !(flags & PT_USER)) return -1;
        size_t chunk = 4096u - (size_t)(va & 0xFFFu);
        if (chunk > length - done) chunk = length - done;
        void *physical_alias = (void *)(uintptr_t)((phys & PT_ADDR) + hhdm_offset + (va & 0xFFFu));
        if (copy_to_user) memcpy(physical_alias, (const uint8_t *)kernel_buf + done, chunk);
        else memcpy((uint8_t *)kernel_buf + done, physical_alias, chunk);
        done += chunk;
    }
    return 0;
}
int vmm_space_copy_from(vmm_address_space_t *space, void *dst, uint64_t src, size_t n) {
    return space_copy(space, src, dst, n, 0);
}
int vmm_space_copy_to(vmm_address_space_t *space, uint64_t dst, const void *src, size_t n) {
    return space_copy(space, dst, (void *)src, n, 1);
}
uint64_t vmm_space_root_phys(const vmm_address_space_t *space) { return space ? space->root_phys : 0; }
void vmm_space_switch(vmm_address_space_t *space) { if (space && space->root_phys) write_cr3(space->root_phys); }
uint64_t vmm_current_root_phys(void) { return read_cr3(); }
