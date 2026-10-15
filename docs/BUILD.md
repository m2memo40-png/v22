# Build and validation

Run:

```sh
make clean
make -j2 all check user-elf test
```

The host suite contains **122 checks**: 19 base checks, 7 FAT32 writer, 17 network, 10 DNS, 11 HTML/browser, 14 HTTP, 15 TCP state machine, 18 window manager and 11 PS/2 mouse packet-decoder tests. Host tests do not prove physical DMA, real boot, live IRQ/mouse behavior, network interoperability or total OS correctness.

The default Limine entry requests a 1024x768x32 mode and enters the lightweight framebuffer desktop when supported. The desktop supports a keyboard plus an optional legacy PS/2 mouse; mouse input is decoded in IRQ12 and handled in the foreground. It does not support USB HID mice. The QEMU smoke driver sends `q` over COM1 to return to the shell before command-based checks.

For ISO construction, provide a consistent extracted official Limine binary release and `xorriso`, then run `make iso LIMINE_DIR=/path/to/limine-binary`. For guest integration, install QEMU and review the actual CI run log. This authoring environment did not have QEMU, xorriso or the Limine bundle; its validation includes the freestanding build and host tests only, not an ISO boot. Wi-Fi, HTTPS/TLS with certificate validation, modern browser layout/JavaScript/images and native accelerated vendor GPU drivers remain unimplemented.
