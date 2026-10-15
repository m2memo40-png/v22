#ifndef FLUXMAX_GUI_COMPOSITOR_H
#define FLUXMAX_GUI_COMPOSITOR_H
#include <stddef.h>
#include <stdint.h>
typedef struct { int32_t x, y, width, height; } fluxmax_rect_t;
typedef struct { uint32_t *pixels; uint32_t width, height, pitch_pixels; } fluxmax_surface_t;
int fluxmax_rect_intersect(fluxmax_rect_t a, fluxmax_rect_t b, fluxmax_rect_t *out);
void fluxmax_surface_clear(fluxmax_surface_t *surface, uint32_t argb);
void fluxmax_surface_fill(fluxmax_surface_t *surface, fluxmax_rect_t rect, uint32_t argb);
void fluxmax_surface_blit(fluxmax_surface_t *dst, int32_t dx, int32_t dy,
                         const fluxmax_surface_t *src, fluxmax_rect_t source);
uint32_t fluxmax_argb_over(uint32_t foreground, uint32_t background);
#endif
