#include "gui/compositor.h"

static int32_t min_i32(int32_t a, int32_t b) { return a < b ? a : b; }
static int32_t max_i32(int32_t a, int32_t b) { return a > b ? a : b; }

int fluxmax_rect_intersect(fluxmax_rect_t a, fluxmax_rect_t b, fluxmax_rect_t *out) {
    if (!out || a.width <= 0 || a.height <= 0 || b.width <= 0 || b.height <= 0) return 0;
    int32_t left = max_i32(a.x, b.x);
    int32_t top = max_i32(a.y, b.y);
    int32_t right = min_i32(a.x + a.width, b.x + b.width);
    int32_t bottom = min_i32(a.y + a.height, b.y + b.height);
    if (right <= left || bottom <= top) return 0;
    *out = (fluxmax_rect_t){left, top, right - left, bottom - top};
    return 1;
}

void fluxmax_surface_clear(fluxmax_surface_t *surface, uint32_t argb) {
    if (!surface || !surface->pixels || !surface->width || !surface->height) return;
    for (uint32_t y = 0; y < surface->height; ++y) {
        uint32_t *row = surface->pixels + (size_t)y * surface->pitch_pixels;
        for (uint32_t x = 0; x < surface->width; ++x) row[x] = argb;
    }
}

void fluxmax_surface_fill(fluxmax_surface_t *surface, fluxmax_rect_t rect, uint32_t argb) {
    if (!surface || !surface->pixels || rect.width <= 0 || rect.height <= 0) return;
    fluxmax_rect_t bounds = {0, 0, (int32_t)surface->width, (int32_t)surface->height};
    fluxmax_rect_t clipped;
    if (!fluxmax_rect_intersect(rect, bounds, &clipped)) return;
    for (int32_t y = clipped.y; y < clipped.y + clipped.height; ++y) {
        uint32_t *row = surface->pixels + (size_t)y * surface->pitch_pixels;
        for (int32_t x = clipped.x; x < clipped.x + clipped.width; ++x) row[x] = argb;
    }
}

uint32_t fluxmax_argb_over(uint32_t fg, uint32_t bg) {
    uint32_t alpha = fg >> 24;
    uint32_t inverse = 255u - alpha;
    uint32_t r = (((fg >> 16) & 255u) * alpha + ((bg >> 16) & 255u) * inverse + 127u) / 255u;
    uint32_t g = (((fg >> 8) & 255u) * alpha + ((bg >> 8) & 255u) * inverse + 127u) / 255u;
    uint32_t b = ((fg & 255u) * alpha + (bg & 255u) * inverse + 127u) / 255u;
    uint32_t a = alpha + (((bg >> 24) * inverse + 127u) / 255u);
    return (a << 24) | (r << 16) | (g << 8) | b;
}

void fluxmax_surface_blit(fluxmax_surface_t *dst, int32_t dx, int32_t dy,
                         const fluxmax_surface_t *src, fluxmax_rect_t source) {
    if (!dst || !src || !dst->pixels || !src->pixels || source.width <= 0 || source.height <= 0) return;
    fluxmax_rect_t src_bounds = {0, 0, (int32_t)src->width, (int32_t)src->height};
    fluxmax_rect_t clipped_source;
    if (!fluxmax_rect_intersect(source, src_bounds, &clipped_source)) return;
    dx += clipped_source.x - source.x;
    dy += clipped_source.y - source.y;
    fluxmax_rect_t dst_rect = {dx, dy, clipped_source.width, clipped_source.height};
    fluxmax_rect_t dst_bounds = {0, 0, (int32_t)dst->width, (int32_t)dst->height};
    fluxmax_rect_t clipped_dst;
    if (!fluxmax_rect_intersect(dst_rect, dst_bounds, &clipped_dst)) return;
    int32_t src_x = clipped_source.x + clipped_dst.x - dst_rect.x;
    int32_t src_y = clipped_source.y + clipped_dst.y - dst_rect.y;
    for (int32_t y = 0; y < clipped_dst.height; ++y) {
        const uint32_t *s = src->pixels + (size_t)(src_y + y) * src->pitch_pixels + src_x;
        uint32_t *d = dst->pixels + (size_t)(clipped_dst.y + y) * dst->pitch_pixels + clipped_dst.x;
        for (int32_t x = 0; x < clipped_dst.width; ++x) d[x] = s[x];
    }
}
