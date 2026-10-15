#include "drivers/keyboard.h"
#include "drivers/ps2_keymap.h"
#include "arch/x86_64/io.h"
#include "arch/x86_64/pic.h"

#define KBD_DATA       0x60u
#define KBD_STATUS     0x64u
#define KBD_COMMAND    0x64u
#define KBD_STATUS_OUT 0x01u
#define KBD_STATUS_IN  0x02u
#define KBD_STATUS_AUX 0x20u
#define KBD_QUEUE_SIZE 256u
#define KBD_WAIT_LIMIT 100000u

static volatile uint8_t queue[KBD_QUEUE_SIZE];
static volatile uint16_t head, tail;
static fluxmax_ps2_keymap_state_t keymap;

static int wait_input_empty(void) {
    for (uint32_t i = 0; i < KBD_WAIT_LIMIT; ++i)
        if ((inb(KBD_STATUS) & KBD_STATUS_IN) == 0) return 0;
    return -1;
}

static int read_controller_command_byte(uint8_t *value) {
    if (!value || wait_input_empty() != 0) return -1;
    outb(KBD_COMMAND, 0x20u);
    for (uint32_t i = 0; i < KBD_WAIT_LIMIT; ++i) {
        uint8_t status = inb(KBD_STATUS);
        if (status & KBD_STATUS_OUT) {
            /* Do not mistake queued auxiliary-device data for the command byte. */
            if (status & KBD_STATUS_AUX) { (void)inb(KBD_DATA); continue; }
            *value = inb(KBD_DATA);
            return 0;
        }
    }
    return -1;
}

static int write_controller_command_byte(uint8_t value) {
    if (wait_input_empty() != 0) return -1;
    outb(KBD_COMMAND, 0x60u);
    if (wait_input_empty() != 0) return -1;
    outb(KBD_DATA, value);
    return 0;
}

static void best_effort_restore_keyboard(void) {
    uint8_t command_byte = 0;
    if (read_controller_command_byte(&command_byte) == 0) {
        command_byte |= 0x41u;
        command_byte &= (uint8_t)~0x10u;
        (void)write_controller_command_byte(command_byte);
    }
    if (wait_input_empty() == 0) outb(KBD_COMMAND, 0xAEu);
    pic_set_mask(1u, 0);
}

int ps2_keyboard_init(void) {
    /* Pause both PS/2 ports while reading/modifying the controller command byte. */
    if (wait_input_empty() != 0) goto init_failed;
    outb(KBD_COMMAND, 0xADu); /* Disable keyboard port. */
    if (wait_input_empty() != 0) goto init_failed;
    outb(KBD_COMMAND, 0xA7u); /* Disable auxiliary port during initialization. */

    /* Discard stale boot/firmware bytes; all operations are bounded. */
    for (uint32_t i = 0; i < 256u; ++i) {
        if (!(inb(KBD_STATUS) & KBD_STATUS_OUT)) break;
        (void)inb(KBD_DATA);
    }

    uint8_t command_byte = 0;
    if (read_controller_command_byte(&command_byte) != 0) goto init_failed;
    command_byte |= 0x41u;        /* Enable IRQ1 and Set-1 translation. */
    command_byte &= (uint8_t)~0x10u; /* Keyboard clock enabled. */
    if (write_controller_command_byte(command_byte) != 0) goto init_failed;
    if (wait_input_empty() != 0) goto init_failed;
    outb(KBD_COMMAND, 0xAEu); /* Enable first PS/2 port. */

    head = tail = 0;
    fluxmax_ps2_keymap_reset(&keymap);
    pic_set_mask(1u, 0);
    return 0;

init_failed:
    /* Do not strand an otherwise usable controller with its keyboard port off. */
    best_effort_restore_keyboard();
    return -1;
}

static void enqueue(char c) {
    uint16_t next = (uint16_t)((head + 1u) & (KBD_QUEUE_SIZE - 1u));
    if (next == tail) return; /* Drop newest input rather than block inside an IRQ. */
    queue[head] = (uint8_t)c;
    __asm__ volatile("" ::: "memory");
    head = next;
}

void ps2_keyboard_irq(void) {
    uint8_t status = inb(KBD_STATUS);
    if (!(status & KBD_STATUS_OUT)) return;
    uint8_t sc = inb(KBD_DATA);
    /* A spurious IRQ1 must not feed mouse bytes to the keyboard keymap. */
    if (status & KBD_STATUS_AUX) return;
    char c;
    if (fluxmax_ps2_keymap_feed(&keymap, sc, &c)) enqueue(c);
}

int ps2_keyboard_read_char(char *out) {
    if (!out || tail == head) return 0;
    __asm__ volatile("" ::: "memory");
    *out = (char)queue[tail];
    tail = (uint16_t)((tail + 1u) & (KBD_QUEUE_SIZE - 1u));
    return 1;
}
