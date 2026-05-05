#ifndef PROCESS_H
#define PROCESS_H

#include "types.h"
#include "task.h"

#define PROC_MAX    16

typedef enum {
    PROC_ALIVE = 0,
    PROC_DEAD  = 1
} proc_state_t;

typedef struct proc {
    uint32_t     pid;
    uint32_t     ppid;
    task_t      *task;          /* owning task */
    proc_state_t state;
    int32_t      exit_code;
} proc_t;

void     proc_init(void);
proc_t  *proc_create(void (*entry)(void), uint32_t ppid);
proc_t  *proc_current(void);    /* O(1) via current_task->owner */
uint32_t proc_getpid(void);
void     proc_exit(int32_t code);

#endif