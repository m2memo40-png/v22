#ifndef FLUXMAX_SCHEDULER_H
#define FLUXMAX_SCHEDULER_H
#include <stdint.h>
#include <stddef.h>
#define FLUXMAX_SCHED_MAX_TASKS 256u
typedef enum { FLUXMAX_TASK_UNUSED, FLUXMAX_TASK_READY, FLUXMAX_TASK_RUNNING, FLUXMAX_TASK_SLEEPING, FLUXMAX_TASK_ZOMBIE } fluxmax_task_state_t;
typedef struct fluxmax_task {
    uint64_t id;
    uint32_t priority;
    uint32_t quantum;
    uint64_t wake_tick;
    fluxmax_task_state_t state;
    void *context;
    void *stack_base;
    size_t stack_size;
    void (*entry)(void *);
    void *argument;
    void *saved_frame;
    uint64_t cr3;
    uint64_t kernel_stack_top;
} fluxmax_task_t;
int fluxmax_sched_register(uint64_t id, uint32_t priority, void *context);
int fluxmax_sched_remove(uint64_t id);
int fluxmax_sched_sleep(uint64_t id, uint64_t now, uint64_t ticks);
void fluxmax_sched_wake_due(uint64_t now);
fluxmax_task_t *fluxmax_sched_next(void);
uint32_t fluxmax_sched_count(void);
uint64_t fluxmax_sched_create_kernel_thread(void (*entry)(void *), void *argument, uint32_t priority);
uint64_t fluxmax_sched_create_user_thread(void *context, uint64_t address_space_root,
                                         uint64_t entry, uint64_t user_stack, uint32_t priority);
void fluxmax_sched_terminate_current(void);
int fluxmax_sched_current_is_zombie(void);
struct isr_frame;
struct isr_frame *fluxmax_sched_on_timer(struct isr_frame *current_frame, uint64_t ticks);
const fluxmax_task_t *fluxmax_sched_current(void);
#endif
