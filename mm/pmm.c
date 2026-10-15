#include "mm/pmm.h"
#include "kernel/bootinfo.h"
#include "kernel/log.h"
#include "kernel/panic.h"
#include "kernel/types.h"
#include <stddef.h>
static uint64_t free_head_phys;
static uint64_t free_count;
static uint64_t hhdm;
static inline uint64_t *phys_word(uint64_t phys) { return (uint64_t *)(uintptr_t)(phys+hhdm); }
static uint64_t pmm_irq_save(void) { uint64_t f; __asm__ volatile("pushfq; popq %0; cli" : "=r"(f) : : "memory"); return f; }
static void pmm_irq_restore(uint64_t f) { if (f & (1ULL << 9)) __asm__ volatile("sti" : : : "memory"); }
void pmm_init(void) {
    uint64_t irq_flags = pmm_irq_save();
    free_head_phys=0; free_count=0; hhdm=boot_hhdm_offset();
    if (!g_memmap) panic("PMM has no Limine memory map");
    for(uint64_t i=0;i<g_memmap->entry_count;i++) {
        struct limine_memmap_entry *e=g_memmap->entries[i];
        if(!e || e->type!=LIMINE_MEMMAP_USABLE) continue;
        uint64_t start=(e->base+PAGE_SIZE-1)&~(PAGE_SIZE-1);
        uint64_t end=(e->base+e->length)&~(PAGE_SIZE-1);
        if(start<0x100000) start=0x100000;
        for(uint64_t p=start;p<end;p+=PAGE_SIZE) {
            *phys_word(p)=free_head_phys; free_head_phys=p; ++free_count;
        }
    }
    log_write(LOG_INFO,"pmm","Physical page allocator initialized from usable Limine ranges");
    pmm_irq_restore(irq_flags);
}
void *pmm_alloc_page(void) {
    uint64_t irq_flags = pmm_irq_save();
    if(!free_head_phys) { pmm_irq_restore(irq_flags); return NULL; }
    uint64_t p=free_head_phys; free_head_phys=*phys_word(p); --free_count;
    void *result = (void *)(uintptr_t)(p+hhdm);
    pmm_irq_restore(irq_flags);
    return result;
}
void pmm_free_page(void *address) {
    uint64_t v=(uint64_t)(uintptr_t)address;
    if(v<hhdm) return;
    uint64_t p=v-hhdm;
    if((p&(PAGE_SIZE-1))!=0 || p<0x100000) return;
    uint64_t irq_flags = pmm_irq_save();
    *phys_word(p)=free_head_phys; free_head_phys=p; ++free_count;
    pmm_irq_restore(irq_flags);
}
uint64_t pmm_free_pages(void) {
    uint64_t irq_flags = pmm_irq_save();
    uint64_t result = free_count;
    pmm_irq_restore(irq_flags);
    return result;
}
