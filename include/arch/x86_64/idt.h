#ifndef FLUXMAX_IDT_H
#define FLUXMAX_IDT_H
#include <stdint.h>
#include <stddef.h>
struct isr_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    /* In 64-bit mode, hardware IRET frames include RSP and SS even when the
       interrupt did not change CPL. These two slots must be represented. */
    uint64_t vector, error_code, rip, cs, rflags, rsp, ss;
};
_Static_assert(offsetof(struct isr_frame, rip) == 136, "ISR RIP offset must match isr_stubs.S");
_Static_assert(offsetof(struct isr_frame, rflags) == 152, "ISR RFLAGS offset must match isr_stubs.S");
_Static_assert(offsetof(struct isr_frame, rsp) == 160, "ISR RSP offset must match long-mode IRET frame");
_Static_assert(offsetof(struct isr_frame, ss) == 168, "ISR SS offset must match long-mode IRET frame");
_Static_assert(sizeof(struct isr_frame) == 176, "ISR frame size must match long-mode hardware frame");
void idt_init(void);
struct isr_frame *isr_dispatch(struct isr_frame *frame);
extern void (*isr_stub_table[256])(void);
#endif
