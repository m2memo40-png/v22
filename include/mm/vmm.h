#ifndef FLUXMAX_VMM_H
#define FLUXMAX_VMM_H
#include <stddef.h>
#include <stdint.h>

/* x86-64 page-table flags accepted by vmm_map()/vmm_space_map(). */
#define VMM_PRESENT       (1ULL << 0)
#define VMM_WRITABLE      (1ULL << 1)
#define VMM_USER          (1ULL << 2)
#define VMM_WRITE_THROUGH (1ULL << 3)
#define VMM_CACHE_DISABLE (1ULL << 4)
#define VMM_GLOBAL        (1ULL << 8)
#define VMM_NO_EXECUTE    (1ULL << 63)

typedef struct vmm_address_space vmm_address_space_t;

/* Operates on the Limine-supplied kernel page tables; 4 KiB mappings only. */
int vmm_init(void);
int vmm_map(uint64_t virtual_address, uint64_t physical_address, uint64_t flags);
int vmm_unmap(uint64_t virtual_address);
int vmm_translate(uint64_t virtual_address, uint64_t *physical_address);

/* Per-process page-table roots share supervisor kernel mappings but maintain
 * private user page-table paths. User virtual addresses must be in the lower
 * canonical half and page aligned. Data pages are owned by the caller. */
vmm_address_space_t *vmm_space_create(void);
void vmm_space_destroy(vmm_address_space_t *space);
int vmm_space_map(vmm_address_space_t *space, uint64_t virtual_address,
                  uint64_t physical_address, uint64_t flags);
int vmm_space_unmap(vmm_address_space_t *space, uint64_t virtual_address);
int vmm_space_translate(vmm_address_space_t *space, uint64_t virtual_address,
                        uint64_t *physical_address, uint64_t *effective_flags);
int vmm_space_copy_from(vmm_address_space_t *space, void *kernel_dst,
                        uint64_t user_src, size_t length);
/* Privileged loader copy: checks mapping is user-accessible but deliberately
 * bypasses user write protection to initialize read-only executable segments. */
int vmm_space_copy_to(vmm_address_space_t *space, uint64_t user_dst,
                      const void *kernel_src, size_t length);
uint64_t vmm_space_root_phys(const vmm_address_space_t *space);
void vmm_space_switch(vmm_address_space_t *space);
uint64_t vmm_current_root_phys(void);
#endif
