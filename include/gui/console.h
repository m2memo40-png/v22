#ifndef FLUXMAX_GUI_CONSOLE_H
#define FLUXMAX_GUI_CONSOLE_H
#include <stdint.h>
void fluxmax_display_console_init(void);
void fluxmax_display_console_set_enabled(int enabled);
void fluxmax_display_console_clear(void);
void fluxmax_display_console_putc(char c);
void fluxmax_display_console_write(const char *s);
/* Shared framebuffer primitives used by the small built-in desktop. Colors are
 * packed according to the active Limine RGB framebuffer masks. */
uint32_t fluxmax_display_rgb(uint8_t r, uint8_t g, uint8_t b);
void fluxmax_display_fill_rect(uint64_t x, uint64_t y, uint64_t width, uint64_t height, uint32_t color);
void fluxmax_display_draw_text(uint64_t x, uint64_t y, const char *text, uint32_t fg, uint32_t bg);
void fluxmax_display_size(uint64_t *width, uint64_t *height);
/* Build a complete GUI frame in RAM, then present it to the firmware framebuffer. */
int fluxmax_display_frame_begin(void);
void fluxmax_display_frame_present(void);
/* Raw framebuffer pixel access used for saving/restoring the software pointer. */
int fluxmax_display_read_pixel(uint64_t x, uint64_t y, uint32_t *raw_color);
int fluxmax_display_write_pixel(uint64_t x, uint64_t y, uint32_t raw_color);
#endif
