#include "arch/x86_64/gdt.h"
#include "kernel/types.h"
#include "lib/mem.h"

struct gdt_entry { uint16_t limit_low; uint16_t base_low; uint8_t base_mid; uint8_t access; uint8_t granularity; uint8_t base_high; } PACKED;
struct tss64 { uint32_t reserved0; uint64_t rsp0, rsp1, rsp2; uint64_t reserved1; uint64_t ist1, ist2, ist3, ist4, ist5, ist6, ist7; uint64_t reserved2; uint16_t reserved3; uint16_t iomap_base; } PACKED;
struct gdt_ptr { uint16_t limit; uint64_t base; } PACKED;
static struct gdt_entry gdt[7] ALIGNED(16);
static struct tss64 tss ALIGNED(16);
extern void gdt_load(const struct gdt_ptr *ptr);

static void gdt_set(uint32_t index, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[index].limit_low = (uint16_t)(limit & 0xFFFF);
    gdt[index].base_low = (uint16_t)(base & 0xFFFF);
    gdt[index].base_mid = (uint8_t)((base >> 16) & 0xFF);
    gdt[index].access = access;
    gdt[index].granularity = (uint8_t)(((limit >> 16) & 0x0F) | (gran & 0xF0));
    gdt[index].base_high = (uint8_t)((base >> 24) & 0xFF);
}
void gdt_init(void) {
    memset(gdt, 0, sizeof(gdt)); memset(&tss, 0, sizeof(tss));
    gdt_set(1, 0, 0, 0x9A, 0x20); /* 64-bit ring-0 code */
    gdt_set(2, 0, 0, 0x92, 0x00); /* ring-0 data */
    gdt_set(3, 0, 0, 0xF2, 0x00); /* ring-3 data */
    gdt_set(4, 0, 0, 0xFA, 0x20); /* ring-3 code */
    tss.iomap_base = sizeof(tss);
    uint64_t base = (uint64_t)&tss; uint32_t limit = sizeof(tss) - 1;
    gdt_set(5, (uint32_t)base, limit, 0x89, 0x00);
    /* A 64-bit TSS descriptor spans two adjacent GDT entries. */
    ((uint64_t *)&gdt[6])[0] = base >> 32;
    struct gdt_ptr ptr = { .limit = sizeof(gdt)-1, .base = (uint64_t)gdt };
    gdt_load(&ptr);
}
void tss_set_rsp0(uint64_t stack_top) { tss.rsp0 = stack_top; }
