# Fluxmax framebuffer desktop (experimental)

The kernel uses the generic 32-bit RGB framebuffer handed over by Limine. The built-in desktop offers System, Files, Network and Web panels; a wallpaper, desktop launch icons, a bottom app dock, movable/resizable windows, and a software PS/2 pointer. Only the System window opens at startup; other apps open from the desktop icons or bottom dock. The shell remains available after pressing `q`, and can reopen the desktop with `desktop` (or `web`).

Controls: `1` System, `2` Files, `3` Network, `4` Web; `Tab`/`n` next window, `p` previous; `h/j/k/l` move, `+/-` resize, `m` minimize, `z` maximize/restore, `x` close, `b` enter a plain-HTTP URL in the Web window, `j/k` scroll Web output, `r` reset, `q` exit to shell. The PS/2 keyboard setup explicitly enables IRQ1 and Set-1 translation; extended navigation keys are ignored by the current text-input API.

**Display and pointer:** the desktop builds complete frames in a RAM backbuffer before copying them to the Limine framebuffer. Pointer-only motion restores the old cursor area and paints the cursor at its new position rather than repainting the whole desktop. This reduces screen flash but is not a replacement for hardware acceleration, vsync, a native GPU driver, or a modern compositor. QEMU input smoke tests are configured to inject mouse movement and keyboard keystrokes and verify that they reach the desktop task.

**Linux compatibility:** this is a small custom kernel GUI, not an XFCE/GNOME/KDE desktop and not a Linux distribution. There is no native GPU acceleration, general GUI toolkit, modern font rasterizer, application packaging system, accessibility stack, or full window-compositor protocol yet.

**Browser support:** the built-in browser fetches plain HTTP and displays HTML as text. It does not implement HTTPS/TLS, certificate validation, JavaScript, CSS layout, images, cookies, or the broad features required by modern websites. This is a development milestone, not yet a daily-use desktop environment.
