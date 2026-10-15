#include "gui/desktop.h"
#include "gui/console.h"
#include "gui/window_manager.h"
#include "kernel/bootinfo.h"
#include "kernel/log.h"
#include "mm/pmm.h"
#include "arch/x86_64/pit.h"
#include "arch/x86_64/io.h"
#include "drivers/keyboard.h"
#include "drivers/ps2_mouse.h"
#include "fs/vfs.h"
#include "browser/browser.h"
#include "net/net.h"
#include "drivers/storage/ata_pio.h"
#include <stdint.h>
#include <stddef.h>

#define BROWSER_CAPTURE_CAP 32768u
#define WM_TOPBAR 38u
#define WM_TASKBAR 38u
#define WM_CONTENT_TOP 29u

static uint64_t screen_w, screen_h;
static fluxmax_wm_t wm;
static int wm_initialized;
static volatile int desktop_foreground;
static int url_entry;
static char browser_url[272];
static size_t browser_url_len;
static char browser_capture[BROWSER_CAPTURE_CAP];
static size_t browser_capture_len;
static unsigned browser_scroll;
static int32_t mouse_x, mouse_y;
static uint8_t previous_mouse_buttons;
static int mouse_position_initialized;
static int dragging_window = -1;
static int mouse_cursor_drawn;
static int mouse_smoke_marker_sent;
static int keyboard_smoke_marker_sent;
static int32_t mouse_cursor_saved_x, mouse_cursor_saved_y;
static uint32_t mouse_cursor_under[14][9];
static uint8_t mouse_cursor_under_valid[14][9];
static void draw_desktop(void);
static void erase_mouse_cursor(void);

static uint32_t c(unsigned r, unsigned g, unsigned b) {
    return fluxmax_display_rgb((uint8_t)r, (uint8_t)g, (uint8_t)b);
}
static void rect(uint64_t x, uint64_t y, uint64_t w, uint64_t h, unsigned r, unsigned g, unsigned b) {
    fluxmax_display_fill_rect(x, y, w, h, c(r,g,b));
}
static void text(uint64_t x, uint64_t y, const char *s, uint32_t fg, uint32_t bg) {
    fluxmax_display_draw_text(x, y, s, fg, bg);
}
static void clipped_text(uint64_t x, uint64_t y, const char *s, size_t max_chars,
                         uint32_t fg, uint32_t bg) {
    char tmp[256]; size_t n=0;
    if (!s) return;
    if (max_chars >= sizeof(tmp)) max_chars=sizeof(tmp)-1u;
    while (s[n] && n<max_chars) { tmp[n]=s[n]; ++n; }
    tmp[n]='\0'; text(x,y,tmp,fg,bg);
}
static void number_text(char *out, size_t cap, uint64_t value) {
    char reverse[24]; size_t n=0, o=0;
    if (!cap) return;
    do { reverse[n++]=(char)('0'+value%10u); value/=10u; } while(value && n<sizeof(reverse));
    while(n && o+1u<cap) out[o++]=reverse[--n];
    out[o]='\0';
}
static void browser_capture_char(char ch, void *ctx) {
    (void)ctx;
    if (browser_capture_len + 1u >= sizeof(browser_capture)) return;
    if (ch=='\r') return;
    browser_capture[browser_capture_len++]=ch;
    browser_capture[browser_capture_len]='\0';
}
static size_t browser_line_count(void) {
    size_t count=1, col=0;
    for(size_t i=0;i<browser_capture_len;++i) {
        if(browser_capture[i]=='\n') { ++count; col=0; }
        else if(++col >= 95u) { ++count; col=0; }
    }
    return count;
}
static void draw_browser_lines(const fluxmax_wm_window_t *w) {
    int32_t inner_x=w->x+10, inner_y=w->y+(int32_t)WM_CONTENT_TOP+31;
    int32_t max_chars=(w->width-20)/8;
    int32_t max_rows=(w->height-(int32_t)WM_CONTENT_TOP-39)/12;
    if(max_chars<8) max_chars=8;
    if(max_chars>160) max_chars=160;
    if(max_rows<1) max_rows=1;
    char line[161]; size_t col=0, logical_line=0; int32_t row=0;
    for(size_t i=0;i<=browser_capture_len && row<max_rows;++i) {
        char ch=i<browser_capture_len?browser_capture[i]:'\n';
        if(ch=='\r') continue;
        if(ch=='\n' || col>=(size_t)max_chars) {
            line[col]='\0';
            if(logical_line>=browser_scroll) {
                text((uint64_t)inner_x,(uint64_t)(inner_y+row*12),line,c(219,230,245),c(16,24,39));
                ++row;
            }
            ++logical_line; col=0;
            if(ch!='\n') line[col++]=ch;
        } else line[col++]=ch;
    }
    if(browser_scroll>browser_line_count()) browser_scroll=(unsigned)browser_line_count();
}
static void draw_window_contents(const fluxmax_wm_window_t *w) {
    uint32_t fg=c(225,235,249), muted=c(167,187,212), bg=c(17,27,44);
    int32_t x=w->x+10, y=w->y+(int32_t)WM_CONTENT_TOP;
    int32_t max_chars=(w->width-20)/8;
    if(max_chars<8) max_chars=8;
    if(max_chars>140) max_chars=140;
    if (w->height < 190) {
        clipped_text((uint64_t)x,(uint64_t)y,"WINDOW TOO SMALL - PRESS + TO RESIZE",max_chars, c(255,193,110), bg);
        return;
    }
    switch(w->id) {
    case FLUXMAX_WM_SYSTEM: {
        char freepages[24], ticks[24]; number_text(freepages,sizeof(freepages),pmm_free_pages()); number_text(ticks,sizeof(ticks),pit_ticks());
        text((uint64_t)x,(uint64_t)y,"FLUXMAX KERNEL STATUS",fg,bg);
        clipped_text((uint64_t)x,(uint64_t)(y+22),"x86-64 custom kernel / Limine",max_chars,muted,bg);
        clipped_text((uint64_t)x,(uint64_t)(y+44),"Physical free pages:",max_chars,fg,bg);
        text((uint64_t)x+176u,(uint64_t)(y+44),freepages,c(86,222,180),bg);
        clipped_text((uint64_t)x,(uint64_t)(y+66),"Scheduler ticks:",max_chars,fg,bg);
        text((uint64_t)x+144u,(uint64_t)(y+66),ticks,c(86,222,180),bg);
        clipped_text((uint64_t)x,(uint64_t)(y+94),"Ring-3 ELF + int 0x80 syscalls",max_chars,muted,bg);
        uint64_t video_w=0, video_h=0; fluxmax_display_size(&video_w,&video_h);
        char video_text[80]; size_t vn=0;
        const char video_prefix[]="Framebuffer: ";
        for(size_t i=0;i<sizeof(video_prefix)-1u && vn+1u<sizeof(video_text);++i) video_text[vn++]=video_prefix[i];
        char vw[24], vh[24]; number_text(vw,sizeof(vw),video_w); number_text(vh,sizeof(vh),video_h);
        for(size_t i=0;vw[i] && vn+1u<sizeof(video_text);++i) video_text[vn++]=vw[i];
        if(vn+1u<sizeof(video_text)) video_text[vn++]='x';
        for(size_t i=0;vh[i] && vn+1u<sizeof(video_text);++i) video_text[vn++]=vh[i];
        video_text[vn]='\0'; clipped_text((uint64_t)x,(uint64_t)(y+116),video_text,max_chars,c(86,222,180),bg);
        clipped_text((uint64_t)x,(uint64_t)(y+138),"Generic framebuffer; no vendor GPU acceleration",max_chars,c(255,193,110),bg);
        break;
    }
    case FLUXMAX_WM_FILES: {
        text((uint64_t)x,(uint64_t)y,"FILES AND STORAGE",fg,bg);
        clipped_text((uint64_t)x,(uint64_t)(y+22),"RAM VFS: /etc /home /tmp /bin",max_chars,muted,bg);
        char devs[24]; number_text(devs,sizeof(devs),fluxmax_ata_device_count());
        if (w->width > 340) {
            clipped_text((uint64_t)x,(uint64_t)(y+44),"Detected legacy ATA devices:",max_chars,fg,bg);
            text((uint64_t)x+224u,(uint64_t)(y+44),devs,c(86,222,180),bg);
        } else {
            clipped_text((uint64_t)x,(uint64_t)(y+44),"ATA device count: use shell 'disks'",max_chars,fg,bg);
        }
        clipped_text((uint64_t)x,(uint64_t)(y+66),"FAT32 root read/write, 8.3 filenames",max_chars,muted,bg);
        clipped_text((uint64_t)x,(uint64_t)(y+88),"No AHCI / NVMe / USB mass storage yet",max_chars,c(255,193,110),bg);
        clipped_text((uint64_t)x,(uint64_t)(y+116),"Disk writes may damage data; use a test disk",max_chars,c(255,151,135),bg);
        break;
    }
    case FLUXMAX_WM_NETWORK: {
        fluxmax_net_status_t status; fluxmax_net_get_status(&status);
        text((uint64_t)x,(uint64_t)y,"NETWORK STATUS",fg,bg);
        clipped_text((uint64_t)x,(uint64_t)(y+22),status.nic_present?status.driver_name:"No supported NIC detected",max_chars,muted,bg);
        clipped_text((uint64_t)x,(uint64_t)(y+44),status.ipv4_configured?"IPv4 configuration available":"IPv4 not configured (try shell: net)",max_chars, status.ipv4_configured?c(86,222,180):c(255,193,110),bg);
        clipped_text((uint64_t)x,(uint64_t)(y+66),"Ethernet / ARP / IPv4 / ICMP / UDP / DHCP / DNS",max_chars,muted,bg);
        clipped_text((uint64_t)x,(uint64_t)(y+88),"Single TCP connection + plain HTTP only",max_chars,muted,bg);
        clipped_text((uint64_t)x,(uint64_t)(y+116),"HTTPS/TLS, Wi-Fi and IPv6 unsupported; run 'internet'",max_chars,c(255,151,135),bg);
        break;
    }
    case FLUXMAX_WM_BROWSER: {
        clipped_text((uint64_t)x,(uint64_t)y,url_entry?"URL (HTTP only) - type then Enter":"WEB VIEW - B opens URL; J/K scroll",max_chars,fg,bg);
        if(url_entry) clipped_text((uint64_t)x,(uint64_t)(y+16),browser_url,max_chars,c(86,222,180),bg);
        else clipped_text((uint64_t)x,(uint64_t)(y+16),browser_url[0]?browser_url:"No page loaded",max_chars,c(86,222,180),bg);
        draw_browser_lines(w);
        break;
    }
    default: break;
    }
}
static void draw_one_window(const fluxmax_wm_window_t *w, int focused) {
    if(!w || !w->visible || w->minimized) return;
    uint32_t title=focused?c(30,67,102):c(34,44,61);
    rect((uint64_t)w->x-2u,(uint64_t)w->y-2u,(uint64_t)w->width+4u,(uint64_t)w->height+4u,7,11,19);
    rect((uint64_t)w->x,(uint64_t)w->y,(uint64_t)w->width,(uint64_t)w->height,17,27,44);
    rect((uint64_t)w->x,(uint64_t)w->y,(uint64_t)w->width,WM_TOPBAR-10u,focused?30:24,focused?67:38,focused?102:59);
    rect((uint64_t)w->x,(uint64_t)w->y,(uint64_t)w->width,2,focused?73:67,focused?175:84,focused?255:108);
    size_t title_chars=(w->width>125?(size_t)(w->width-116)/8u:4u);
    clipped_text((uint64_t)w->x+8u,(uint64_t)w->y+7u,w->title,title_chars,c(244,248,255),title);
    text((uint64_t)(w->x+w->width-105),(uint64_t)w->y+7u,"[M] [Z] [X]",c(244,248,255),title);
    draw_window_contents(w);
}
static int point_in_window(const fluxmax_wm_window_t *w, int32_t x, int32_t y) {
    return w && w->visible && !w->minimized && x >= w->x && y >= w->y &&
           x < w->x + w->width && y < w->y + w->height;
}

static int mouse_window_at(int32_t x, int32_t y) {
    if (wm.active_index >= 0 && wm.active_index < FLUXMAX_WM_MAX_WINDOWS &&
        point_in_window(&wm.windows[wm.active_index], x, y)) return wm.active_index;
    /* Non-focused windows are painted in ascending array order, so the last
     * matching index is the topmost of them. */
    for (int i = FLUXMAX_WM_MAX_WINDOWS - 1; i >= 0; --i) {
        if (i == wm.active_index) continue;
        if (point_in_window(&wm.windows[i], x, y)) return i;
    }
    return -1;
}

static int32_t dock_button_width(void) {
    int32_t available = (int32_t)screen_w - 20 - 18;
    int32_t width = available > 0 ? available / 4 : 1;
    return width > 82 ? 82 : width;
}

static int dock_button_at(int32_t x) {
    int32_t width = dock_button_width();
    for (int i = 0; i < 4; ++i) {
        int32_t left = 10 + i * (width + 6);
        if (x >= left && x < left + width) return i;
    }
    return -1;
}

static void launch_taskbar_item(int32_t x) {
    int item = dock_button_at(x);
    url_entry = 0;
    if (item == 0) (void)fluxmax_wm_show(&wm, FLUXMAX_WM_SYSTEM);
    else if (item == 1) (void)fluxmax_wm_show(&wm, FLUXMAX_WM_FILES);
    else if (item == 2) (void)fluxmax_wm_show(&wm, FLUXMAX_WM_NETWORK);
    else if (item == 3) (void)fluxmax_wm_show(&wm, FLUXMAX_WM_BROWSER);
}

static int desktop_icon_at(int32_t x, int32_t y) {
    if (y >= 70 && y < 148) {
        if (x >= 16 && x < 94) return FLUXMAX_WM_SYSTEM;
        if (x >= 102 && x < 180) return FLUXMAX_WM_FILES;
    }
    if (y >= 160 && y < 238) {
        if (x >= 16 && x < 94) return FLUXMAX_WM_NETWORK;
        if (x >= 102 && x < 180) return FLUXMAX_WM_BROWSER;
    }
    return -1;
}

static void draw_desktop_icons(void) {
    static const char *labels[4] = {"SYSTEM", "FILES", "NETWORK", "WEB"};
    static const unsigned colors[4][3] = {
        {46, 126, 192}, {46, 155, 115}, {187, 126, 50}, {116, 93, 190}
    };
    static const int32_t xs[4] = {16, 102, 16, 102};
    static const int32_t ys[4] = {70, 70, 160, 160};
    for (int i = 0; i < 4; ++i) {
        int32_t x = xs[i], y = ys[i];
        rect((uint64_t)x, (uint64_t)y, 78, 76, 15, 25, 43);
        rect((uint64_t)(x + 13), (uint64_t)(y + 5), 52, 43,
             colors[i][0], colors[i][1], colors[i][2]);
        text((uint64_t)(x + 33), (uint64_t)(y + 21),
             i == 0 ? "S" : (i == 1 ? "F" : (i == 2 ? "N" : "W")),
             c(255,255,255), c(colors[i][0], colors[i][1], colors[i][2]));
        clipped_text((uint64_t)(x + 4), (uint64_t)(y + 56), labels[i], 9,
                     c(223,234,248), c(15,25,43));
    }
}

static void draw_wallpaper_accents(void) {
    /* Quiet diagonal light trails provide a desktop wallpaper rather than a
       solid console-like background; keep them behind windows and icons. */
    if (screen_w < 400 || screen_h < 300) return;
    for (int i = 0; i < 17; ++i) {
        uint64_t y = 62u + (uint64_t)i * 27u;
        if (y + 2u >= screen_h - WM_TASKBAR) break;
        uint64_t x = screen_w / 2u + ((uint64_t)i * 19u) % (screen_w / 5u);
        if (x < screen_w) rect(x, y, screen_w - x, 2u,
                               13u + (unsigned)(i % 3) * 2u,
                               28u + (unsigned)(i % 4) * 2u,
                               49u + (unsigned)(i % 5) * 3u);
    }
}

static int mouse_left_click(void) {
    if (mouse_y >= (int32_t)screen_h - (int32_t)WM_TASKBAR) {
        int item = dock_button_at(mouse_x);
        launch_taskbar_item(mouse_x);
        dragging_window = -1;
        return item >= 0;
    }
    int index = mouse_window_at(mouse_x, mouse_y);
    if (index < 0) {
        int icon = desktop_icon_at(mouse_x, mouse_y);
        if (icon >= 0) {
            url_entry = 0;
            (void)fluxmax_wm_show(&wm, (fluxmax_wm_window_id_t)icon);
            dragging_window = -1;
            return 1;
        }
        dragging_window = -1;
        return 0;
    }
    fluxmax_wm_window_t *w = &wm.windows[index];
    if (w->id != FLUXMAX_WM_BROWSER) url_entry = 0;
    (void)fluxmax_wm_focus(&wm, w->id);
    int32_t rel_x = mouse_x - w->x;
    int32_t rel_y = mouse_y - w->y;
    if (rel_y < (int32_t)(WM_TOPBAR - 10u)) {
        /* These hit boxes match the [M] [Z] [X] controls drawn in the title. */
        if (rel_x >= w->width - 105 && rel_x < w->width - 80) {
            dragging_window = -1;
            (void)fluxmax_wm_toggle_minimize(&wm);
        } else if (rel_x >= w->width - 73 && rel_x < w->width - 48) {
            dragging_window = -1;
            (void)fluxmax_wm_toggle_maximize(&wm);
        } else if (rel_x >= w->width - 41 && rel_x < w->width - 16) {
            dragging_window = -1;
            (void)fluxmax_wm_close(&wm, w->id);
        } else {
            dragging_window = index;
        }
    } else {
        dragging_window = -1;
    }
    return 1;
}

static int32_t clamp_mouse(int32_t value, int32_t minimum, int32_t maximum) {
    if (maximum < minimum) maximum = minimum;
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static void erase_mouse_cursor(void) {
    if (!mouse_cursor_drawn) return;
    for (size_t row = 0; row < 14u; ++row) {
        for (size_t col = 0; col < 9u; ++col) {
            if (!mouse_cursor_under_valid[row][col]) continue;
            (void)fluxmax_display_write_pixel((uint64_t)(mouse_cursor_saved_x + (int32_t)col),
                (uint64_t)(mouse_cursor_saved_y + (int32_t)row), mouse_cursor_under[row][col]);
        }
    }
    mouse_cursor_drawn = 0;
}

static void draw_mouse_cursor(void) {
    if (!ps2_mouse_is_available()) return;
    static const char *shape[] = {
        "B........", "BB.......", "BWB......", "BWWB.....", "BW.WB....",
        "BW..WB...", "BW...WB..", "BW....WB.", "BW.....WB", "BWBBWWWWW",
        "BWBW.....", "BBW......", "B..WB....", "....BB..."
    };
    mouse_cursor_saved_x = mouse_x;
    mouse_cursor_saved_y = mouse_y;
    for (size_t row = 0; row < 14u; ++row) {
        for (size_t col = 0; col < 9u; ++col) {
            uint64_t x = (uint64_t)(mouse_x + (int32_t)col);
            uint64_t y = (uint64_t)(mouse_y + (int32_t)row);
            mouse_cursor_under_valid[row][col] = (uint8_t)fluxmax_display_read_pixel(
                x, y, &mouse_cursor_under[row][col]);
            if (shape[row][col] == 'B')
                (void)fluxmax_display_write_pixel(x, y, c(0,0,0));
            else if (shape[row][col] == 'W')
                (void)fluxmax_display_write_pixel(x, y, c(245,249,255));
        }
    }
    mouse_cursor_drawn = 1;
}

static void process_mouse_events(void) {
    if (!ps2_mouse_is_available()) return;
    int repaint = 0;
    int cursor_moved = 0;
    fluxmax_mouse_event_t event;
    while (ps2_mouse_read_event(&event)) {
        if (!mouse_smoke_marker_sent) {
            debugcon_write("FLUXMAX:MOUSE_EVENT_SEEN\n");
            mouse_smoke_marker_sent = 1;
        }
        int32_t old_x = mouse_x, old_y = mouse_y;
        mouse_x = clamp_mouse(mouse_x + event.dx, 0, (int32_t)screen_w - 10);
        mouse_y = clamp_mouse(mouse_y + event.dy, 0, (int32_t)screen_h - 15);
        int left_down = (event.buttons & 0x01u) != 0;
        int left_was_down = (previous_mouse_buttons & 0x01u) != 0;
        if (left_down && left_was_down && dragging_window >= 0 &&
            wm.active_index == dragging_window && (event.dx || event.dy)) {
            if (fluxmax_wm_move_active(&wm, event.dx, event.dy) == 0) repaint = 1;
        }
        if (left_down && !left_was_down) {
            if (mouse_left_click()) repaint = 1;
        }
        if (!left_down) dragging_window = -1;
        if (mouse_x != old_x || mouse_y != old_y) cursor_moved = 1;
        previous_mouse_buttons = event.buttons;
    }
    /* Pointer motion alone must never repaint the entire desktop. Restoring
       the saved pixels under the old cursor avoids the visible blank/flash. */
    if (repaint) draw_desktop();
    else if (cursor_moved) { erase_mouse_cursor(); draw_mouse_cursor(); }
}

static void draw_desktop(void) {
    if(!screen_w || !screen_h) return;
    erase_mouse_cursor();
    int buffered = fluxmax_display_frame_begin();
    rect(0,0,screen_w,screen_h,10,16,28);
    draw_wallpaper_accents();
    rect(0,0,screen_w,WM_TOPBAR,20,31,49);
    text(14,12,"FLUXMAX OS  |  DESKTOP",c(81,201,255),c(20,31,49));
    if (screen_w > 560)
        clipped_text(screen_w-205u,12,"EXPERIMENTAL WORKSPACE",24,c(179,198,222),c(20,31,49));
    draw_desktop_icons();
    /* Paint background windows first, and the focused window last. */
    for(int i=0;i<FLUXMAX_WM_MAX_WINDOWS;++i) if(i!=wm.active_index) draw_one_window(&wm.windows[i],0);
    if(wm.active_index>=0 && wm.active_index<FLUXMAX_WM_MAX_WINDOWS) draw_one_window(&wm.windows[wm.active_index],1);
    uint64_t by=screen_h>WM_TASKBAR?screen_h-WM_TASKBAR:0;
    rect(0,by,screen_w,WM_TASKBAR,20,31,49);
    rect(0,by,screen_w,2,44,99,153);
    static const char *apps[4] = {"SYSTEM", "FILES", "NETWORK", "WEB"};
    static const unsigned app_colors[4][3] = {
        {46,126,192}, {46,155,115}, {187,126,50}, {116,93,190}
    };
    int32_t bw = dock_button_width();
    for (int i = 0; i < 4; ++i) {
        int32_t bx = 10 + i * (bw + 6);
        int active = wm.active_index == i && wm.windows[i].visible && !wm.windows[i].minimized;
        rect((uint64_t)bx,by+5u,(uint64_t)bw,WM_TASKBAR-10u,
             active ? 37u : 27u, active ? 57u : 42u, active ? 82u : 61u);
        rect((uint64_t)(bx+5),by+11u,8u,8u,app_colors[i][0],app_colors[i][1],app_colors[i][2]);
        clipped_text((uint64_t)(bx+17),by+13u,apps[i],(size_t)(bw>21?((bw-20)/8):4),
                     c(230,239,251),c(active?37:27,active?57:42,active?82:61));
    }
    if (screen_w > 500)
        clipped_text((uint64_t)(10 + 4 * (bw + 6) + 12),by+13u,
                     "HJKL MOVE  B WEB  R REDRAW  Q EXIT",
                     (size_t)((screen_w > (uint64_t)(10 + 4 * (bw + 6) + 12)) ?
                     (screen_w - (uint64_t)(10 + 4 * (bw + 6) + 12)) / 8u : 0u),
                     c(141,199,255),c(20,31,49));
    if (buffered) fluxmax_display_frame_present();
    /* Cursor is an overlay with a small save-under buffer; it never causes a
       complete desktop redraw when only the pointer position changes. */
    mouse_cursor_drawn = 0;
    draw_mouse_cursor();
}

static int read_key(char *key) {
    int got=ps2_keyboard_read_char(key);
    if (got && !keyboard_smoke_marker_sent) {
        debugcon_write("FLUXMAX:KEYBOARD_EVENT_SEEN\n");
        keyboard_smoke_marker_sent = 1;
    }
    if(!got) got=serial_try_read_char(key);
    return got;
}
static void start_url_entry(void) {
    url_entry=1; browser_url_len=0; browser_url[0]='\0';
    browser_capture_len=0; browser_capture[0]='\0'; browser_scroll=0;
    (void)fluxmax_wm_show(&wm,FLUXMAX_WM_BROWSER);
}
static void fetch_browser_url(void) {
    url_entry=0; browser_capture_len=0; browser_capture[0]='\0'; browser_scroll=0;
    browser_set_output(browser_capture_char,0);
    (void)browser_open_url(browser_url);
    browser_set_output(0,0);
    draw_desktop();
}
void fluxmax_desktop_set_active(int active) { desktop_foreground = active ? 1 : 0; }
int fluxmax_desktop_is_active(void) { return desktop_foreground != 0; }

int desktop_init(void) {
    fluxmax_display_size(&screen_w,&screen_h);
    if(screen_w<320 || screen_h<220 || !g_framebuffer || g_framebuffer->bpp!=32 || g_framebuffer->memory_model!=LIMINE_FRAMEBUFFER_RGB) return -1;
    if(!wm_initialized || wm.screen_width!=(int32_t)screen_w || wm.screen_height!=(int32_t)screen_h) {
        if(fluxmax_wm_init(&wm,(int32_t)screen_w,(int32_t)screen_h)!=0) return -1;
        wm_initialized=1; url_entry=0; browser_capture_len=0; browser_capture[0]='\0'; browser_url[0]='\0';
    }
    if (!mouse_position_initialized) {
        mouse_x = (int32_t)(screen_w / 2u); mouse_y = (int32_t)(screen_h / 2u);
        previous_mouse_buttons = 0; dragging_window = -1; mouse_position_initialized = 1;
    }
    return 0;
}
void desktop_run(void) {
    if(desktop_init()!=0) {
        fluxmax_desktop_set_active(0);
        fluxmax_display_console_set_enabled(1);
        serial_write("desktop: requires a 32-bit RGB framebuffer at least 320x220; serial shell remains available\n");
        return;
    }
    fluxmax_desktop_set_active(1);
    fluxmax_display_console_set_enabled(0); /* Serial logs must not paint over the desktop framebuffer. */
    draw_desktop();
    serial_write("Fluxmax window manager started. See desktop bottom bar for keyboard shortcuts.\n");
    for(;;) {
        process_mouse_events();
        char key=0; if(!read_key(&key)) { cpu_hlt(); continue; }
        if(url_entry) {
            if(key==27) { url_entry=0; draw_desktop(); continue; }
            if(key=='\r') continue;
            if(key=='\n') { browser_url[browser_url_len]='\0'; fetch_browser_url(); continue; }
            if(key=='\b') { if(browser_url_len) browser_url[--browser_url_len]='\0'; draw_desktop(); continue; }
            if((unsigned char)key>=32u && (unsigned char)key<127u && browser_url_len+1u<sizeof(browser_url)) { browser_url[browser_url_len++]=key; browser_url[browser_url_len]='\0'; draw_desktop(); }
            continue;
        }
        if(key=='q'||key=='Q'||key==27) break;
        if(key=='\t'||key=='n'||key=='N') { (void)fluxmax_wm_cycle(&wm,1); }
        else if(key=='p'||key=='P') { (void)fluxmax_wm_cycle(&wm,-1); }
        else if(key=='1') (void)fluxmax_wm_show(&wm,FLUXMAX_WM_SYSTEM);
        else if(key=='2') (void)fluxmax_wm_show(&wm,FLUXMAX_WM_FILES);
        else if(key=='3') (void)fluxmax_wm_show(&wm,FLUXMAX_WM_NETWORK);
        else if(key=='4') (void)fluxmax_wm_show(&wm,FLUXMAX_WM_BROWSER);
        else if(key=='b'||key=='B') start_url_entry();
        else if(key=='h'||key=='H') (void)fluxmax_wm_move_active(&wm,-16,0);
        else if(key=='l'||key=='L') (void)fluxmax_wm_move_active(&wm,16,0);
        else if(key=='k'||key=='K') {
            if(fluxmax_wm_active(&wm) && fluxmax_wm_active(&wm)->id==FLUXMAX_WM_BROWSER && !url_entry) { if(browser_scroll) --browser_scroll; }
            else (void)fluxmax_wm_move_active(&wm,0,-16);
        }
        else if(key=='j'||key=='J') {
            if(fluxmax_wm_active(&wm) && fluxmax_wm_active(&wm)->id==FLUXMAX_WM_BROWSER && !url_entry) ++browser_scroll;
            else (void)fluxmax_wm_move_active(&wm,0,16);
        }
        else if(key=='+') (void)fluxmax_wm_resize_active(&wm,24,16);
        else if(key=='-') (void)fluxmax_wm_resize_active(&wm,-24,-16);
        else if(key=='m'||key=='M') (void)fluxmax_wm_toggle_minimize(&wm);
        else if(key=='z'||key=='Z') (void)fluxmax_wm_toggle_maximize(&wm);
        else if(key=='x'||key=='X') { const fluxmax_wm_window_t *w=fluxmax_wm_active(&wm); if(w) (void)fluxmax_wm_close(&wm,w->id); }
        else if(key=='r'||key=='R') { wm_initialized=0; (void)desktop_init(); draw_desktop(); continue; }
        else continue;
        draw_desktop();
    }
    cpu_cli();
    fluxmax_desktop_set_active(0);
    fluxmax_display_console_set_enabled(1);
    fluxmax_display_console_init();
    cpu_sti();
    serial_write("desktop: returned to shell\n");
}
