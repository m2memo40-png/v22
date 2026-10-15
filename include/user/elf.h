#ifndef FLUXMAX_USER_ELF_H
#define FLUXMAX_USER_ELF_H
#include <stddef.h>
#include <stdint.h>
#include "mm/vmm.h"

typedef struct fluxmax_user_image {
    vmm_address_space_t *space;
    uint64_t entry;
    uint64_t stack_pointer;
    uint64_t *virtual_pages;
    uint64_t *physical_pages;
    size_t page_count;
    size_t page_capacity;
} fluxmax_user_image_t;

/* Loads static ELF64 x86-64 ET_EXEC binaries into a private user address space.
 * Dynamic linking, shared objects, and ET_DYN are deliberately rejected. */
int fluxmax_elf_load(const void *file_data, size_t file_size, fluxmax_user_image_t *out);
void fluxmax_user_image_destroy(fluxmax_user_image_t *image);
#endif
