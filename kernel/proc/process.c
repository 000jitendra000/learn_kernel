#include "types.h"
#include "process.h"
#include "task.h"

extern task_t *current_task;
extern void scheduler_request(void);
extern void scheduler_run(void);

static proc_t   proc_table[PROC_MAX];
static uint32_t next_pid = 1;   /* PID 0 reserved — kernel idle */

void proc_init(void) {
    uint32_t i;
    for (i = 0; i < PROC_MAX; i++) {
        proc_table[i].pid       = 0;
        proc_table[i].ppid      = 0;
        proc_table[i].task      = (task_t *)0;
        proc_table[i].state     = PROC_DEAD;
        proc_table[i].exit_code = 0;
    }
}

proc_t *proc_create(void (*entry)(void), uint32_t ppid) {
    uint32_t i;
    proc_t *p = (proc_t *)0;

    /* find free slot */
    for (i = 0; i < PROC_MAX; i++) {
        if (proc_table[i].state == PROC_DEAD && proc_table[i].pid == 0) {
            p = &proc_table[i];
            break;
        }
    }
    if (!p) return (proc_t *)0;

    /* create the underlying task */
    task_t *t = task_create(entry);
    if (!t) return (proc_t *)0;

    p->pid       = next_pid++;
    p->ppid      = ppid;
    p->task      = t;
    p->state     = PROC_ALIVE;
    p->exit_code = 0;

    /* O(1) reverse lookup — task knows its owner directly */
    t->owner = p;

    return p;
}

/*
 * O(1): current_task->owner set at proc_create time.
 * Kernel idle task (task 0) has owner = NULL — not a process.
 */
proc_t *proc_current(void) {
    if (!current_task) return (proc_t *)0;
    return current_task->owner;
}

uint32_t proc_getpid(void) {
    proc_t *p = proc_current();
    if (!p) return 0;
    return p->pid;
}

void proc_exit(int32_t code) {
    proc_t *p = proc_current();
    if (!p) return;

    p->state     = PROC_DEAD;
    p->exit_code = code;

    /* block the underlying task so scheduler skips it */
    if (p->task)
        p->task->state = TASK_BLOCKED;

    /* yield immediately — this call does not return */
    scheduler_request();
    scheduler_run();

    /* unreachable — but halt in case scheduler returns somehow */
    for (;;) __asm__ volatile("hlt");
}