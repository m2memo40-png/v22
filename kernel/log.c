#include "kernel/log.h"
#include "arch/x86_64/io.h"
#include "gui/console.h"

#define COM1 0x3F8
static log_level_t minimum_level = LOG_TRACE;
static const char *const labels[] = { "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL" };

void serial_init(void) {
    outb(COM1 + 1, 0x00); outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03); outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03); outb(COM1 + 2, 0xC7); outb(COM1 + 4, 0x0B);
}
void serial_write_char(char c) {
    unsigned int timeout = 100000u;
    while (!(inb(COM1 + 5) & 0x20) && --timeout) { }
    if (timeout) outb(COM1, (uint8_t)c);
    fluxmax_display_console_putc(c);
}
int serial_try_read_char(char *out) {
    if (!out || !(inb(COM1 + 5) & 0x01u)) return 0;
    *out = (char)inb(COM1);
    return 1;
}
void serial_write(const char *s) { if (!s) return; while (*s) { if (*s == '\n') serial_write_char('\r'); serial_write_char(*s++); } }
/* QEMU debugcon is optional hardware; writing here is harmless on real PCs. */
void debugcon_write(const char *s) { if (!s) return; while (*s) outb(0xE9u, (uint8_t)*s++); }
void serial_write_hex(uint64_t n) {
    static const char h[] = "0123456789ABCDEF";
    serial_write("0x");
    int started = 0;
    for (int shift = 60; shift >= 0; shift -= 4) {
        unsigned int v = (unsigned int)((n >> shift) & 0xF);
        if (v || started || shift == 0) { serial_write_char(h[v]); started = 1; }
    }
}
void log_set_level(log_level_t level) { minimum_level = level; }
void log_write(log_level_t level, const char *component, const char *message) {
    if (level < minimum_level || level > LOG_FATAL) return;
    serial_write("["); serial_write(labels[level]); serial_write("] ");
    if (component) { serial_write(component); serial_write(": "); }
    serial_write(message ? message : "(null)"); serial_write("\n");
}
