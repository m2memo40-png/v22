#include "gui/window_manager.h"

static int32_t clamp32(int32_t v, int32_t lo, int32_t hi) {
    if (hi < lo) hi = lo;
    return v < lo ? lo : (v > hi ? hi : v);
}

static int valid_id(fluxmax_wm_window_id_t id) {
    return (unsigned)id < FLUXMAX_WM_MAX_WINDOWS;
}

static void clamp_window(const fluxmax_wm_t *wm, fluxmax_wm_window_t *w) {
    int32_t min_w = wm->screen_width < 220 ? wm->screen_width : 220;
    int32_t min_h = wm->screen_height < 120 ? wm->screen_height : 120;
    int32_t max_w = wm->screen_width > 16 ? wm->screen_width - 16 : wm->screen_width;
    int32_t max_h = wm->screen_height > 88 ? wm->screen_height - 88 : wm->screen_height;
    w->width = clamp32(w->width, min_w, max_w);
    w->height = clamp32(w->height, min_h, max_h);
    w->x = clamp32(w->x, 0, wm->screen_width > w->width ? wm->screen_width - w->width : 0);
    w->y = clamp32(w->y, 38, wm->screen_height - w->height - 36);
}

int fluxmax_wm_init(fluxmax_wm_t *wm, int32_t sw, int32_t sh) {
    if (!wm || sw < 320 || sh < 220) return -1;
    wm->screen_width = sw; wm->screen_height = sh; wm->active_index = FLUXMAX_WM_SYSTEM;
    const char *titles[FLUXMAX_WM_MAX_WINDOWS] = {"System Monitor", "File Manager", "Network Status", "Fluxmax Web"};
    int32_t w = sw > 900 ? 420 : (sw * 3) / 5;
    int32_t h = sh > 650 ? 280 : (sh * 3) / 5;
    if (w > sw - 16) w = sw - 16;
    if (h > sh - 56) h = sh - 56;
    int32_t positions[FLUXMAX_WM_MAX_WINDOWS][2] = {
        {sw / 4, 64}, {sw / 3, 88}, {(sw * 2) / 5, 120}, {sw / 4, sh / 5}
    };
    for (int i = 0; i < FLUXMAX_WM_MAX_WINDOWS; ++i) {
        fluxmax_wm_window_t *win = &wm->windows[i];
        win->id = (fluxmax_wm_window_id_t)i;
        win->x = positions[i][0]; win->y = positions[i][1]; win->width = w; win->height = h;
        win->restore_x = win->x; win->restore_y = win->y; win->restore_width = w; win->restore_height = h;
        /* Keep the initial desktop uncluttered: launch other apps from the
           desktop icons or bottom panel instead of stacking every window. */
        win->visible = i == FLUXMAX_WM_SYSTEM; win->minimized = 0; win->maximized = 0; win->title = titles[i];
        clamp_window(wm, win);
    }
    return 0;
}

fluxmax_wm_window_t *fluxmax_wm_get(fluxmax_wm_t *wm, fluxmax_wm_window_id_t id) {
    return wm && valid_id(id) ? &wm->windows[(int)id] : 0;
}

const fluxmax_wm_window_t *fluxmax_wm_active(const fluxmax_wm_t *wm) {
    if (!wm || wm->active_index < 0 || wm->active_index >= FLUXMAX_WM_MAX_WINDOWS) return 0;
    const fluxmax_wm_window_t *w = &wm->windows[wm->active_index];
    return w->visible && !w->minimized ? w : 0;
}

int fluxmax_wm_focus(fluxmax_wm_t *wm, fluxmax_wm_window_id_t id) {
    fluxmax_wm_window_t *w = fluxmax_wm_get(wm, id);
    if (!w) return -1;
    if (!w->visible) return -2;
    w->minimized = 0; wm->active_index = (int)id; return 0;
}

int fluxmax_wm_show(fluxmax_wm_t *wm, fluxmax_wm_window_id_t id) {
    fluxmax_wm_window_t *w = fluxmax_wm_get(wm, id);
    if (!w) return -1;
    w->visible = 1; w->minimized = 0;
    return fluxmax_wm_focus(wm, id);
}

int fluxmax_wm_close(fluxmax_wm_t *wm, fluxmax_wm_window_id_t id) {
    fluxmax_wm_window_t *w = fluxmax_wm_get(wm, id);
    if (!w) return -1;
    w->visible = 0; w->minimized = 0; w->maximized = 0;
    if (wm->active_index == (int)id) (void)fluxmax_wm_cycle(wm, 1);
    return 0;
}

int fluxmax_wm_cycle(fluxmax_wm_t *wm, int direction) {
    if (!wm || !direction) return -1;
    int start = wm->active_index;
    for (int step = 1; step <= FLUXMAX_WM_MAX_WINDOWS; ++step) {
        int idx = (start + (direction > 0 ? step : -step)) % FLUXMAX_WM_MAX_WINDOWS;
        if (idx < 0) idx += FLUXMAX_WM_MAX_WINDOWS;
        fluxmax_wm_window_t *w = &wm->windows[idx];
        if (w->visible && !w->minimized) { wm->active_index = idx; return 0; }
    }
    wm->active_index = -1;
    return -2;
}

int fluxmax_wm_move_active(fluxmax_wm_t *wm, int32_t dx, int32_t dy) {
    const fluxmax_wm_window_t *active = fluxmax_wm_active(wm);
    if (!active || active->maximized) return -1;
    fluxmax_wm_window_t *w = &wm->windows[wm->active_index];
    w->x += dx; w->y += dy; clamp_window(wm, w); return 0;
}

int fluxmax_wm_resize_active(fluxmax_wm_t *wm, int32_t dw, int32_t dh) {
    const fluxmax_wm_window_t *active = fluxmax_wm_active(wm);
    if (!active || active->maximized) return -1;
    fluxmax_wm_window_t *w = &wm->windows[wm->active_index];
    w->width += dw; w->height += dh; clamp_window(wm, w); return 0;
}

int fluxmax_wm_toggle_minimize(fluxmax_wm_t *wm) {
    const fluxmax_wm_window_t *active = fluxmax_wm_active(wm);
    if (!active) return -1;
    fluxmax_wm_window_t *w = &wm->windows[wm->active_index];
    w->minimized = 1;
    return fluxmax_wm_cycle(wm, 1);
}

int fluxmax_wm_toggle_maximize(fluxmax_wm_t *wm) {
    const fluxmax_wm_window_t *active = fluxmax_wm_active(wm);
    if (!active) return -1;
    fluxmax_wm_window_t *w = &wm->windows[wm->active_index];
    if (!w->maximized) {
        w->restore_x = w->x; w->restore_y = w->y; w->restore_width = w->width; w->restore_height = w->height;
        w->x = 8; w->y = 42; w->width = wm->screen_width > 16 ? wm->screen_width - 16 : wm->screen_width;
        w->height = wm->screen_height > 92 ? wm->screen_height - 92 : wm->screen_height - 50;
        w->maximized = 1;
    } else {
        w->x = w->restore_x; w->y = w->restore_y; w->width = w->restore_width; w->height = w->restore_height;
        w->maximized = 0; clamp_window(wm, w);
    }
    return 0;
}
