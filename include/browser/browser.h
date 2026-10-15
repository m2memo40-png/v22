#ifndef FLUXMAX_BROWSER_H
#define FLUXMAX_BROWSER_H
#include <stddef.h>
/* Text HTML renderer plus a bounded plain-HTTP client. No TLS, JS, CSS layout, or images. */
typedef void (*fluxmax_browser_putc_fn)(char value, void *context);
typedef int (*fluxmax_browser_url_handler_fn)(const char *url);
int browser_init(void);
void browser_set_url_handler(fluxmax_browser_url_handler_fn handler);
/* Optional output sink; NULL restores the framebuffer/serial console output. */
void browser_set_output(fluxmax_browser_putc_fn output, void *context);
int browser_open_url(const char *url); /* HTTP only until TLS and certificate validation exist. */
int browser_launch_ui(void);
int fluxmax_browser_render_html_text(const char *html, size_t length,
                                    fluxmax_browser_putc_fn putc_fn, void *context);
#endif
