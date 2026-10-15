#include "arch/x86_64/pit.h"
#include "arch/x86_64/io.h"
static volatile uint64_t ticks;
void pit_init(uint32_t hz) {
    if (hz == 0) return;
    uint32_t divisor=1193182u/hz; if(divisor==0) divisor=1; if(divisor>65535) divisor=65535;
    outb(0x43,0x36); outb(0x40,(uint8_t)(divisor&0xFF)); outb(0x40,(uint8_t)((divisor>>8)&0xFF));
    /* IRQ0 intentionally stays masked until the scheduler/timer policy is ready. */
}
uint64_t pit_ticks(void) { return ticks; }
void pit_tick_increment(void) { ++ticks; }
