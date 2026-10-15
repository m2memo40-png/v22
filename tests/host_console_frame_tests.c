#include "kernel/bootinfo.h"
#include "gui/console.h"
#include "mm/heap.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct limine_memmap_response *g_memmap;
struct limine_hhdm_response *g_hhdm;
struct limine_framebuffer *g_framebuffer;

void *kmalloc(size_t size) { return malloc(size); }
void kfree(void *ptr) { free(ptr); }

static unsigned passed;
#define TEST(name, expr) do { \
    if (!(expr)) { fprintf(stderr, "FAIL: %s\n", name); return 1; } \
    ++passed; printf("PASS: %s\n", name); \
} while (0)

int main(void) {
    enum { W = 32, H = 20 };
    const uint32_t sentinel = 0xA1B2C3D4u;
    uint32_t *storage = (uint32_t *)malloc((W * H + 2u) * sizeof(uint32_t));
    assert(storage);
    for (size_t i = 0; i < W * H + 2u; ++i) storage[i] = sentinel;
    struct limine_framebuffer fb = {
        .address = &storage[1], .width = W, .height = H, .pitch = W * sizeof(uint32_t),
        .bpp = 32, .memory_model = LIMINE_FRAMEBUFFER_RGB,
        .red_mask_size = 8, .red_mask_shift = 16,
        .green_mask_size = 8, .green_mask_shift = 8,
        .blue_mask_size = 8, .blue_mask_shift = 0
    };
    g_framebuffer = &fb;

    TEST("framebuffer begin allocates RAM backbuffer", fluxmax_display_frame_begin() == 1);
    uint32_t base_color = fluxmax_display_rgb(4, 8, 12);
    uint32_t color = fluxmax_display_rgb(10, 20, 30);
    /* The desktop compositor paints every pixel of each frame. */
    fluxmax_display_fill_rect(0, 0, W, H, base_color);
    fluxmax_display_fill_rect(2, 3, 5, 4, color);
    TEST("drawing is not visible before frame present", storage[1 + 3 * W + 2] == sentinel);
    TEST("framebuffer guards remain untouched before present", storage[0] == sentinel && storage[W * H + 1] == sentinel);
    fluxmax_display_frame_present();
    TEST("frame present copies rendered pixels", storage[1 + 3 * W + 2] == color);
    TEST("filled rectangle leaves outside pixels at the frame background", storage[1 + 2 * W + 2] == base_color && storage[1 + 3 * W + 7] == base_color);
    uint32_t raw = 0;
    TEST("raw framebuffer read returns exact pixel", fluxmax_display_read_pixel(2, 3, &raw) && raw == color);
    TEST("raw framebuffer write updates exact pixel", fluxmax_display_write_pixel(31, 19, 0x55667788u) && storage[1 + 19 * W + 31] == 0x55667788u);
    TEST("out-of-bounds pixel read is rejected", !fluxmax_display_read_pixel(W, H, &raw));
    TEST("out-of-bounds pixel write is rejected", !fluxmax_display_write_pixel(W, H, 0));
    TEST("framebuffer guard words stay intact", storage[0] == sentinel && storage[W * H + 1] == sentinel);
    free(storage);
    printf("Framebuffer frame tests: %u passed, 0 failed.\n", passed);
    return 0;
}
