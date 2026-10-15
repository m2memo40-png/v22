#include "sched/scheduler.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/io.h"
#include "arch/x86_64/gdt.h"
#include "mm/heap.h"
#include "lib/mem.h"
#include "kernel/types.h"
#include "kernel/log.h"
#include <stddef.h>

#define TASK_STACK_BYTES (16u * 1024u)
#define USER_CODE_SELECTOR 0x23u
#define USER_DATA_SELECTOR 0x1Bu
#define KERNEL_CODE_SELECTOR 0x08u
#define KERNEL_RFLAGS_INITIAL 0x202u
static fluxmax_task_t tasks[FLUXMAX_SCHED_MAX_TASKS];
static uint32_t task_count, cursor;
static uint64_t next_id = 1;
static fluxmax_task_t *current_task;
static struct isr_frame *bootstrap_frame;
static uint64_t bootstrap_cr3;
static int scheduler_started;

static uint64_t cpu_read_cr3(void) {
    uint64_t value; __asm__ volatile("mov %%cr3,%0" : "=r"(value)); return value & 0x000FFFFFFFFFF000ULL;
}
static void cpu_write_cr3(uint64_t value) {
    __asm__ volatile("mov %0,%%cr3" : : "r"(value & 0x000FFFFFFFFFF000ULL) : "memory");
}
static int find_task(uint64_t id) {
    for (uint32_t i=0;i<FLUXMAX_SCHED_MAX_TASKS;++i)
        if (tasks[i].state != FLUXMAX_TASK_UNUSED && tasks[i].id == id) return (int)i;
    return -1;
}
int fluxmax_sched_register(uint64_t id, uint32_t priority, void *context) {
    if (!id || find_task(id)>=0) return -1;
    for (uint32_t i=0;i<FLUXMAX_SCHED_MAX_TASKS;++i) if (tasks[i].state==FLUXMAX_TASK_UNUSED) {
        memset(&tasks[i],0,sizeof(tasks[i])); tasks[i].id=id;
        tasks[i].priority=priority>31u?31u:priority; tasks[i].quantum=1u+tasks[i].priority;
        tasks[i].state=FLUXMAX_TASK_READY; tasks[i].context=context; tasks[i].cr3=cpu_read_cr3(); ++task_count;
        return 0;
    }
    return -1;
}
int fluxmax_sched_remove(uint64_t id) {
    int ix=find_task(id); if(ix<0 || &tasks[ix]==current_task) return -1;
    fluxmax_task_t *t=&tasks[ix];
    if (t->state != FLUXMAX_TASK_ZOMBIE) return -1;
    if(t->stack_base) kfree(t->stack_base);
    memset(t,0,sizeof(*t)); if(task_count) --task_count; return 0;
}
int fluxmax_sched_sleep(uint64_t id,uint64_t now,uint64_t ticks) {
    int ix=find_task(id); if(ix<0 || ticks>UINT64_MAX-now) return -1;
    tasks[ix].wake_tick=now+ticks; tasks[ix].state=FLUXMAX_TASK_SLEEPING; return 0;
}
void fluxmax_sched_wake_due(uint64_t now) {
    for(uint32_t i=0;i<FLUXMAX_SCHED_MAX_TASKS;++i)
        if(tasks[i].state==FLUXMAX_TASK_SLEEPING && tasks[i].wake_tick<=now) {
            tasks[i].wake_tick=0; tasks[i].state=FLUXMAX_TASK_READY;
        }
}
fluxmax_task_t *fluxmax_sched_next(void) {
    for(uint32_t tries=0;tries<FLUXMAX_SCHED_MAX_TASKS;++tries) {
        cursor=(cursor+1u)%FLUXMAX_SCHED_MAX_TASKS;
        if(tasks[cursor].state==FLUXMAX_TASK_READY && tasks[cursor].saved_frame) { tasks[cursor].state=FLUXMAX_TASK_RUNNING; return &tasks[cursor]; }
    }
    if(current_task && current_task->state==FLUXMAX_TASK_READY && current_task->saved_frame) {
        current_task->state=FLUXMAX_TASK_RUNNING; return current_task;
    }
    return 0;
}
uint32_t fluxmax_sched_count(void) { return task_count; }
const fluxmax_task_t *fluxmax_sched_current(void) { return current_task; }
void fluxmax_sched_terminate_current(void) {
    if (current_task) current_task->state = FLUXMAX_TASK_ZOMBIE;
}
int fluxmax_sched_current_is_zombie(void) {
    return current_task && current_task->state == FLUXMAX_TASK_ZOMBIE;
}

static NORETURN void task_returned_unexpectedly(void) {
    cpu_cli();
    debugcon_write("FLUXMAX:TASK_RETURNED_UNEXPECTEDLY\n");
    for (;;) cpu_hlt();
}

static NORETURN void task_trampoline(fluxmax_task_t *task) {
    /* New kernel tasks start with IF=0. Do not permit a timer IRQ to preempt
       the synthetic IRET frame before this C function has established its
       own stack frame. The initial-frame builder therefore leaves IF clear;
       this is the first safe point at which to enable interrupts. */
    debugcon_write("FLUXMAX:TASK_START\n");
    cpu_sti();
    if (task && task->entry) task->entry(task->argument);
    if (task) task->state=FLUXMAX_TASK_ZOMBIE;
    for (;;) cpu_hlt();
}
static uint64_t reserve_task(void *context, uint32_t priority, fluxmax_task_t **task_out) {
    uint64_t id=next_id++;
    if(!id) id=next_id++;
    if(fluxmax_sched_register(id,priority,context)!=0) return 0;
    int ix=find_task(id); if(ix<0) return 0;
    *task_out=&tasks[ix];
    return id;
}
static void *alloc_task_stack(fluxmax_task_t *task) {
    void *stack=kmalloc(TASK_STACK_BYTES);
    if(!stack) { memset(task,0,sizeof(*task)); if(task_count) --task_count; return 0; }
    task->stack_base=stack; task->stack_size=TASK_STACK_BYTES;
    uintptr_t top=((uintptr_t)stack+TASK_STACK_BYTES)&~(uintptr_t)0xFULL;
    task->kernel_stack_top=(uint64_t)top;
    return stack;
}
uint64_t fluxmax_sched_create_kernel_thread(void (*entry)(void *),void *argument,uint32_t priority) {
    if(!entry) return 0;
    fluxmax_task_t *t=0;
    if(!reserve_task(argument,priority,&t)) return 0;
    if(!alloc_task_stack(t)) return 0;
    t->entry=entry; t->argument=argument; t->cr3=cpu_read_cr3();
    uintptr_t top=(uintptr_t)t->kernel_stack_top;
    /* Long-mode IRETQ always restores RIP/CS/RFLAGS/RSP/SS. Keep the full
       synthetic frame below a separate return-address slot at top-8. */
    struct isr_frame *frame=(struct isr_frame *)(top-sizeof(struct isr_frame)-sizeof(uint64_t));
    memset(frame,0,sizeof(*frame));
    frame->rdi=(uint64_t)(uintptr_t)t;
    frame->rip=(uint64_t)(uintptr_t)task_trampoline;
    frame->cs=KERNEL_CODE_SELECTOR;
    frame->rflags=0x002u; /* Enable IF only after task_trampoline builds its C frame. */
    frame->rsp=(uint64_t)(top-sizeof(uint64_t));
    frame->ss=0x10u;
    *(uint64_t *)(uintptr_t)frame->rsp=(uint64_t)(uintptr_t)task_returned_unexpectedly;
    t->saved_frame=frame;
    return t->id;
}
uint64_t fluxmax_sched_create_user_thread(void *context, uint64_t address_space_root,
                                         uint64_t entry, uint64_t user_stack, uint32_t priority) {
    if (!context || !address_space_root || entry < 0x1000u || user_stack < 0x1000u ||
        entry >= 0x0000800000000000ULL || user_stack >= 0x0000800000000000ULL) return 0;
    fluxmax_task_t *t=0;
    if(!reserve_task(context,priority,&t)) return 0;
    if(!alloc_task_stack(t)) return 0;
    t->cr3=address_space_root;
    uintptr_t top=(uintptr_t)t->kernel_stack_top;
    struct isr_frame *frame=(struct isr_frame *)(top-sizeof(struct isr_frame));
    memset(frame,0,sizeof(*frame));
    frame->rip=entry;
    frame->cs=USER_CODE_SELECTOR;
    frame->rflags=KERNEL_RFLAGS_INITIAL;
    frame->rsp=user_stack;
    frame->ss=USER_DATA_SELECTOR;
    t->saved_frame=frame;
    return t->id;
}

struct isr_frame *fluxmax_sched_on_timer(struct isr_frame *frame,uint64_t ticks) {
    if(!scheduler_started) {
        bootstrap_frame=frame; bootstrap_cr3=cpu_read_cr3(); scheduler_started=1;
    }
    if(current_task) {
        current_task->saved_frame=frame;
        current_task->cr3=cpu_read_cr3();
        if(current_task->state==FLUXMAX_TASK_RUNNING) current_task->state=FLUXMAX_TASK_READY;
    } else {
        bootstrap_frame=frame;
        bootstrap_cr3=cpu_read_cr3();
    }
    fluxmax_sched_wake_due(ticks);
    fluxmax_task_t *next=fluxmax_sched_next();
    if(!next) {
        current_task=0;
        if (bootstrap_cr3) cpu_write_cr3(bootstrap_cr3);
        return bootstrap_frame ? bootstrap_frame : frame;
    }
    current_task=next;
    if (next->cr3) cpu_write_cr3(next->cr3);
    tss_set_rsp0(next->kernel_stack_top);
    return (struct isr_frame *)next->saved_frame;
}
