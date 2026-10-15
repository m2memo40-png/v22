#include "drivers/ps2_mouse.h"
#include "arch/x86_64/io.h"
#include "arch/x86_64/pic.h"
#include <stdint.h>

#define PS2_DATA       0x60u
#define PS2_STATUS     0x64u
#define PS2_COMMAND    0x64u
#define PS2_STATUS_OUT 0x01u
#define PS2_STATUS_IN  0x02u
#define PS2_STATUS_AUX 0x20u
#define PS2_AUX_IRQ    12u
#define PS2_QUEUE_SIZE 64u
#define PS2_WAIT_LIMIT 100000u
#define PS2_ACK        0xFAu
#define PS2_RESEND     0xFEu

static fluxmax_ps2_mouse_decoder_t decoder;
static volatile fluxmax_mouse_event_t event_queue[PS2_QUEUE_SIZE];
static volatile uint8_t queue_head, queue_tail;
static volatile int available;
static uint8_t command_byte;

static int wait_input_empty(void) {
    for (uint32_t i = 0; i < PS2_WAIT_LIMIT; ++i)
        if ((inb(PS2_STATUS) & PS2_STATUS_IN) == 0) return 0;
    return -1;
}

static int wait_output_full(uint8_t *status_out) {
    for (uint32_t i = 0; i < PS2_WAIT_LIMIT; ++i) {
        uint8_t status = inb(PS2_STATUS);
        if (status & PS2_STATUS_OUT) {
            if (status_out) *status_out = status;
            return 0;
        }
    }
    return -1;
}

static int controller_command(uint8_t command) {
    if (wait_input_empty() != 0) return -1;
    outb(PS2_COMMAND, command);
    return 0;
}

static int controller_read_command_byte(uint8_t *out) {
    if (!out || controller_command(0x20u) != 0) return -1;
    uint8_t status = 0;
    if (wait_output_full(&status) != 0) return -1;
    /* The command-byte response is from the controller, not the auxiliary port. */
    *out = inb(PS2_DATA);
    return 0;
}

static int controller_write_command_byte(uint8_t value) {
    if (controller_command(0x60u) != 0 || wait_input_empty() != 0) return -1;
    outb(PS2_DATA, value);
    return 0;
}

static int mouse_write_once(uint8_t value, uint8_t *reply) {
    if (controller_command(0xD4u) != 0 || wait_input_empty() != 0) return -1;
    outb(PS2_DATA, value);
    uint8_t status = 0;
    if (wait_output_full(&status) != 0 || !(status & PS2_STATUS_AUX)) return -1;
    if (reply) *reply = inb(PS2_DATA);
    else (void)inb(PS2_DATA);
    return 0;
}

static int mouse_write(uint8_t value) {
    for (unsigned attempt = 0; attempt < 3u; ++attempt) {
        uint8_t reply = 0;
        if (mouse_write_once(value, &reply) != 0) return -1;
        if (reply == PS2_ACK) return 0;
        if (reply != PS2_RESEND) return -1;
    }
    return -1;
}

static void enqueue_event(const fluxmax_mouse_event_t *event) {
    uint8_t next = (uint8_t)((queue_head + 1u) & (PS2_QUEUE_SIZE - 1u));
    if (next == queue_tail) return; /* Do not block inside IRQ context. */
    event_queue[queue_head].dx = event->dx;
    event_queue[queue_head].dy = event->dy;
    event_queue[queue_head].buttons = event->buttons;
    __asm__ volatile("" ::: "memory");
    queue_head = next;
}

int ps2_mouse_init(void) {
    available = 0;
    queue_head = queue_tail = 0;
    fluxmax_ps2_mouse_decoder_reset(&decoder);
    pic_set_mask(PS2_AUX_IRQ, 1);

    /* Enable the auxiliary port, but keep its IRQ disabled while commands are
     * exchanged synchronously. All waits are bounded for machines without one. */
    if (controller_command(0xA8u) != 0 || controller_read_command_byte(&command_byte) != 0)
        return -1;
    command_byte = (uint8_t)((command_byte & (uint8_t)~0x22u)); /* clock on; IRQ12 off */
    if (controller_write_command_byte(command_byte) != 0) return -1;
    if (mouse_write(0xF6u) != 0 || mouse_write(0xF4u) != 0) return -1;

    command_byte = (uint8_t)((command_byte | 0x02u) & (uint8_t)~0x20u);
    if (controller_write_command_byte(command_byte) != 0) return -1;
    fluxmax_ps2_mouse_decoder_reset(&decoder);
    queue_head = queue_tail = 0;
    pic_set_mask(2u, 0); /* Cascade IRQ from the secondary PIC. */
    pic_set_mask(PS2_AUX_IRQ, 0);
    available = 1;
    return 0;
}

void ps2_mouse_irq(void) {
    uint8_t status = inb(PS2_STATUS);
    if (!(status & PS2_STATUS_OUT) || !(status & PS2_STATUS_AUX)) return;
    uint8_t byte = inb(PS2_DATA);
    fluxmax_mouse_event_t event;
    if (fluxmax_ps2_mouse_decode_byte(&decoder, byte, &event)) enqueue_event(&event);
}

int ps2_mouse_is_available(void) { return available != 0; }

int ps2_mouse_read_event(fluxmax_mouse_event_t *event) {
    if (!event || queue_tail == queue_head) return 0;
    __asm__ volatile("" ::: "memory");
    event->dx = event_queue[queue_tail].dx;
    event->dy = event_queue[queue_tail].dy;
    event->buttons = event_queue[queue_tail].buttons;
    queue_tail = (uint8_t)((queue_tail + 1u) & (PS2_QUEUE_SIZE - 1u));
    return 1;
}
