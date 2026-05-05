#ifndef TASK_H
#define TASK_H

#include "types.h"

#define TASK_STACK_BASE  0x00800000
#define TASK_STACK_SIZE  0x00001000   /* 4 KB per task */
#define TASK_MAX         16

typedef enum {
    TASK_READY   = 0,
    TASK_BLOCKED = 1
} task_state_t;

struct proc;   /* forward declaration — defined in process.h */

typedef struct task {
    uint32_t      esp;          /* saved stack pointer — MUST be first field */
    uint32_t      stack_top;
    uint32_t      id;
    task_state_t  state;
    struct proc  *owner;        /* owning process — set by proc_create() */
    struct task  *next;
} task_t;

void     task_init(void);
task_t  *task_adopt_current(void);
task_t  *task_create(void (*entry)(void));

void     scheduler_init(void);
void     scheduler_request(void);
void     scheduler_run(void);

void task_switch(uint32_t *old_esp, uint32_t new_esp);

#endif