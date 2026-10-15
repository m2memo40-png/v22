#ifndef FLUXMAX_PROCESS_H
#define FLUXMAX_PROCESS_H
#include <stdint.h>
#include "sched/scheduler.h"
#include "mm/vmm.h"
typedef enum { PROC_NEW, PROC_READY, PROC_RUNNING, PROC_BLOCKED, PROC_ZOMBIE } process_state_t;
typedef struct process {
    uint64_t pid;
    process_state_t state;
    vmm_address_space_t *address_space;
    void *kernel_stack;
    char name[32];
    int exit_status;
    void (*entry)(void *);
    void *argument;
    uint64_t task_id;
    uint64_t user_entry;
    uint64_t user_stack;
} process_t;
process_t *process_create(const char *name);
process_t *process_spawn(const char *name, void (*entry)(void *), void *argument, uint32_t priority);
process_t *process_spawn_user(const char *name, vmm_address_space_t *space,
                              uint64_t entry, uint64_t user_stack, uint32_t priority);
int process_exit(int status);
int process_reap(process_t *process);
process_t *process_current(void);
process_t *process_find(uint64_t pid);
uint32_t process_count(void);
#endif
