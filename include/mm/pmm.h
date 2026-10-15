#ifndef FLUXMAX_PMM_H
#define FLUXMAX_PMM_H
#include <stdint.h>
void pmm_init(void);
void *pmm_alloc_page(void);
void pmm_free_page(void *virtual_address);
uint64_t pmm_free_pages(void);
#endif
