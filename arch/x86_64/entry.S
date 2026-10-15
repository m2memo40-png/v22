/* Early entry marker for QEMU debugcon (I/O port 0xE9).
 * This helps distinguish a bootloader/configuration problem from a kernel
 * fault before the COM1 serial driver can report anything.
 */
.section .text.boot,"ax"
.global _start
.type _start, @function
.extern fluxmax_kernel_main
_start:
    cli
    movb $'T', %al; outb %al, $0xe9
    movb $'U', %al; outb %al, $0xe9
    movb $'Y', %al; outb %al, $0xe9
    movb $'R', %al; outb %al, $0xe9
    movb $'Y', %al; outb %al, $0xe9
    movb $'U', %al; outb %al, $0xe9
    movb $':', %al; outb %al, $0xe9
    movb $'E', %al; outb %al, $0xe9
    movb $'N', %al; outb %al, $0xe9
    movb $'T', %al; outb %al, $0xe9
    movb $'R', %al; outb %al, $0xe9
    movb $'Y', %al; outb %al, $0xe9
    movb $'\n', %al; outb %al, $0xe9
    jmp fluxmax_kernel_main
.size _start, .-_start
