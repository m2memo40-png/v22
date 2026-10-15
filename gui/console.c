#include "gui/console.h"
#include "kernel/bootinfo.h"
#include "lib/mem.h"
#include "mm/heap.h"
#include <stdint.h>
#include <stddef.h>

#define CELL_W 8u
#define CELL_H 10u
#define COLOR_BG 0x10151Eu
static uint64_t cursor_x, cursor_y;
static int active;
static uint32_t *frame_backbuffer;
static size_t frame_backbuffer_pixels;
static int frame_active;

/* Five-column 5x7 bitmap font: A-Z, then 0-9. Converted to uppercase at output. */
static const uint8_t font[36][5] = {
 {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
 {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
 {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},
 {0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
 {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
 {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
 {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},
 {0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
 {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},
 {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x62,0x51,0x49,0x49,0x46},
 {0x22,0x41,0x49,0x49,0x36},{0x18,0x14,0x12,0x7F,0x10},{0x2F,0x49,0x49,0x49,0x31},
 {0x3E,0x49,0x49,0x49,0x32},{0x01,0x71,0x09,0x05,0x03},{0x36,0x49,0x49,0x49,0x36},
 {0x26,0x49,0x49,0x49,0x3E}
};

static uint32_t pack_rgb(uint8_t r, uint8_t g, uint8_t b) {
    struct limine_framebuffer *fb = g_framebuffer;
    if (!fb) return 0;
    uint32_t rr = fb->red_mask_size ? ((uint32_t)r * ((1u << fb->red_mask_size) - 1u) / 255u) : r;
    uint32_t gg = fb->green_mask_size ? ((uint32_t)g * ((1u << fb->green_mask_size) - 1u) / 255u) : g;
    uint32_t bb = fb->blue_mask_size ? ((uint32_t)b * ((1u << fb->blue_mask_size) - 1u) / 255u) : b;
    return (rr << fb->red_mask_shift) | (gg << fb->green_mask_shift) | (bb << fb->blue_mask_shift);
}
static void pixel(uint64_t x, uint64_t y, uint32_t color) {
    struct limine_framebuffer *fb = g_framebuffer;
    if (!fb || !fb->address || fb->bpp != 32 || x >= fb->width || y >= fb->height) return;
    if (frame_active && frame_backbuffer && y <= SIZE_MAX / fb->width) {
        frame_backbuffer[(size_t)y * (size_t)fb->width + (size_t)x] = color;
        return;
    }
    volatile uint32_t *row = (volatile uint32_t *)((uintptr_t)fb->address + y * fb->pitch);
    row[x] = color;
}
static void fill(uint64_t x, uint64_t y, uint64_t w, uint64_t h, uint32_t color) {
    struct limine_framebuffer *fb = g_framebuffer;
    if (!fb || !fb->address || fb->bpp != 32) return;
    if (x >= fb->width || y >= fb->height) return;
    if (w > fb->width - x) w = fb->width - x;
    if (h > fb->height - y) h = fb->height - y;
    if (frame_active && frame_backbuffer) {
        for (uint64_t yy = y; yy < y + h; ++yy) {
            uint32_t *row = frame_backbuffer + (size_t)yy * (size_t)fb->width + (size_t)x;
            for (uint64_t xx = 0; xx < w; ++xx) row[xx] = color;
        }
        return;
    }
    /* The framebuffer is MMIO/uncached on many systems: write rows directly
       instead of recomputing the address and bounds once for every pixel. */
    for (uint64_t yy = y; yy < y + h; ++yy) {
        volatile uint32_t *row = (volatile uint32_t *)((uintptr_t)fb->address + yy * fb->pitch) + x;
        for (uint64_t xx = 0; xx < w; ++xx) row[xx] = color;
    }
}
static void glyph(char c, uint64_t x, uint64_t y, uint32_t fg, uint32_t bg) {
    uint8_t cols[5] = {0,0,0,0,0};
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') memcpy(cols, font[(unsigned)(c - 'A')], 5);
    else if (c >= '0' && c <= '9') memcpy(cols, font[26u + (unsigned)(c - '0')], 5);
    else switch (c) {
        case '-': cols[0]=0x08; cols[1]=0x08; cols[2]=0x08; cols[3]=0x08; cols[4]=0x08; break;
        case '_': cols[0]=0x40; cols[1]=0x40; cols[2]=0x40; cols[3]=0x40; cols[4]=0x40; break;
        case '.': cols[2]=0x60; break;
        case ',': cols[2]=0x40; cols[3]=0x20; break;
        case ':': cols[2]=0x24; break;
        case ';': cols[2]=0x44; cols[3]=0x20; break;
        case '/': cols[0]=0x60; cols[1]=0x18; cols[2]=0x06; cols[3]=0x01; break;
        case '\\': cols[0]=0x01; cols[1]=0x06; cols[2]=0x18; cols[3]=0x60; break;
        case '!': cols[2]=0x5F; break;
        case '?': cols[0]=0x02; cols[1]=0x01; cols[2]=0x59; cols[3]=0x09; cols[4]=0x06; break;
        case '>': cols[0]=0x41; cols[1]=0x22; cols[2]=0x14; cols[3]=0x08; break;
        case '<': cols[0]=0x08; cols[1]=0x14; cols[2]=0x22; cols[3]=0x41; break;
        case '=': cols[0]=0x14; cols[1]=0x14; cols[2]=0x14; cols[3]=0x14; cols[4]=0x14; break;
        case '+': cols[1]=0x08; cols[2]=0x1C; cols[3]=0x08; break;
        case '*': cols[1]=0x14; cols[2]=0x08; cols[3]=0x14; break;
        case '(': cols[1]=0x1C; cols[2]=0x22; cols[3]=0x41; break;
        case ')': cols[1]=0x41; cols[2]=0x22; cols[3]=0x1C; break;
        case '[': cols[1]=0x7F; cols[2]=0x41; cols[3]=0x41; break;
        case ']': cols[1]=0x41; cols[2]=0x41; cols[3]=0x7F; break;
        case ' ': break;
        default: cols[0]=0x02; cols[1]=0x01; cols[2]=0x59; cols[3]=0x09; cols[4]=0x06; break;
    }
    fill(x, y, CELL_W, CELL_H, bg);
    for (uint32_t cx=0; cx<5; ++cx)
        for (uint32_t cy=0; cy<7; ++cy)
            if (cols[cx] & (1u << cy)) pixel(x + cx + 1u, y + cy + 1u, fg);
}
static void scroll_if_needed(void) {
    struct limine_framebuffer *fb = g_framebuffer;
    if (!fb) return;
    uint64_t margin = 12;
    if (cursor_y + CELL_H + margin < fb->height) return;
    for (uint64_t y = 30; y + CELL_H < fb->height; ++y) {
        volatile uint32_t *dst = (volatile uint32_t *)((uintptr_t)fb->address + y * fb->pitch);
        volatile uint32_t *src = (volatile uint32_t *)((uintptr_t)fb->address + (y + CELL_H) * fb->pitch);
        for (uint64_t x=0; x<fb->width; ++x) dst[x] = src[x];
    }
    fill(0, fb->height > CELL_H ? fb->height - CELL_H : 0, fb->width, CELL_H, pack_rgb(16,21,30));
    cursor_y = fb->height > CELL_H + margin ? fb->height - CELL_H - margin : 30;
}
void fluxmax_display_console_clear(void) {
    struct limine_framebuffer *fb = g_framebuffer;
    if (!fb || !fb->address || fb->bpp != 32 || fb->memory_model != LIMINE_FRAMEBUFFER_RGB) { active = 0; return; }
    fill(0,0,fb->width,fb->height,pack_rgb(16,21,30));
    fill(0,0,fb->width,28,pack_rgb(28,42,58));
    cursor_x=12; cursor_y=38; active=1;
    const char *title="FLUXMAX OS  |  KERNEL CONSOLE";
    for (size_t i=0; title[i] && (i+1u)*CELL_W<fb->width; ++i) glyph(title[i], 10u+i*CELL_W, 8u, pack_rgb(54,200,240), pack_rgb(28,42,58));
    cursor_x=12; cursor_y=38;
}
void fluxmax_display_console_init(void) { fluxmax_display_console_clear(); }
void fluxmax_display_console_set_enabled(int enabled) { active = enabled ? 1 : 0; }
void fluxmax_display_console_putc(char c) {
    struct limine_framebuffer *fb = g_framebuffer;
    if (!active || !fb) return;
    if (c=='\r') { cursor_x=12; return; }
    if (c=='\n') { cursor_x=12; cursor_y+=CELL_H; scroll_if_needed(); return; }
    if (c=='\b') {
        if (cursor_x > 12) cursor_x-=CELL_W;
        glyph(' ',cursor_x,cursor_y,pack_rgb(220,231,243),pack_rgb(16,21,30)); return;
    }
    if (cursor_x + CELL_W >= fb->width) { cursor_x=12; cursor_y+=CELL_H; scroll_if_needed(); }
    glyph(c,cursor_x,cursor_y,pack_rgb(220,231,243),pack_rgb(16,21,30));
    cursor_x+=CELL_W;
}
void fluxmax_display_console_write(const char *s) { if (!s) return; while (*s) fluxmax_display_console_putc(*s++); }


uint32_t fluxmax_display_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return pack_rgb(r, g, b);
}
void fluxmax_display_fill_rect(uint64_t x, uint64_t y, uint64_t width, uint64_t height, uint32_t color) {
    fill(x, y, width, height, color);
}
void fluxmax_display_size(uint64_t *width, uint64_t *height) {
    struct limine_framebuffer *fb = g_framebuffer;
    if (width) *width = fb ? fb->width : 0;
    if (height) *height = fb ? fb->height : 0;
}
void fluxmax_display_draw_text(uint64_t x, uint64_t y, const char *text, uint32_t fg, uint32_t bg) {
    struct limine_framebuffer *fb = g_framebuffer;
    if (!fb || !text) return;
    const uint64_t start_x = x;
    while (*text) {
        char c = *text++;
        if (c == '\r') continue;
        if (c == '\n') { x = start_x; y += CELL_H + 2u; continue; }
        if (x + CELL_W >= fb->width) { x = start_x; y += CELL_H + 2u; }
        if (y + CELL_H >= fb->height) return;
        glyph(c, x, y, fg, bg);
        x += CELL_W;
    }
}

int fluxmax_display_frame_begin(void) {
    struct limine_framebuffer *fb = g_framebuffer;
    frame_active = 0;
    if (!fb || !fb->address || fb->bpp != 32 || !fb->width || !fb->height ||
        fb->width > SIZE_MAX / fb->height) return 0;
    size_t pixels = (size_t)fb->width * (size_t)fb->height;
    if (pixels > SIZE_MAX / sizeof(uint32_t)) return 0;
    if (!frame_backbuffer || frame_backbuffer_pixels < pixels) {
        uint32_t *replacement = (uint32_t *)kmalloc(pixels * sizeof(uint32_t));
        if (!replacement) return 0;
        if (frame_backbuffer) kfree(frame_backbuffer);
        frame_backbuffer = replacement;
        frame_backbuffer_pixels = pixels;
    }
    frame_active = 1;
    return 1;
}

void fluxmax_display_frame_present(void) {
    struct limine_framebuffer *fb = g_framebuffer;
    if (!frame_active || !frame_backbuffer || !fb || !fb->address || fb->bpp != 32) {
        frame_active = 0;
        return;
    }
    frame_active = 0;
    for (uint64_t y = 0; y < fb->height; ++y) {
        volatile uint32_t *dst = (volatile uint32_t *)((uintptr_t)fb->address + y * fb->pitch);
        const uint32_t *src = frame_backbuffer + (size_t)y * (size_t)fb->width;
        for (uint64_t x = 0; x < fb->width; ++x) dst[x] = src[x];
    }
}

int fluxmax_display_read_pixel(uint64_t x, uint64_t y, uint32_t *raw_color) {
    struct limine_framebuffer *fb = g_framebuffer;
    if (!raw_color || !fb || !fb->address || fb->bpp != 32 || x >= fb->width || y >= fb->height)
        return 0;
    const volatile uint32_t *row = (const volatile uint32_t *)((uintptr_t)fb->address + y * fb->pitch);
    *raw_color = row[x];
    return 1;
}

int fluxmax_display_write_pixel(uint64_t x, uint64_t y, uint32_t raw_color) {
    struct limine_framebuffer *fb = g_framebuffer;
    if (!fb || !fb->address || fb->bpp != 32 || x >= fb->width || y >= fb->height) return 0;
    volatile uint32_t *row = (volatile uint32_t *)((uintptr_t)fb->address + y * fb->pitch);
    row[x] = raw_color;
    return 1;
}
