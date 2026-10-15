# Fluxmax v23 — status

Fluxmax is an independent custom x86-64 kernel with an experimental framebuffer desktop. This revision standardizes branding to Fluxmax and documents the engineering steps required to run a GNOME-like interface and eventually GNOME over the custom kernel.

## Confirmed in this authoring environment
- The v22 source baseline contains 152 host assertions in the existing test suite.
- v23 adds a branding check and updates the ISO target name to `build/Fluxmax-x86_64.iso`.
- The codebase must be rebuilt and host tests rerun after the namespace migration; see `VALIDATION-v23-AR.txt` for measured results.

## Not claimed
- A bootable ISO created in this environment.
- A successful QEMU run or visually verified mouse/keyboard behavior.
- GNOME, Wayland/Mutter, POSIX compatibility, Wi-Fi or TLS/HTTPS support.

The exact GNOME port is a substantial compatibility project and cannot be achieved by a name or theme change alone. See `GNOME-PORT-ROADMAP_AR.md`.
