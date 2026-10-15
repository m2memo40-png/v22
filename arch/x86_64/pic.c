#include "arch/x86_64/pic.h"
#include "arch/x86_64/io.h"
#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC2_CMD 0xA0
#define PIC2_DATA 0xA1
void pic_init_masked(void) {
    uint8_t m1=inb(PIC1_DATA), m2=inb(PIC2_DATA);
    outb(PIC1_CMD,0x11); io_wait(); outb(PIC2_CMD,0x11); io_wait();
    outb(PIC1_DATA,0x20); io_wait(); outb(PIC2_DATA,0x28); io_wait();
    outb(PIC1_DATA,0x04); io_wait(); outb(PIC2_DATA,0x02); io_wait();
    outb(PIC1_DATA,0x01); io_wait(); outb(PIC2_DATA,0x01); io_wait();
    (void)m1; (void)m2; outb(PIC1_DATA,0xFF); outb(PIC2_DATA,0xFF);
}
void pic_eoi(uint8_t irq) { if (irq >= 8) outb(PIC2_CMD,0x20); outb(PIC1_CMD,0x20); }
void pic_set_mask(uint8_t irq, int masked) {
    uint16_t port; uint8_t value;
    if (irq < 8) port=PIC1_DATA; else { irq=(uint8_t)(irq-8); port=PIC2_DATA; }
    value=inb(port); if(masked) value|=(uint8_t)(1u<<irq); else value&=(uint8_t)~(1u<<irq); outb(port,value);
}
