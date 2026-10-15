#include "gui/boot_splash.h"
#include "kernel/bootinfo.h"
#include "kernel/log.h"
#include <stdint.h>

static void put_pixel(struct limine_framebuffer *fb, uint64_t x, uint64_t y, uint32_t color) {
    if(!fb || !fb->address || fb->bpp!=32 || x>=fb->width || y>=fb->height) return;
    volatile uint32_t *row=(volatile uint32_t *)((uintptr_t)fb->address+y*fb->pitch);
    row[x]=color;
}
void boot_splash_draw(void) {
    struct limine_framebuffer *fb=g_framebuffer;
    if(!fb || fb->memory_model!=LIMINE_FRAMEBUFFER_RGB || fb->bpp!=32) {
        log_write(LOG_WARN,"display","No supported 32-bit RGB framebuffer; serial console remains available"); return;
    }
    /* Minimal boot splash only. This is not yet the phase-6 desktop/compositor. */
    for(uint64_t y=0;y<fb->height;y++) {
        uint32_t shade=(uint32_t)(10+(35*y/(fb->height?fb->height:1)));
        for(uint64_t x=0;x<fb->width;x++) put_pixel(fb,x,y,(shade<<16)|(shade<<8)|(shade+18));
    }
    uint64_t cx=fb->width/2, cy=fb->height/2;
    for(uint64_t r=0;r<72;r++) for(uint64_t a=0;a<360;a++) {
        /* A simple geometric mark without libm; avoids introducing a graphics stack early. */
        uint64_t x1=cx+(r*(a%3==0?1:0));
        uint64_t y1=cy+(r*(a%3==1?1:0));
        if(x1<fb->width && y1<fb->height) put_pixel(fb,x1,y1,0x39C5F0);
    }
    for(uint64_t y=cy+100;y<cy+108 && y<fb->height;y++)
        for(uint64_t x=cx>180?cx-180:0;x<cx+180 && x<fb->width;x++) put_pixel(fb,x,y,0x39C5F0);
    log_write(LOG_INFO,"display","Boot splash drawn; full desktop is phase 6");
}
