/* Minimal Limine protocol definitions for this bootstrap.
 * Protocol source: Limine-Bootloader/limine-protocol, SPDX-License-Identifier: 0BSD.
 * This is a deliberately small subset, not a replacement for the official header.
 */
#ifndef FLUXMAX_LIMINE_MIN_H
#define FLUXMAX_LIMINE_MIN_H
#include <stdint.h>
#define LIMINE_COMMON_MAGIC 0xc7b1dd30df4c8b88, 0x0a82e883a194f07b
#define LIMINE_REQUESTS_START_MARKER { 0xf6b8f4b39de7d1ae, 0xfab91a6940fcb9cf, 0x785c6ed015d3e316, 0x181e920a7852b9d9 }
#define LIMINE_REQUESTS_END_MARKER { 0xadc0e0531bb10d03, 0x9572709f31764c62 }
#define LIMINE_BASE_REVISION(N) { 0xf9562b2d5c95a6c8, 0x6a7b384944536bdc, (N) }
#define LIMINE_FRAMEBUFFER_REQUEST_ID { LIMINE_COMMON_MAGIC, 0x9d5827dcd881dd75, 0xa3148604f6fab11b }
#define LIMINE_HHDM_REQUEST_ID { LIMINE_COMMON_MAGIC, 0x48dcf1cb8ad2b852, 0x63984e959a98244b }
#define LIMINE_MEMMAP_REQUEST_ID { LIMINE_COMMON_MAGIC, 0x67cf3d9d378a806f, 0xe304acdfc50c3c62 }
#define LIMINE_MEMMAP_USABLE 0
#define LIMINE_FRAMEBUFFER_RGB 1
struct limine_framebuffer {
    void *address; uint64_t width, height, pitch; uint16_t bpp;
    uint8_t memory_model, red_mask_size, red_mask_shift, green_mask_size,
            green_mask_shift, blue_mask_size, blue_mask_shift, unused[7];
    uint64_t edid_size; void *edid;
    uint64_t mode_count; struct limine_video_mode **modes;
};
struct limine_video_mode {
    uint64_t pitch, width, height; uint16_t bpp; uint8_t memory_model;
    uint8_t red_mask_size, red_mask_shift, green_mask_size, green_mask_shift,
            blue_mask_size, blue_mask_shift;
};
struct limine_framebuffer_response { uint64_t revision, framebuffer_count; struct limine_framebuffer **framebuffers; };
struct limine_framebuffer_request { uint64_t id[4], revision; struct limine_framebuffer_response *response; };
struct limine_hhdm_response { uint64_t revision, offset; };
struct limine_hhdm_request { uint64_t id[4], revision; struct limine_hhdm_response *response; };
struct limine_memmap_entry { uint64_t base, length, type; };
struct limine_memmap_response { uint64_t revision, entry_count; struct limine_memmap_entry **entries; };
struct limine_memmap_request { uint64_t id[4], revision; struct limine_memmap_response *response; };
#endif
