#include "arch/x86_64/idt.h"
#include "arch/x86_64/pic.h"
#include "arch/x86_64/pit.h"
#include "kernel/log.h"
#include "kernel/syscall.h"
#include "sched/process.h"
#include "kernel/panic.h"
#include "kernel/types.h"
#include "arch/x86_64/io.h"
#include "drivers/keyboard.h"
#include "drivers/ps2_mouse.h"
#include "sched/scheduler.h"
#include "lib/mem.h"
struct idt_gate { uint16_t offset_low, selector; uint8_t ist, attributes; uint16_t offset_mid; uint32_t offset_high, reserved; } PACKED;
struct idt_ptr { uint16_t limit; uint64_t base; } PACKED;
static struct idt_gate idt[256] ALIGNED(16);
extern void (*isr_stub_table[256])(void);
static void gate_set(uint8_t n, void (*handler)(void)) {
    uint64_t a = (uint64_t)handler;
    idt[n].offset_low=(uint16_t)a; idt[n].selector=0x08; idt[n].ist=0;
    idt[n].attributes=(n == 0x80u) ? 0xEEu : 0x8Eu; /* User-callable interrupt gate for int 0x80. */ idt[n].offset_mid=(uint16_t)(a>>16);
    idt[n].offset_high=(uint32_t)(a>>32); idt[n].reserved=0;
}
void idt_init(void) {
    memset(idt, 0, sizeof(idt));
    for (unsigned int i=0; i<256; ++i) gate_set((uint8_t)i, isr_stub_table[i]);
    struct idt_ptr p={.limit=sizeof(idt)-1,.base=(uint64_t)idt};
    __asm__ volatile("lidt %0" : : "m"(p) : "memory");
    log_write(LOG_INFO, "idt", "Installed 256 interrupt gates");
}
struct isr_frame *isr_dispatch(struct isr_frame *f) {
    if (!f) panic("ISR called without a frame");
    /* PUSH imm32 sign-extends vectors >= 128; normalize to the architectural byte. */
    uint8_t vector = (uint8_t)f->vector;
    if (vector < 32) {
        if ((f->cs & 3u) == 3u) {
            log_write(LOG_ERROR, "user-fault", "Terminating user task after CPU exception");
            serial_write("user exception: vector="); serial_write_hex(vector);
            serial_write(" error="); serial_write_hex(f->error_code);
            serial_write(" rip="); serial_write_hex(f->rip); serial_write("\n");
            if (process_exit(128 + (int)vector) < 0) panic("Could not terminate faulting user task");
            return fluxmax_sched_on_timer(f, pit_ticks());
        }
        log_write(LOG_FATAL, "exception", "Kernel CPU exception; vector/error/RIP follow");
        serial_write_hex(vector); serial_write(" / "); serial_write_hex(f->error_code); serial_write(" / "); serial_write_hex(f->rip); serial_write("\n");
        panic("unhandled CPU exception");
    }
    if (vector == 32) {
        pit_tick_increment();
    } else if (vector == 33) {
        ps2_keyboard_irq();
    } else if (vector == 44) { /* IRQ12: PS/2 auxiliary device (mouse). */
        ps2_mouse_irq();
    } else if (vector == 0x80u) {
        f->rax = (uint64_t)fluxmax_syscall_dispatch(f->rax, f->rdi, f->rsi, f->rdx, f->r10, f->r8, f->r9);
        /* Syscall entry is also a scheduling point; exit marks the current task zombie. */
        return fluxmax_sched_on_timer(f, pit_ticks());
    }
    if (vector >= 32 && vector < 48) pic_eoi((uint8_t)(vector - 32));
    if (vector == 32) return fluxmax_sched_on_timer(f, pit_ticks());
    return f;
}
