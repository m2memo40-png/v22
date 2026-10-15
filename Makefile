CC ?= gcc
LD ?= ld
BUILD := build
CFLAGS := -std=gnu11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-builtin \
          -fno-stack-protector -fno-pic -fno-pie -fno-omit-frame-pointer \
          -fno-asynchronous-unwind-tables -fno-unwind-tables \
          -m64 -mno-red-zone -mcmodel=kernel -mgeneral-regs-only \
          -I. -Iinclude -Iboot -Ilib
ASFLAGS := -m64 -ffreestanding -fno-pic -fno-pie -mno-red-zone -I. -Iinclude -Iboot
LDFLAGS := -nostdlib -z max-page-size=0x1000 -T linker.ld

C_SOURCES := \
    kernel/kmain.c kernel/log.c kernel/panic.c kernel/bootinfo.c \
    kernel/syscall.c kernel/feature_ledger.c kernel/selftest.c kernel/shell.c \
    arch/x86_64/gdt.c arch/x86_64/idt.c arch/x86_64/pic.c arch/x86_64/pit.c \
    mm/pmm.c mm/vmm.c mm/heap.c lib/mem.c lib/unicode.c \
    gui/boot_splash.c gui/console.c gui/compositor.c gui/window_manager.c gui/desktop.c \
    drivers/pci/pci.c drivers/pci/config.c drivers/keyboard/ps2.c drivers/keyboard/ps2_keymap.c drivers/input/ps2_mouse.c drivers/input/ps2_mouse_protocol.c drivers/storage/ata_pio.c drivers/net/rtl8139.c drivers/net/e1000.c \
    fs/vfs.c fs/fat32/fat32_geometry.c fs/fat32/fat32_ro.c fs/fat32/fat32_rw.c \
    sched/scheduler_core.c sched/process.c net/ipv4.c net/stack.c net/dns.c net/tcp.c net/net.c browser/html/html_lexer.c browser/browser.c browser/http_client.c user/elf.c
S_SOURCES := arch/x86_64/entry.S arch/x86_64/gdt_load.S arch/x86_64/isr_stubs.S
USER_HELLO_OBJ := $(BUILD)/userland/hello.o
USER_HELLO_ELF := $(BUILD)/userland/hello.elf
USER_BLOB_OBJ := $(BUILD)/userland/builtin_hello.o
OBJECTS := $(patsubst %.c,$(BUILD)/%.o,$(C_SOURCES)) $(patsubst %.S,$(BUILD)/%.o,$(S_SOURCES)) $(USER_BLOB_OBJ)
ISO := $(BUILD)/Fluxmax-x86_64.iso

HOST_CC ?= cc
HOST_CFLAGS := -std=c11 -O2 -Wall -Wextra -Werror -Iinclude -Iboot

.PHONY: all clean check iso dirs test test-host test-net test-dns test-browser test-http test-tcp test-wm test-mouse test-framebuffer test-keyboard user-elf check-source-tree check-branding
all: $(BUILD)/kernel.elf

# Validate that all required source/header files are present before compilation.
test: check-source-tree check-branding
	@$(MAKE) --no-print-directory test-host test-net test-dns test-browser test-http test-tcp test-wm test-mouse test-framebuffer test-keyboard

check-source-tree:
	@bash scripts/check-source-files.sh

check-branding:
	@bash scripts/check-branding.sh

user-elf: $(USER_HELLO_ELF)
	@file $(USER_HELLO_ELF)
	@readelf -h $(USER_HELLO_ELF) | grep -E 'Class:|Machine:|Type:|Entry point'

test-host: $(BUILD)/host-unit-tests $(BUILD)/host-fat32-rw-tests
	@./$(BUILD)/host-unit-tests
	@./$(BUILD)/host-fat32-rw-tests

test-net: $(BUILD)/host-net-stack-tests
	@./$(BUILD)/host-net-stack-tests

test-dns: $(BUILD)/host-dns-tests
	@./$(BUILD)/host-dns-tests


test-browser: $(BUILD)/host-browser-tests
	@./$(BUILD)/host-browser-tests

test-http: $(BUILD)/host-http-client-tests
	@./$(BUILD)/host-http-client-tests

test-tcp: $(BUILD)/host-tcp-tests
	@./$(BUILD)/host-tcp-tests

test-wm: $(BUILD)/host-window-manager-tests
	@./$(BUILD)/host-window-manager-tests

test-mouse: $(BUILD)/host-ps2-mouse-tests
	@./$(BUILD)/host-ps2-mouse-tests

test-framebuffer: $(BUILD)/host-console-frame-tests
	@./$(BUILD)/host-console-frame-tests

test-keyboard: $(BUILD)/host-ps2-keymap-tests
	@./$(BUILD)/host-ps2-keymap-tests

$(BUILD)/host-ps2-mouse-tests: tests/host_ps2_mouse_tests.c drivers/input/ps2_mouse_protocol.c
	@mkdir -p $(BUILD)
	$(HOST_CC) $(HOST_CFLAGS) $^ -o $@

$(BUILD)/host-window-manager-tests: tests/host_window_manager_tests.c gui/window_manager.c
	@mkdir -p $(BUILD)
	$(HOST_CC) $(HOST_CFLAGS) $^ -o $@

$(BUILD)/host-http-client-tests: tests/host_http_client_tests.c browser/http_client.c browser/browser.c browser/html/html_lexer.c net/ipv4.c lib/mem.c
	@mkdir -p $(BUILD)
	$(HOST_CC) $(HOST_CFLAGS) -fno-builtin $^ -o $@

$(BUILD)/host-tcp-tests: tests/host_tcp_tests.c net/tcp.c net/ipv4.c lib/mem.c
	@mkdir -p $(BUILD)
	$(HOST_CC) $(HOST_CFLAGS) -fno-builtin $^ -o $@

$(BUILD)/host-browser-tests: tests/host_browser_tests.c browser/browser.c browser/html/html_lexer.c
	@mkdir -p $(BUILD)
	$(HOST_CC) $(HOST_CFLAGS) $^ -o $@

$(BUILD)/host-dns-tests: tests/host_dns_tests.c net/dns.c lib/mem.c
	@mkdir -p $(BUILD)
	$(HOST_CC) $(HOST_CFLAGS) -fno-builtin $^ -o $@

$(BUILD)/host-net-stack-tests: tests/host_net_stack_tests.c net/stack.c net/ipv4.c lib/mem.c
	@mkdir -p $(BUILD)
	$(HOST_CC) $(HOST_CFLAGS) -fno-builtin $^ -o $@

$(BUILD)/host-unit-tests: tests/host_unit_tests.c net/ipv4.c fs/fat32/fat32_geometry.c fs/vfs.c
	@mkdir -p $(BUILD)
	$(HOST_CC) $(HOST_CFLAGS) $^ -o $@

$(BUILD)/host-fat32-rw-tests: tests/host_fat32_rw_tests.c fs/fat32/fat32_geometry.c fs/fat32/fat32_ro.c fs/fat32/fat32_rw.c
	@mkdir -p $(BUILD)
	$(HOST_CC) $(HOST_CFLAGS) $^ -o $@

dirs:
	@mkdir -p $(sort $(dir $(OBJECTS)))

$(BUILD)/%.o: %.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S | dirs
	$(CC) $(ASFLAGS) -c $< -o $@

$(USER_HELLO_OBJ): userland/apps/hello.S | dirs
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(USER_HELLO_ELF): $(USER_HELLO_OBJ) userland/linker.ld
	@mkdir -p $(dir $@)
	$(LD) -nostdlib -static -z max-page-size=0x1000 -T userland/linker.ld -o $@ $(USER_HELLO_OBJ)

$(USER_BLOB_OBJ): userland/builtin_hello.S $(USER_HELLO_ELF)
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(OBJECTS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

check: all
	@file $(BUILD)/kernel.elf
	@readelf -h $(BUILD)/kernel.elf | grep -E 'Class:|Machine:|Type:'

iso: all
	@test -n "$(LIMINE_DIR)" || (echo 'Set LIMINE_DIR to an extracted official Limine binary release.' >&2; exit 2)
	@LIMINE_DIR="$(LIMINE_DIR)" ISO_OUT="$(ISO)" bash ./scripts/build-iso.sh

clean:
	rm -rf $(BUILD)

$(BUILD)/host-console-frame-tests: tests/host_console_frame_tests.c gui/console.c lib/mem.c
	@mkdir -p $(BUILD)
	$(HOST_CC) $(HOST_CFLAGS) -fno-builtin $^ -o $@

$(BUILD)/host-ps2-keymap-tests: tests/host_ps2_keymap_tests.c drivers/keyboard/ps2_keymap.c
	@mkdir -p $(BUILD)
	$(HOST_CC) $(HOST_CFLAGS) $^ -o $@
