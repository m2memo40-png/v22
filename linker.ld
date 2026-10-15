OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

KERNEL_VMA = 0xffffffff80000000;
KERNEL_PMA = 0x200000;

PHDRS
{
    text PT_LOAD FLAGS(5);       /* R-X */
    rodata PT_LOAD FLAGS(4);     /* R-- */
    requests PT_LOAD FLAGS(6);   /* RW for Limine request negotiation */
    data PT_LOAD FLAGS(6);      /* RW */
}

SECTIONS
{
    . = KERNEL_VMA;
    .text ALIGN(4K) : AT(KERNEL_PMA + (ADDR(.text) - KERNEL_VMA)) {
        KEEP(*(.text.boot))
        *(.text .text.*)
    } :text
    .rodata ALIGN(4K) : AT(KERNEL_PMA + (ADDR(.rodata) - KERNEL_VMA)) {
        *(.rodata .rodata.*)
    } :rodata
    /* Must start on a fresh page: Limine rejects PT_LOAD permission conflicts
       when this writable request area shares the final .rodata page. */
    .limine_requests ALIGN(4K) : AT(KERNEL_PMA + (ADDR(.limine_requests) - KERNEL_VMA)) {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    } :requests
    .data ALIGN(4K) : AT(KERNEL_PMA + (ADDR(.data) - KERNEL_VMA)) {
        *(.data .data.*)
    } :data
    .bss ALIGN(4K) (NOLOAD) : AT(KERNEL_PMA + (ADDR(.bss) - KERNEL_VMA)) {
        *(COMMON)
        *(.bss .bss.*)
    } :data
    /DISCARD/ : { *(.eh_frame*) *(.note*) *(.comment*) }
}
