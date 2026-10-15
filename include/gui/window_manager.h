#ifndef FLUXMAX_GUI_WINDOW_MANAGER_H
#define FLUXMAX_GUI_WINDOW_MANAGER_H
#include <stdint.h>

#define FLUXMAX_WM_MAX_WINDOWS 4
#define FLUXMAX_WM_TITLE_HEIGHT 24

typedef enum {
    FLUXMAX_WM_SYSTEM = 0,
    FLUXMAX_WM_FILES = 1,
    FLUXMAX_WM_NETWORK = 2,
    FLUXMAX_WM_BROWSER = 3
} fluxmax_wm_window_id_t;

typedef struct {
    fluxmax_wm_window_id_t id;
    int32_t x, y, width, height;
    int32_t restore_x, restore_y, restore_width, restore_height;
    int visible, minimized, maximized;
    const char *title;
} fluxmax_wm_window_t;

typedef struct {
    fluxmax_wm_window_t windows[FLUXMAX_WM_MAX_WINDOWS];
    int32_t screen_width, screen_height;
    int active_index;
} fluxmax_wm_t;

int fluxmax_wm_init(fluxmax_wm_t *wm, int32_t screen_width, int32_t screen_height);
fluxmax_wm_window_t *fluxmax_wm_get(fluxmax_wm_t *wm, fluxmax_wm_window_id_t id);
const fluxmax_wm_window_t *fluxmax_wm_active(const fluxmax_wm_t *wm);
int fluxmax_wm_show(fluxmax_wm_t *wm, fluxmax_wm_window_id_t id);
int fluxmax_wm_close(fluxmax_wm_t *wm, fluxmax_wm_window_id_t id);
int fluxmax_wm_focus(fluxmax_wm_t *wm, fluxmax_wm_window_id_t id);
int fluxmax_wm_cycle(fluxmax_wm_t *wm, int direction);
int fluxmax_wm_move_active(fluxmax_wm_t *wm, int32_t dx, int32_t dy);
int fluxmax_wm_resize_active(fluxmax_wm_t *wm, int32_t dw, int32_t dh);
int fluxmax_wm_toggle_minimize(fluxmax_wm_t *wm);
int fluxmax_wm_toggle_maximize(fluxmax_wm_t *wm);
#endif
