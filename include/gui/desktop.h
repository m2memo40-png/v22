#ifndef FLUXMAX_DESKTOP_H
#define FLUXMAX_DESKTOP_H
/* Framebuffer desktop with keyboard controls and optional PS/2 mouse input. */
int desktop_init(void);
void desktop_run(void);
void fluxmax_desktop_set_active(int active);
int fluxmax_desktop_is_active(void);
#endif
