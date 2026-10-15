# Fluxmax architecture snapshot — custom kernel source v23

This project keeps the custom freestanding x86-64 kernel as its runtime. It does not use a Linux distribution or prebuilt desktop. Limine is an external bootloader; GCC/binutils build the kernel.

## Kernel, devices and user code
The tree contains x86-64 entry/interrupt code, physical/virtual-memory helpers, kernel heap, a single-CPU scheduler/process records, a static ELF64 loader, Ring-3/syscall code, COM1 and a Limine framebuffer console. Peripheral support includes PCI config access/enumeration, PS/2 keyboard, legacy ATA PIO and RTL8139; these are a limited device subset rather than broad modern hardware support.

## Storage/network/browser
RAM VFS is separate from bounded FAT32 root file operations on ATA PIO. Networking supports RTL8139, Ethernet/ARP/IPv4/ICMP/UDP, DHCP/DNS and an initial single-connection TCP path. The browser issues bounded plain HTTP GET requests and presents parsed HTML as text. HTTPS/TLS is deliberately unsupported.

## Graphics and window manager
The Limine config requests a generic 32-bit RGB framebuffer and the kernel enters the custom desktop automatically when one is available; this is not a native accelerated GPU driver. `gui/window_manager.c` holds a testable state model for four windows (System, Files, Network, Web). It supports focus/cycling, move and geometry clamping, resize, minimize, maximize/restore, close and reopen. `gui/desktop.c` renders the focused window last and the browser output sink redirects HTTP/HTML output into the Web window. Keyboard controls are described in README; the optional legacy PS/2 mouse path draws a pointer and handles clicks/window dragging.

## Validation status
Locally, `make clean && make -j2 all check user-elf test` passed, with 122 host checks and a successful kernel/user ELF build, including 11 newly added PS/2 mouse packet-decoder tests. The v21 kernel queues the generic framebuffer desktop as a scheduled task when a compatible 32-bit RGB framebuffer is returned; the QEMU smoke workflow requires the desktop startup marker. The TCP/HTTP tests simulate peers; window-manager tests exercise state and bounds but not a live framebuffer. This environment lacks QEMU, xorriso and the Limine binary bundle, so no ISO was built/booted here. The v21 scheduler change addresses the missing desktop-start marker by moving the long-lived desktop loop out of the bootstrap context. Its runtime fix still requires the GitHub Actions QEMU smoke tests. The PS/2 mouse decoder was host-tested, but the hardware IRQ path remains unverified. Do not claim daily-driver completeness until device support, storage integration, hardened user isolation, HTTPS/browser security, power management and real QEMU/hardware validation are complete.


The v21 network layer selects an Intel E1000 adapter first and falls back to RTL8139. GitHub Actions attempts separate QEMU runs with both NIC models, including DHCP, gateway ping, DNS and a local HTTP fixture. This authoring environment does not have QEMU/xorriso, so these runtime checks must be confirmed in Actions.
