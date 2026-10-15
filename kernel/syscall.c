#include "kernel/syscall.h"
#include "kernel/errno.h"
#include "kernel/log.h"
#include "sched/process.h"
#include "arch/x86_64/pit.h"
#include "mm/vmm.h"
#include "lib/mem.h"

#define FLUXMAX_SYSCALL_MAX 256u
#define USER_WRITE_MAX 65536u
static fluxmax_syscall_entry_t syscall_table[FLUXMAX_SYSCALL_MAX];
static uint32_t syscall_count;
static int initialized;

const fluxmax_syscall_entry_t *fluxmax_syscall_lookup(uint32_t number) {
    for (uint32_t i = 0; i < syscall_count; ++i)
        if (syscall_table[i].number == number) return &syscall_table[i];
    return 0;
}

int fluxmax_syscall_register(uint32_t number, const char *name, fluxmax_syscall_fn handler, uint8_t argument_count) {
    if (!name || !handler || argument_count > 6 || syscall_count >= FLUXMAX_SYSCALL_MAX) return -1;
    if (fluxmax_syscall_lookup(number)) return -1;
    syscall_table[syscall_count++] = (fluxmax_syscall_entry_t){number, name, handler, argument_count};
    return 0;
}

static int64_t sys_exit(uint64_t status, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    process_t *p = process_current();
    if (!p) return -T_EPERM;
    process_exit((int)status);
    return 0;
}
static int64_t sys_write(uint64_t fd, uint64_t buffer, uint64_t length,
                         uint64_t a3, uint64_t a4, uint64_t a5) {
    (void)a3; (void)a4; (void)a5;
    process_t *p = process_current();
    if (!p || !p->address_space) return -T_EPERM;
    if (fd != 1u && fd != 2u) return -T_EINVAL;
    if (length > USER_WRITE_MAX) return -T_EINVAL;
    if (!length) return 0;
    char chunk[128];
    uint64_t done = 0;
    while (done < length) {
        size_t n = (size_t)(length - done);
        if (n > sizeof(chunk)) n = sizeof(chunk);
        if (buffer > UINT64_MAX - done || vmm_space_copy_from(p->address_space, chunk, buffer + done, n) != 0)
            return done ? (int64_t)done : -T_EFAULT;
        for (size_t i = 0; i < n; ++i) {
            if (chunk[i] == '\n') serial_write("\n");
            else serial_write_char(chunk[i]);
        }
        done += n;
    }
    return (int64_t)done;
}
static int64_t sys_getpid(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    process_t *p = process_current(); return p ? (int64_t)p->pid : -T_EPERM;
}
static int64_t sys_yield(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    /* Returning from int 0x80 goes through the scheduler, providing a yield. */
    return 0;
}
static int64_t sys_uptime(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return (int64_t)pit_ticks();
}
int fluxmax_syscall_init(void) {
    if (initialized) return 0;
    if (fluxmax_syscall_register(FLUXMAX_SYS_EXIT, "exit", sys_exit, 1) != 0 ||
        fluxmax_syscall_register(FLUXMAX_SYS_WRITE, "write", sys_write, 3) != 0 ||
        fluxmax_syscall_register(FLUXMAX_SYS_GETPID, "getpid", sys_getpid, 0) != 0 ||
        fluxmax_syscall_register(FLUXMAX_SYS_YIELD, "yield", sys_yield, 0) != 0 ||
        fluxmax_syscall_register(FLUXMAX_SYS_UPTIME, "uptime", sys_uptime, 0) != 0) {
        log_write(LOG_ERROR, "syscall", "Could not register default syscall ABI");
        return -1;
    }
    initialized = 1;
    log_write(LOG_INFO, "syscall", "Installed ring-3 int 0x80 ABI: exit/write/getpid/yield/uptime");
    return 0;
}
int64_t fluxmax_syscall_dispatch(uint64_t number, uint64_t a0, uint64_t a1, uint64_t a2,
                                uint64_t a3, uint64_t a4, uint64_t a5) {
    if (number > UINT32_MAX) return -T_ENOSYS;
    const fluxmax_syscall_entry_t *entry = fluxmax_syscall_lookup((uint32_t)number);
    if (!entry) return -T_ENOSYS;
    return entry->handler(a0, a1, a2, a3, a4, a5);
}
