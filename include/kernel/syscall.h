#ifndef FLUXMAX_KERNEL_SYSCALL_H
#define FLUXMAX_KERNEL_SYSCALL_H
#include <stdint.h>
typedef int64_t (*fluxmax_syscall_fn)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
typedef struct { uint32_t number; const char *name; fluxmax_syscall_fn handler; uint8_t argument_count; } fluxmax_syscall_entry_t;
enum {
    FLUXMAX_SYS_EXIT = 0,
    FLUXMAX_SYS_WRITE = 1,
    FLUXMAX_SYS_GETPID = 2,
    FLUXMAX_SYS_YIELD = 3,
    FLUXMAX_SYS_UPTIME = 4
};
int fluxmax_syscall_init(void);
int fluxmax_syscall_register(uint32_t number, const char *name, fluxmax_syscall_fn handler, uint8_t argument_count);
int64_t fluxmax_syscall_dispatch(uint64_t number, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5);
const fluxmax_syscall_entry_t *fluxmax_syscall_lookup(uint32_t number);
#endif
