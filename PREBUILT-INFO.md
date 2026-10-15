# Prebuilt ELF files — Fluxmax v23

- `prebuilt/kernel.elf`: local rebuilt image of the renamed Fluxmax custom kernel. It is a development artifact and still requires a passing guest boot test.
- `prebuilt/hello-user.elf`: static x86-64 Ring-3 demonstration executable rebuilt from the current source.

Local validation is recorded in `VALIDATION-v23-AR.txt`. This environment cannot run QEMU or build a bootable ISO because QEMU, xorriso and the pinned Limine binary bundle are unavailable. Do not infer a successful graphical boot from compilation or host tests.

GNOME, Wayland/Mutter, POSIX userland compatibility, Wi-Fi, TLS/HTTPS, USB HID and native accelerated GPU drivers remain incomplete. See `GNOME-PORT-ROADMAP_AR.md`.
