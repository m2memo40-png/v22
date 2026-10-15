#include "gui/window_manager.h"
#include <assert.h>
#include <stdio.h>

#define TEST(name, expr) do { assert(expr); ++passed; puts("PASS " name); } while (0)
static int passed;
int main(void) {
    fluxmax_wm_t wm;
    TEST("reject too-small screen", fluxmax_wm_init(&wm, 100, 100) == -1);
    TEST("initialize 4 windows", fluxmax_wm_init(&wm, 1024, 768) == 0);
    TEST("system window visible", fluxmax_wm_get(&wm, FLUXMAX_WM_SYSTEM)->visible == 1);
    TEST("initial windows fit before taskbar", fluxmax_wm_get(&wm, FLUXMAX_WM_NETWORK)->y + fluxmax_wm_get(&wm, FLUXMAX_WM_NETWORK)->height <= 768 - 34);
    TEST("browser initially hidden", fluxmax_wm_get(&wm, FLUXMAX_WM_BROWSER)->visible == 0);
    TEST("focus rejects hidden browser", fluxmax_wm_focus(&wm, FLUXMAX_WM_BROWSER) == -2);
    TEST("show browser", fluxmax_wm_show(&wm, FLUXMAX_WM_BROWSER) == 0);
    TEST("browser becomes active", fluxmax_wm_active(&wm)->id == FLUXMAX_WM_BROWSER);
    int oldx = fluxmax_wm_active(&wm)->x;
    TEST("move active window", fluxmax_wm_move_active(&wm, 20, 10) == 0 && fluxmax_wm_active(&wm)->x == oldx + 20);
    TEST("window move clamps to screen", fluxmax_wm_move_active(&wm, 5000, 5000) == 0 && fluxmax_wm_active(&wm)->x + fluxmax_wm_active(&wm)->width <= wm.screen_width && fluxmax_wm_active(&wm)->y + fluxmax_wm_active(&wm)->height <= wm.screen_height - 34);
    TEST("resize active window", fluxmax_wm_resize_active(&wm, 30, 20) == 0 && fluxmax_wm_active(&wm)->width >= 250);
    TEST("maximize active window", fluxmax_wm_toggle_maximize(&wm) == 0 && fluxmax_wm_active(&wm)->maximized);
    TEST("cannot move maximized window", fluxmax_wm_move_active(&wm, 10, 10) == -1);
    TEST("restore active window", fluxmax_wm_toggle_maximize(&wm) == 0 && !fluxmax_wm_active(&wm)->maximized);
    TEST("cycle focus", fluxmax_wm_cycle(&wm, -1) == 0 && fluxmax_wm_active(&wm)->id != FLUXMAX_WM_BROWSER);
    TEST("minimize active window", fluxmax_wm_toggle_minimize(&wm) == 0);
    TEST("close a window", fluxmax_wm_close(&wm, FLUXMAX_WM_FILES) == 0 && !fluxmax_wm_get(&wm, FLUXMAX_WM_FILES)->visible);
    TEST("reopen closed window", fluxmax_wm_show(&wm, FLUXMAX_WM_FILES) == 0 && fluxmax_wm_active(&wm)->id == FLUXMAX_WM_FILES);
    printf("%d window manager tests passed\n", passed);
    return passed == 18 ? 0 : 1;
}
