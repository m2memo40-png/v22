#include "user/elf.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "lib/mem.h"
#include "kernel/bootinfo.h"
#include <stddef.h>

#define ELF_PT_LOAD 1u
#define ELF_ET_EXEC 2u
#define ELF_EM_X86_64 62u
#define ELF_PF_X 1u
#define ELF_PF_W 2u
#define ELF_PF_R 4u
#define ELF_PAGE 4096ULL
#define USER_STACK_PAGES 8u
#define USER_STACK_TOP 0x00007FFF00010000ULL
#define USER_STACK_BASE (USER_STACK_TOP - (USER_STACK_PAGES * ELF_PAGE))
#define USER_IMAGE_MAX_PAGES 8192u
#define ELF_IDENT_SIZE 16u
#define ELF_CLASS_64 2u
#define ELF_DATA_LSB 1u
#define ELF_VERSION_CURRENT 1u

typedef struct __attribute__((packed)) {
    unsigned char ident[ELF_IDENT_SIZE];
    uint16_t type, machine;
    uint32_t version;
    uint64_t entry, phoff, shoff;
    uint32_t flags;
    uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} elf64_ehdr_t;

typedef struct __attribute__((packed)) {
    uint32_t type, flags;
    uint64_t offset, vaddr, paddr, filesz, memsz, align;
} elf64_phdr_t;

static int bounds_ok(size_t total, uint64_t offset, uint64_t length) {
    return offset <= (uint64_t)total && length <= (uint64_t)total - offset;
}
static int valid_user_range(uint64_t start, uint64_t length) {
    const uint64_t user_top = 0x0000800000000000ULL;
    if (!length || start < 0x1000ULL || start >= user_top || length > user_top - start) return 0;
    if (start + length > USER_STACK_BASE) return 0;
    return 1;
}
static int image_record_page(fluxmax_user_image_t *image, uint64_t va, uint64_t pa) {
    if (image->page_count >= USER_IMAGE_MAX_PAGES) return -1;
    if (image->page_count == image->page_capacity) {
        size_t new_capacity = image->page_capacity ? image->page_capacity * 2u : 32u;
        if (new_capacity > USER_IMAGE_MAX_PAGES) new_capacity = USER_IMAGE_MAX_PAGES;
        uint64_t *new_va = (uint64_t *)kmalloc(new_capacity * sizeof(uint64_t));
        uint64_t *new_pa = (uint64_t *)kmalloc(new_capacity * sizeof(uint64_t));
        if (!new_va || !new_pa) { if (new_va) kfree(new_va); if (new_pa) kfree(new_pa); return -1; }
        if (image->page_count) {
            memcpy(new_va, image->virtual_pages, image->page_count * sizeof(uint64_t));
            memcpy(new_pa, image->physical_pages, image->page_count * sizeof(uint64_t));
        }
        if (image->virtual_pages) kfree(image->virtual_pages);
        if (image->physical_pages) kfree(image->physical_pages);
        image->virtual_pages = new_va; image->physical_pages = new_pa;
        image->page_capacity = new_capacity;
    }
    image->virtual_pages[image->page_count] = va;
    image->physical_pages[image->page_count] = pa;
    ++image->page_count;
    return 0;
}
static int image_has_page(const fluxmax_user_image_t *image, uint64_t va) {
    for (size_t i = 0; i < image->page_count; ++i)
        if (image->virtual_pages[i] == va) return 1;
    return 0;
}
static int map_zero_page(fluxmax_user_image_t *image, uint64_t va, uint64_t flags) {
    if (image_has_page(image, va)) return -1; /* No ambiguous overlapping PT_LOAD pages. */
    void *page = pmm_alloc_page();
    if (!page) return -1;
    memset(page, 0, ELF_PAGE);
    uint64_t physical = (uint64_t)(uintptr_t)page;
    /* PMM returns the HHDM virtual alias; the VMM API wants a physical address. */
    uint64_t hhdm = boot_hhdm_offset();
    if (!hhdm || physical < hhdm) { pmm_free_page(page); return -1; }
    physical -= hhdm;
    if (vmm_space_map(image->space, va, physical, flags | VMM_PRESENT | VMM_USER) != 0) {
        pmm_free_page(page); return -1;
    }
    if (image_record_page(image, va, physical) != 0) {
        (void)vmm_space_unmap(image->space, va);
        pmm_free_page(page); return -1;
    }
    return 0;
}
static int copy_segment(fluxmax_user_image_t *image, uint64_t destination,
                        const uint8_t *source, size_t length) {
    while (length) {
        size_t chunk = (size_t)(ELF_PAGE - (destination & (ELF_PAGE - 1u)));
        if (chunk > length) chunk = length;
        if (vmm_space_copy_to(image->space, destination, source, chunk) != 0) return -1;
        destination += chunk; source += chunk; length -= chunk;
    }
    return 0;
}

int fluxmax_elf_load(const void *file_data, size_t file_size, fluxmax_user_image_t *out) {
    if (!file_data || !out || file_size < sizeof(elf64_ehdr_t)) return -1;
    memset(out, 0, sizeof(*out));
    const uint8_t *bytes = (const uint8_t *)file_data;
    const elf64_ehdr_t *eh = (const elf64_ehdr_t *)file_data;
    if (eh->ident[0] != 0x7Fu || eh->ident[1] != 'E' || eh->ident[2] != 'L' || eh->ident[3] != 'F' ||
        eh->ident[4] != ELF_CLASS_64 || eh->ident[5] != ELF_DATA_LSB || eh->ident[6] != ELF_VERSION_CURRENT ||
        eh->type != ELF_ET_EXEC || eh->machine != ELF_EM_X86_64 || eh->version != ELF_VERSION_CURRENT ||
        eh->ehsize != sizeof(*eh) || eh->phentsize != sizeof(elf64_phdr_t) || !eh->phnum || eh->phnum > 128u ||
        !bounds_ok(file_size, eh->phoff, (uint64_t)eh->phnum * eh->phentsize)) return -1;
    if (!valid_user_range(eh->entry, 1u)) return -1;

    out->space = vmm_space_create();
    if (!out->space) return -2;
    const elf64_phdr_t *ph = (const elf64_phdr_t *)(bytes + eh->phoff);
    unsigned int loaded_segments = 0;
    for (uint16_t i = 0; i < eh->phnum; ++i) {
        if (ph[i].type != ELF_PT_LOAD || ph[i].memsz == 0) continue;
        if (ph[i].memsz < ph[i].filesz || !bounds_ok(file_size, ph[i].offset, ph[i].filesz) ||
            !valid_user_range(ph[i].vaddr, ph[i].memsz) || ph[i].vaddr + ph[i].memsz < ph[i].vaddr) goto fail;
        if (ph[i].align && ((ph[i].align & (ph[i].align - 1u)) != 0)) goto fail;
        uint64_t start = ph[i].vaddr & ~(ELF_PAGE - 1u);
        uint64_t end = (ph[i].vaddr + ph[i].memsz + ELF_PAGE - 1u) & ~(ELF_PAGE - 1u);
        uint64_t flags = VMM_USER;
        if (ph[i].flags & ELF_PF_W) flags |= VMM_WRITABLE;
        if (!(ph[i].flags & ELF_PF_X)) flags |= VMM_NO_EXECUTE;
        for (uint64_t va = start; va < end; va += ELF_PAGE)
            if (map_zero_page(out, va, flags) != 0) goto fail;
        if (ph[i].filesz && copy_segment(out, ph[i].vaddr, bytes + ph[i].offset, (size_t)ph[i].filesz) != 0) goto fail;
        ++loaded_segments;
    }
    if (!loaded_segments) goto fail;
    {
        uint64_t phys, flags;
        if (vmm_space_translate(out->space, eh->entry, &phys, &flags) != 0 || !(flags & VMM_USER) ||
            (flags & VMM_NO_EXECUTE)) goto fail;
    }
    for (uint64_t va = USER_STACK_BASE; va < USER_STACK_TOP; va += ELF_PAGE)
        if (map_zero_page(out, va, VMM_WRITABLE | VMM_NO_EXECUTE) != 0) goto fail;
    out->entry = eh->entry;
    out->stack_pointer = (USER_STACK_TOP - 16u) & ~0xFULL;
    return 0;
fail:
    fluxmax_user_image_destroy(out);
    return -3;
}

void fluxmax_user_image_destroy(fluxmax_user_image_t *image) {
    if (!image) return;
    for (size_t i = image->page_count; i > 0; --i) {
        uint64_t physical = image->physical_pages[i - 1];
        (void)vmm_space_unmap(image->space, image->virtual_pages[i - 1]);
        pmm_free_page((void *)(uintptr_t)(physical + boot_hhdm_offset()));
    }
    if (image->space) vmm_space_destroy(image->space);
    if (image->virtual_pages) kfree(image->virtual_pages);
    if (image->physical_pages) kfree(image->physical_pages);
    memset(image, 0, sizeof(*image));
}
