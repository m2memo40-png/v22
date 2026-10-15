#include "kernel/bootinfo.h"
#include "kernel/log.h"
#include "kernel/panic.h"
#include <stddef.h>

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_start_marker[] = LIMINE_REQUESTS_START_MARKER;
__attribute__((used, section(".limine_requests")))
volatile uint64_t limine_base_revision[3] = LIMINE_BASE_REVISION(3);
__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = { .id = LIMINE_HHDM_REQUEST_ID, .revision = 0 };
__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = { .id = LIMINE_MEMMAP_REQUEST_ID, .revision = 0 };
__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = { .id = LIMINE_FRAMEBUFFER_REQUEST_ID, .revision = 0 };
__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_end_marker[] = LIMINE_REQUESTS_END_MARKER;

struct limine_memmap_response *g_memmap;
struct limine_hhdm_response *g_hhdm;
struct limine_framebuffer *g_framebuffer;

void bootinfo_init(void) {
    if (limine_base_revision[2] != 0) panic("Limine base revision 3 unsupported");
    g_memmap = memmap_request.response;
    g_hhdm = hhdm_request.response;
    if (framebuffer_request.response && framebuffer_request.response->framebuffer_count > 0)
        g_framebuffer = framebuffer_request.response->framebuffers[0];
    if (!g_memmap || !g_hhdm) panic("Limine did not provide memory map/HHDM");
    log_write(LOG_INFO, "boot", "Limine responses accepted");
    if (g_framebuffer && g_framebuffer->address && g_framebuffer->width && g_framebuffer->height) {
        if (g_framebuffer->bpp == 32 && g_framebuffer->memory_model == LIMINE_FRAMEBUFFER_RGB)
            log_write(LOG_INFO, "video", "32-bit RGB framebuffer available; using firmware/bootloader display mode (no vendor GPU acceleration)");
        else
            log_write(LOG_WARN, "video", "Framebuffer found, but this build requires 32-bit RGB mode for its desktop");
    } else {
        log_write(LOG_WARN, "video", "No framebuffer response; the graphical desktop cannot start");
    }
}

uint64_t boot_hhdm_offset(void) { return g_hhdm ? g_hhdm->offset : 0; }
