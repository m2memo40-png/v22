# Fluxmax — custom x86-64 kernel and desktop (development v23)

**Fluxmax** is an independent operating-system project intended to keep its own freestanding x86-64 kernel. It boots through the external Limine bootloader and is built with GCC/binutils; it does not use a Linux kernel as its runtime. The ISO output is `build/Fluxmax-x86_64.iso` and the shell prompt is `fluxmax>`.

**The current GUI is Fluxmax Desktop, not the GNOME desktop environment.** v23 standardizes the product and internal source namespace and adds a strict branding check. The existing desktop still uses a generic framebuffer, a small custom window manager, and experimental PS/2 input. Host tests exercise state models and input parsers, but only QEMU/real-device tests can prove end-to-end input and graphics behavior.

The long-term target is to run a GNOME-like/then GNOME-compatible user interface above the **Fluxmax kernel**, not by replacing the kernel with Linux. GNOME is not runnable on this kernel today: the current kernel exposes only five basic syscalls, lacks a POSIX-compatible userland, dynamic loader and required service/graphics APIs. See [GNOME-PORT-ROADMAP_AR.md](GNOME-PORT-ROADMAP_AR.md) for prerequisites and pass/fail gates. QEMU and xorriso are unavailable in the current authoring environment, so this source archive is not a newly built or runtime-tested ISO.

## v21 scheduling correction (historical)

The prior QEMU log reached `FLUXMAX:SHELL_READY` and accepted DHCP, but timed out before either the desktop startup marker or shell prompt. The kernel launched the desktop loop on the bootstrap context, which the current scheduler does not requeue while worker tasks remain runnable. v21 queues the desktop as a kernel task and leaves bootstrap in an idle loop. The source/kernel build and host tests pass locally; QEMU/xorriso are unavailable here, so this diagnosis-driven fix still needs a passing GitHub Actions QEMU run.

## Source implementations

- x86-64 kernel entry, GDT/TSS, IDT/exception path, PIC/PIT, serial logging, physical-page allocation, kernel heap, virtual-memory helpers and a single-CPU preemptive kernel-thread scheduler.
- Initial user address spaces/mappings, static ELF64 `ET_EXEC` loader, Ring-3 demonstration and `int 0x80` syscall path (`exit`, `write`, `getpid`, `yield`, `uptime`). End-to-end isolation and interrupt-return safety still need runtime testing and hardening.
- RAM VFS. Legacy ATA PIO sector read/write/flush and bounded FAT32 root directory helpers for short 8.3 filenames. FAT32 is not a fully mounted persistent VFS, and long names/subdirectories are not fully supported.
- PCI config/enumeration, PS/2 keyboard and optional legacy PS/2 mouse (IRQ12), Limine-provided 32-bit RGB framebuffer renderer, rectangle/surface compositor and a custom window manager. Four windows are defined (System, Files, Network and Web); keyboard controls and the mouse pointer support focus, taskbar launch, title-bar actions and dragging windows. USB HID mice are not implemented.
- Polling-mode Intel E1000/legacy 8254x and RTL8139 Ethernet drivers; Ethernet II, ARP, IPv4 (no fragment reassembly), ICMP echo, checked UDP, DHCP, DNS A queries and ping. The shell command `internet` checks gateway ICMP and DNS reachability.
- Initial single-outbound-connection TCP state machine with bounded retransmit and in-order receive ring; not a general-purpose sockets implementation.
- Built-in HTML-as-text web view: `browser /path.html` previews a local VFS document; `browser http://host/path` fetches plain HTTP via DNS/TCP; desktop `b` opens a URL prompt in the Web window. Browser output is captured into that window, not a full modern graphical page renderer.

## Window-manager controls

The desktop opens automatically when a compatible framebuffer is available. The `internet` shell command tests ICMP reachability to the configured gateway and DNS resolution for `example.com`; it does not test HTTPS. Press `q` to return to the kernel shell; run `desktop` or `web` to reopen it. `1` System, `2` Files, `3` Network, `4` Web; `Tab` or `n` next window, `p` previous; `h/j/k/l` move the focused window; `+/-` resize; `m` minimize; `z` maximize/restore; `x` close; `b` enter an HTTP URL in the Web window; `j/k` scroll the Web output while Web is focused; `r` reset desktop; `q` return to shell. A PS/2-compatible mouse is supported; its event path is now injected by the QEMU smoke test. USB HID/Bluetooth mice are not supported. The framebuffer is a generic bootloader-provided display mode, not an accelerated Intel/AMD/NVIDIA driver.

## Browser and networking limitations

The current custom web view is not a modern daily-use browser. No HTTPS/TLS or certificate validation. Do not enter credentials or sensitive data. No JavaScript, CSS layout, images, cookies/authentication, automatic redirects, complete DOM, or broad HTTP compatibility. TCP is single-connection and lacks a general socket API, mature congestion control and robust recovery. Only legacy Intel 8254x/E1000 and RTL8139-compatible wired Ethernet paths are supported by these drivers; no Wi-Fi, modern e1000e, USB or VirtIO network driver is included.

## GitHub Actions header-path fix (v9)

The previous Actions log failed with `fatal error: net/tcp.h: No such file or directory`. The required header lives at the repository-root-relative path `include/net/tcp.h`; `-Iinclude` resolves `#include "net/tcp.h"` only when that file is actually committed in the checkout. v9 contains this header and adds `scripts/check-source-files.sh`, which validates critical files before compilation and prints a direct diagnostic instead of a long compiler error. If the preflight fails in GitHub, the branch/commit being built does not contain the entire source tree.

## GitHub Actions ISO build fix (v9)

The latest build log showed that the kernel linked successfully, but `make iso` failed with `Required Limine bundle file not found: limine`. The Limine binary bundle can contain the host utility source without a prebuilt `limine` executable. The ISO build script and workflow now run `make` in the extracted bundle directory when the utility is missing, then verify that the executable exists before calling `limine bios-install`. This fixes the missing host-tool step; it does not by itself prove that a real ISO boots. See `FIX-GITHUB-LIMINE-TOOL-AR.md`.


## QEMU smoke-test synchronization fix (v10)

The v9 smoke log reported `QEMU boot smoke test did not report the expected 14/14 kernel self-test result (qemu exit=124)`. The exit code 124 is the timeout wrapper terminating QEMU; by itself it does not distinguish a kernel boot hang from commands being sent before the shell was ready. The v9 smoke script used fixed sleeps and piped commands blindly. v10 replaces that with `scripts/qemu-smoke-driver.py`, which connects QEMU COM1 through a raw pseudo-terminal, waits for the actual `fluxmax> ` shell prompt, sends one command at a time, and waits for expected output before proceeding. It writes the full guest serial transcript to the specified log and prints that transcript plus the exact failed stage when a timeout occurs. The source preflight now also checks that the driver is present.

The new driver was syntax-checked and exercised against a synthetic QEMU-compatible fixture, including the HTTP smoke path. This validates host-side interaction logic only; it does **not** count as a real QEMU boot. If the real guest still fails, Actions should now display the guest serial output leading up to the missing shell prompt or failing command, which identifies the next kernel/runtime issue.

## Build and host tests

Requirements: GCC, binutils, Python 3 and GNU Make. ISO creation also requires a consistent extracted Limine binary release and `xorriso`; QEMU is required for guest integration tests.

```sh
make clean
make -j2 all check user-elf test
make iso LIMINE_DIR=/path/to/extracted/limine-binary
FLUXMAX_QEMU_NIC=rtl8139 FLUXMAX_HTTP_SMOKE=1 bash scripts/qemu-smoke-test.sh build/Fluxmax-x86_64.iso build/qemu-smoke-rtl8139.log
FLUXMAX_QEMU_NIC=e1000 FLUXMAX_HTTP_SMOKE=1 bash scripts/qemu-smoke-test.sh build/Fluxmax-x86_64.iso build/qemu-smoke-e1000.log
```

`make test` runs **152 host checks**: 19 base checks, 7 FAT32 writer checks, 17 network checks, 10 DNS checks, 12 HTML/browser checks, 14 HTTP client checks, 15 TCP state tests, 18 window-manager state tests, 11 PS/2 mouse decoder checks, 10 framebuffer frame/pixel checks and 19 PS/2 keyboard keymap checks. Host tests do not prove physical DMA, boot, real network interoperability, GUI rendering in a live guest, or total OS correctness.

The GitHub Actions workflow includes ISO/QEMU integration attempts. Check the actual run result before claiming successful boot, Ring-3 execution or live DHCP/ping/DNS/HTTP.

## Explicitly incomplete

AHCI/NVMe/USB/Wi-Fi/audio and USB HID mouse drivers; fully mounted persistent filesystem; robust user isolation; libc and general application ABI; SMP/APIC/ACPI/power management; TLS/HTTPS; modern browser layout/JavaScript/images; general GUI apps; installer, updates/package management, device compatibility and security review. Do not install this as your only daily OS or trust it with important data.


## v12 QEMU boot diagnostics

v11 failed before the guest shell prompt and produced empty guest serial/debugcon logs. Those symptoms do not alone establish whether the failure occurred in BIOS/ISO boot, Limine configuration/loading, or the kernel handoff. v12 enables Limine serial/verbose output, adds an ASCII preview of the VGA screenshot to the Actions log, records QEMU interrupt/reset information, validates `_start` against the ELF entry, and removes the diagnostics Artifact upload step that failed because the repository reached its Actions artifact quota. These changes improve diagnosis; real QEMU boot must still pass before claiming a successful boot.


## v13: BIOS/Limine auto-selection and ISO layout validation

The v12 GitHub log showed repeated legacy BIOS timer interrupts and no serial/debugcon marker from `_start`. This is consistent with control not reaching the kernel entry, but is not enough to prove the exact failure stage. v13 sets `default_entry: 1` and `timeout: 0`, installs the same `limine.conf` at both `/boot/limine/limine.conf` (Limine's first documented search location) and `/limine.conf`, verifies that the copy extracted from the completed ISO matches the source, and prints an El Torito report before attempting QEMU. See `FIX-GITHUB-QEMU-BOOT-AR.md`.

The editing environment used for this archive did not have `xorriso` or `qemu-system-x86_64`, so no actual ISO build or real QEMU boot was run here. The CI run must pass the real boot smoke test before this change can be considered a verified boot fix.

## v14: stronger Limine selection, ELF preflight and boot-failure diagnostics

The v13 log still had no kernel-entry/debugcon marker. It also showed the BIOS timer interrupt trace repeatedly, while the VGA preview was too sparse to identify the visible message. The exact cause is **not yet proven**, so v14 changes the build and diagnostics to avoid another guess:

- `scripts/build-iso.sh` now requires all four Limine boot assets (`limine-bios.sys`, `limine-bios-cd.bin`, `limine-uefi-cd.bin`, `BOOTX64.EFI`) to come from the same directory/release, rather than finding each file independently.
- The same checked configuration is installed at Limine's BIOS/UEFI search locations: `/boot/limine/limine.conf`, `/boot/limine.conf`, `/limine/limine.conf`, `/limine.conf`, and `/EFI/BOOT/limine.conf`.
- `limine.conf` uses a 3-second visible autoboot countdown. This should make a recognized menu/config distinguishable from a loader that did not read the intended configuration.
- `scripts/check-kernel-elf.py` verifies ELF64/x86-64/ET_EXEC, PT_LOAD file/memory sizes and alignments, executable entry coverage, `_start` matching the ELF entry, and an early loaded `.limine_requests` section.
- ISO validation checks that `/boot/kernel.elf` extracted from the finished ISO matches the just-built ELF byte-for-byte, in addition to checking the config and El Torito entry.
- On QEMU failure, the driver asks the monitor for `info registers`, `info block`, `info roms`, `info pci`, and `info bootindex`. The QEMU trace no longer includes the enormous `int` log; it keeps guest errors, CPU resets and unimplemented-instruction diagnostics.
- The GitHub log is the diagnostic surface; this does not add Actions artifacts and avoids the repository's exhausted artifact quota.

Host verification: 104 tests pass and both ELF files build. This environment did not have `xorriso` or `qemu-system-x86_64`, so this is **not** a verified ISO boot fix. The next CI run should show whether the VM is still in BIOS/bootloader code and which CD-ROM/boot files QEMU sees. See `FIX-GITHUB-QEMU-BOOT-AR.md`.


## v15: fix confirmed Limine ELF page-permission conflict

The v14 QEMU BIOS smoke-test reached Limine and selected `boot/kernel.elf`, but Limine printed `Attempted to load ELF file with PHDRs with different permissions sharing the same memory page`. The old ELF placed `.limine_requests` at `0xffffffff8011c030`, immediately after `.rodata`; both PT_LOAD headers therefore touched page `0xffffffff8011c000`, with flags R-- and RW. The kernel entry was not reached, which explains the missing `FLUXMAX:ENTRY` marker and shell prompt.

v15 makes `.limine_requests` start at a 4 KiB boundary, keeping the writable Limine request page separate from the read-only `.rodata` pages. `scripts/check-kernel-elf.py` now checks page-rounded PT_LOAD ranges and fails the build if segments with different permissions share a 4 KiB page. The old linked ELF was rejected by this new check; after the linker adjustment, the rebuilt ELF passed and `readelf -lW` showed the request segment starting at `0xffffffff8011d000`.

Local verification confirms kernel compilation and ELF preflight after the fix. This environment still has no `qemu-system-x86_64` or `xorriso`, so the v15 ISO and real QEMU boot have **not** been tested here. Run the committed GitHub Actions workflow; the next expected proof is the disappearance of the Limine permission panic, followed by `FLUXMAX:ENTRY` and `fluxmax>`. Passing this loader stage alone will not prove all OS features work.


## v16: correct full 64-bit IRET frames and kernel-task startup

The v15 QEMU report proved that Limine reached the kernel and emitted `FLUXMAX:SHELL_READY`, but the guest then raised #GP before the prompt. The task-frame code had treated `iretq` like a same-privilege three-slot return and left the kernel task's saved RSP/SS slots unspecified. In 64-bit mode, `iretq` consumes RIP, CS, RFLAGS, RSP and SS; the synthetic kernel-task frame therefore loaded invalid stack state.

v16 extends `struct isr_frame` with explicit `rsp` and `ss` fields and compile-time offset/size assertions. It initializes both fields for kernel and user task contexts. New kernel tasks begin with interrupts disabled and enable them inside `task_trampoline` after its C stack frame is established. A `FLUXMAX:TASK_START` debugcon marker distinguishes a successful initial context restore from a later failure.

Host tests, kernel build, ELF preflight and Limine configuration checks pass locally. QEMU/xorriso are not installed in this environment and DNS resolution to Debian repositories fails, so the new ISO's runtime behavior still needs the committed GitHub Actions QEMU test.


## v17: TCP pending-state ordering and HTTP failure diagnostics

The v16 smoke log reached the interactive shell and passed self-tests, the Ring-3 sample, DHCP, ping, and DNS, but the browser failed while sending the HTTP request. v17 prepares the pending TCP segment and retry deadline before publishing `pending_valid`, and initializes the retry deadline before the transmit routine. This closes a preemption window where the background network task could observe stale pending state and attempt a premature retransmission.

The browser now prints a stable TCP error code (local TX failure, ACK/retry timeout, peer reset, or caller timeout). The QEMU smoke driver detects the first HTTP error immediately, and the shell harness prints the fixture server log on failure. Local host tests, the kernel build, ELF checks, and Limine-config checks pass. QEMU and xorriso are unavailable in the current build environment, so v17 still requires the GitHub Actions runtime test before claiming the HTTP smoke test is fixed.


## v18: TCP half-close and HTTP smoke regression

The v17 QEMU log showed that the host HTTP fixture received `GET / HTTP/1.0` and logged `200`, yet the guest reported `request send failed`. v18 addresses the transport-state edge case in which a fast peer can ACK the request and return response data together with FIN before the sending code resumes. TCP `CLOSE_WAIT` means the peer has closed its send direction; it does not mean our send direction instantly failed. The sender now accepts `CLOSE_WAIT` while waiting for an outstanding ACK, and a regression test injects ACK+PSH+FIN in one segment to verify successful send, readable buffered response data, EOF, and close.

The QEMU smoke driver no longer matches the beginning of the TCP error message; it waits for the complete diagnostic suffix so the actual error number/reason is present in the Actions log. Local validation passes 111 host checks, the kernel build, ELF and Limine checks, and Python/Bash syntax checks. QEMU and xorriso are not installed in this environment, so the v18 guest HTTP success remains to be confirmed by GitHub Actions.


## v19: automatic framebuffer desktop

v19 changes the Limine configuration from text-mode menu preference to graphics enabled and requests a 1024x768x32 framebuffer for the kernel. The kernel draws its custom desktop automatically when a compatible framebuffer is returned. The background shell now leaves PS/2 keyboard input and framebuffer drawing alone until `q` exits the desktop. `desktop` and `web` both open the desktop from the shell.

The CI smoke test now requires the desktop startup marker, exits it over COM1, and then runs the existing kernel, Ring-3, DHCP, ping, DNS and HTTP fixture checks. Local source/build checks pass, but QEMU and xorriso are unavailable in the editing environment, so no new bootable ISO or live graphics run is claimed.

This milestone uses a generic Limine-provided framebuffer, not a native Intel/AMD/NVIDIA driver. The built-in Web window supports plain HTTP and renders HTML as text only; HTTPS/TLS, JavaScript, CSS layout, images, mouse input and GPU acceleration remain unimplemented. It is not ready to replace a complete daily-use desktop OS.


## v20 network-driver expansion

v20 adds a polling-mode Intel 8254x/E1000 driver and generic NIC selection with RTL8139 fallback. QEMU smoke tests are parameterized for each NIC model and the workflow now runs both against the DHCP/DNS/ping/plain-HTTP fixture. The `net` command reports the actual driver, while `internet` checks ICMP to the configured gateway and DNS resolution for `example.com`. Host-side tests and kernel compilation passed in the current environment; because QEMU/xorriso are not installed here, the live NIC paths still require a passing Actions run.

The generic Limine framebuffer remains a software-rendered display path, not accelerated vendor GPU support. HTTPS/TLS and a modern browser engine remain unavailable, so this revision improves wired network hardware support and diagnostics but still cannot browse most modern HTTPS websites.


## v20 input update (October 2026)

The v20 source now initializes an optional standard three-byte PS/2 mouse, routes IRQ12 through the IDT, validates/synchronizes packets, rejects overflowed movement, and draws a pointer in the framebuffer desktop. Left-click focuses a window, activates taskbar shortcuts and uses title-bar minimize/maximize/close controls; dragging a title bar moves the focused window. The decoder is covered by 11 host tests. Hardware/IRQ behavior still needs QEMU or real-device confirmation. This does not add USB HID or touch input.

The local Linux validation performed for this archive builds the kernel and user ELF and passes 122 host checks. It does not create or boot an ISO because QEMU, xorriso and the Limine binary bundle are unavailable in this environment. Wi-Fi hardware drivers and TLS/HTTPS are still **not** implemented; do not treat this archive as meeting those requirements yet.

## v22 mouse, keyboard and desktop rendering correction

The user reported that moving the mouse made the image flash/disappear, keyboard input was unreliable, and the desktop did not feel like a normal desktop environment. The v21 GUI repainted directly into the video framebuffer on every pointer movement, first clearing the entire display and then repainting multiple windows pixel by pixel. v22 draws the complete frame into a RAM backbuffer and presents it as a frame; mouse-only movement saves/restores the small area under the software pointer and does not trigger a whole-desktop repaint. Fill operations use row writes instead of calling the per-pixel routine for every pixel.

The PS/2 keyboard controller is initialized through its command byte to enable IRQ1 and Set-1 translation; left and right Shift are tracked independently; Caps Lock autorepeat is suppressed; E0 navigation and E1 Pause/Break bytes are ignored safely by the current character API. New host tests cover keymap behavior and framebuffer frame/pixel boundaries. The desktop starts with only System open, adds clickable desktop app tiles and a bottom app dock, and avoids repainting the screen for an empty-area click.

The QEMU smoke driver now injects an HMP mouse move and a PS/2 `sendkey q`, then waits for guest debugcon markers proving that both input paths reached the desktop. These are new integration assertions, not a claim that QEMU already passed: this environment has no `qemu-system-x86_64` or `xorriso`. `make test` passes 152 host checks locally and `make check` links the kernel, but the next GitHub Actions run must prove the GUI starts, mouse events arrive, keyboard input exits to the shell, and both NIC smoke paths pass.
