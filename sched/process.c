#include "sched/process.h"
#include "lib/mem.h"
#include "kernel/errno.h"
#include <stddef.h>
#define PROCESS_LIMIT 64u
static process_t process_table[PROCESS_LIMIT];
static uint64_t next_pid=1;
static size_t str_len_local(const char *s) { size_t n=0; if(s) while(s[n]) ++n; return n; }
process_t *process_create(const char *name) {
    if(!name || !name[0]) return 0;
    for(uint32_t i=0;i<PROCESS_LIMIT;++i) if(process_table[i].pid==0) {
        process_t *p=&process_table[i]; memset(p,0,sizeof(*p)); p->pid=next_pid++;
        if(!next_pid) next_pid=1;
        size_t n=str_len_local(name); if(n>=sizeof(p->name)) n=sizeof(p->name)-1;
        memcpy(p->name,name,n); p->name[n]='\0'; p->state=PROC_NEW; return p;
    }
    return 0;
}
static void process_thread_entry(void *opaque) {
    process_t *p=(process_t *)opaque; if(!p) return;
    p->state=PROC_RUNNING;
    if(p->entry) p->entry(p->argument);
    if (p->state != PROC_ZOMBIE) { p->exit_status=0; p->state=PROC_ZOMBIE; }
}
process_t *process_spawn(const char *name,void (*entry)(void *),void *argument,uint32_t priority) {
    if(!entry) return 0;
    process_t *p=process_create(name); if(!p) return 0;
    p->entry=entry; p->argument=argument; p->state=PROC_READY;
    p->task_id=fluxmax_sched_create_kernel_thread(process_thread_entry,p,priority);
    if(!p->task_id) { memset(p,0,sizeof(*p)); return 0; }
    return p;
}
process_t *process_spawn_user(const char *name, vmm_address_space_t *space,
                              uint64_t entry, uint64_t user_stack, uint32_t priority) {
    if (!space || !entry || !user_stack) return 0;
    process_t *p=process_create(name); if(!p) return 0;
    p->address_space=space; p->user_entry=entry; p->user_stack=user_stack; p->state=PROC_READY;
    p->task_id=fluxmax_sched_create_user_thread(p,vmm_space_root_phys(space),entry,user_stack,priority);
    if(!p->task_id) { memset(p,0,sizeof(*p)); return 0; }
    return p;
}
int process_exit(int status) {
    process_t *p=process_current();
    if(!p) return -1;
    p->exit_status=status; p->state=PROC_ZOMBIE;
    fluxmax_sched_terminate_current();
    return status;
}
process_t *process_current(void) {
    const fluxmax_task_t *task=fluxmax_sched_current();
    return task ? (process_t *)task->context : 0;
}
process_t *process_find(uint64_t pid) { for(uint32_t i=0;i<PROCESS_LIMIT;++i) if(process_table[i].pid==pid) return &process_table[i]; return 0; }
uint32_t process_count(void) { uint32_t n=0; for(uint32_t i=0;i<PROCESS_LIMIT;++i) if(process_table[i].pid && process_table[i].state!=PROC_ZOMBIE) ++n; return n; }
int process_reap(process_t *p) {
    if (!p || !p->pid || p->state != PROC_ZOMBIE || !p->task_id) return -T_EINVAL;
    if (fluxmax_sched_remove(p->task_id) != 0) return -T_EINVAL;
    memset(p,0,sizeof(*p));
    return 0;
}
