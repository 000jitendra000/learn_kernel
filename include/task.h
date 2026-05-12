#ifndef TASK_H
#define TASK_H

#include "types.h"

#define TASK_STACK_BASE  0x00800000
#define TASK_STACK_SIZE  0x00001000
#define TASK_MAX         16

typedef enum {
    TASK_READY   = 0,
    TASK_BLOCKED = 1
} task_state_t;

struct proc;

typedef struct task {
    uint32_t      esp;
    uint32_t      stack_top;
    uint32_t      id;
    task_state_t  state;
    struct proc  *owner;
    struct task  *next;
    uint32_t      user_entry;
    uint32_t      user_stack_top;
} task_t;

void    task_init(void);
task_t *task_adopt_current(void);
task_t *task_create(void (*entry)(void));
task_t *task_create_user(uint32_t user_entry, uint32_t user_stack_top);

/* Phase 25 */
void task_block(task_t *t);
void task_wake(task_t *t);

/* Phase 26 */
task_t *task_clone(task_t *parent, uint32_t child_kernel_esp);

void    scheduler_init(void);
void    scheduler_request(void);
void    scheduler_run(void);
void    task_switch(uint32_t *old_esp, uint32_t new_esp);

#endif