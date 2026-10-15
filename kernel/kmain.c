#include "kernel/types.h"
#include "kernel/log.h"
#include "kernel/panic.h"
#include "kernel/bootinfo.h"
#include "kernel/shell.h"
#include "arch/x86_64/gdt.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/pic.h"
#include "arch/x86_64/pit.h"
#include "arch/x86_64/io.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "mm/heap.h"
#include "fs/vfs.h"
#include "drivers/keyboard.h"
#include "drivers/ps2_mouse.h"
#include "drivers/storage/ata_pio.h"
#include "gui/console.h"
#include "gui/desktop.h"
#include "sched/process.h"
#include "kernel/syscall.h"
#include "user/builtin.h"
#include "net/net.h"
#include "browser/browser.h"
#include "browser/http_client.h"

/* Long-lived foreground work must run as a scheduler task. The bootstrap
   context is only the idle fallback: scheduler_core does not rotate it back
   in while any kernel task remains runnable. */
static void fluxmax_desktop_task(void *argument) {
    (void)argument;
    debugcon_write("FLUXMAX:DESKTOP_TASK_ENTER\n");
    desktop_run();
}

void fluxmax_kernel_main(void) __attribute__((noreturn));
void fluxmax_kernel_main(void) {
    cpu_cli();
    serial_init();
    debugcon_write("FLUXMAX:SERIAL_READY\n");
    log_write(LOG_INFO, "kernel", "Fluxmax kernel entry reached");
    bootinfo_init();
    debugcon_write("FLUXMAX:BOOTINFO_OK\n");
    fluxmax_display_console_init();
    gdt_init();
    idt_init();
    pic_init_masked();
    pmm_init();
    if (vmm_init() != 0) log_write(LOG_WARN, "vmm", "Using Limine page tables; VMM mapper could not attach");
    if (heap_init() != 0) panic("Kernel heap initialization failed");
    debugcon_write("FLUXMAX:MEMORY_OK\n");
    if (vfs_init() != 0) panic("RAM VFS initialization failed");
    debugcon_write("FLUXMAX:VFS_OK\n");
    (void)vfs_mkdir("/etc");
    (void)vfs_mkdir("/home");
    (void)vfs_mkdir("/tmp");
    (void)vfs_mkdir("/bin");
    vfs_node_t *welcome = vfs_create("/etc/issue", VFS_NODE_FILE);
    if (welcome) {
        static const char text[] = "Fluxmax experimental kernel. Type help for shell commands.\n";
        (void)vfs_write(welcome, 0, text, sizeof(text)-1u);
    }
    vfs_node_t *hello_bin = vfs_create("/bin/hello", VFS_NODE_FILE);
    if (hello_bin) {
        size_t hello_size = fluxmax_builtin_hello_elf_size();
        if (hello_size && hello_size <= 1024u * 1024u &&
            vfs_write(hello_bin, 0, fluxmax_builtin_hello_elf_start, hello_size) == (int64_t)hello_size)
            log_write(LOG_INFO, "userland", "Embedded static ELF demo installed at /bin/hello");
        else log_write(LOG_WARN, "userland", "Could not install the embedded ELF demo");
    }
    if (fluxmax_syscall_init() != 0) log_write(LOG_ERROR, "syscall", "User mode will be unavailable");
    fluxmax_ata_init(); /* Legacy IDE/ATA PIO; newer AHCI/NVMe remain unsupported. */
    if (net_init() != 0) log_write(LOG_INFO, "net", "No supported Intel E1000 or Realtek RTL8139 NIC; Ethernet services disabled");
    (void)browser_init();
    fluxmax_browser_http_init();
    if (ps2_keyboard_init() == 0)
        log_write(LOG_INFO, "input", "PS/2 keyboard initialized (IRQ1, Set-1 translation)");
    else
        log_write(LOG_WARN, "input", "PS/2 keyboard controller setup timed out; hardware input may be unavailable");
    if (ps2_mouse_init() == 0) log_write(LOG_INFO, "input", "PS/2 mouse enabled (IRQ12)");
    else log_write(LOG_INFO, "input", "PS/2 mouse unavailable; keyboard desktop remains available");
    pit_init(1000);
    fluxmax_net_status_t net_status;
    fluxmax_net_get_status(&net_status);
    if (net_status.nic_present && !process_spawn("network-poll", fluxmax_net_task, 0, 12))
        log_write(LOG_WARN, "net", "Could not start network polling task");
    process_t *shell = process_spawn("kernel-shell", fluxmax_shell_task, 0, 16);
    if (!shell) log_write(LOG_ERROR, "scheduler", "Could not allocate shell task; entering idle loop");
    else log_write(LOG_INFO, "scheduler", "Preemptive kernel-thread scheduling enabled (single CPU)");
    pic_set_mask(0, 0); /* PIT IRQ0 */
    pic_set_mask(1, 0); /* PS/2 keyboard IRQ1 */
    log_write(LOG_INFO, "kernel", "Boot services initialized; attempting the framebuffer desktop");
    debugcon_write("FLUXMAX:SHELL_READY\n");
    /* Initialize the GUI while interrupts are disabled so the shell task cannot steal PS/2 keys at startup. */
    int desktop_ready = (desktop_init() == 0);
    if (desktop_ready) {
        /* Block the shell from consuming input before its GUI task gets CPU time. */
        fluxmax_desktop_set_active(1);
        if (!process_spawn("desktop", fluxmax_desktop_task, 0, 20u)) {
            fluxmax_desktop_set_active(0);
            log_write(LOG_WARN, "desktop", "Could not schedule desktop task; continuing with the serial/keyboard shell");
        } else {
            debugcon_write("FLUXMAX:DESKTOP_TASK_QUEUED\n");
        }
    } else {
        log_write(LOG_WARN, "desktop", "No supported RGB framebuffer; continuing with the serial/keyboard shell");
    }
    cpu_sti();
    /* Do not run desktop_run() on this bootstrap stack. The current scheduler
       switches from bootstrap into runnable tasks but does not schedule the
       bootstrap frame while any task remains READY, so foreground work belongs
       in a kernel task. This context remains the idle fallback. */
    for (;;) cpu_hlt();
}
