#include "task.h"
#include "types.h"
#include "paging.h"
#include "pmm.h"

extern void tss_set_kernel_stack(uint32_t esp0);

static task_t   task_table[TASK_MAX];
static uint32_t next_id = 0;

task_t *current_task = (task_t *)0;

void task_init(void) {
    uint32_t i;
    for (i = 0; i < TASK_MAX; i++) {
        task_table[i].id        = 0;
        task_table[i].esp       = 0;
        task_table[i].stack_top = 0;
        task_table[i].state     = TASK_BLOCKED;
        task_table[i].next      = (task_t *)0;
    }
}

task_t *task_adopt_current(void) {
    task_t *t    = &task_table[0];
    t->id        = next_id++;
    t->state     = TASK_READY;
    t->stack_top = 0x00180000;
    t->esp       = 0;
    t->next      = t;
    t->owner     = (struct proc *)0;
    current_task = t;
    return t;
}

task_t *task_create(void (*entry)(void)) {
    uint32_t i;
    task_t *t = (task_t *)0;

    for (i = 1; i < TASK_MAX; i++) {
        if (task_table[i].id == 0 &&
            task_table[i].state == TASK_BLOCKED) {
            t = &task_table[i];
            break;
        }
    }

    if (!t)
        return (task_t *)0;

    uint32_t id      = next_id++;
    uint32_t stk_top = TASK_STACK_BASE + (id * TASK_STACK_SIZE);

    /*
     * Allocate + map kernel stack
     */
    uint32_t stack_phys = pmm_alloc_frame();
    if (!stack_phys)
        return (task_t *)0;

    page_map(stk_top - PAGE_SIZE,
             stack_phys,
             PAGE_PRESENT | PAGE_WRITE);

    uint32_t *sp = (uint32_t *)stk_top;

    /*
     * task_switch restore path:
     *
     *   popa
     *   popf
     *   ret
     *
     * stack layout:
     *
     *   edi
     *   esi
     *   ebp
     *   esp dummy
     *   ebx
     *   edx
     *   ecx
     *   eax
     *   eflags
     *   ret/eip
     */

    /* ret target */
    *--sp = (uint32_t)entry;

    /* eflags */
    *--sp = 0x0202;

    /* popa restore order */
    *--sp = 0; /* eax */
    *--sp = 0; /* ecx */
    *--sp = 0; /* edx */
    *--sp = 0; /* ebx */
    *--sp = 0; /* esp dummy */
    *--sp = 0; /* ebp */
    *--sp = 0; /* esi */
    *--sp = 0; /* edi */

    t->id        = id;
    t->stack_top = stk_top;
    t->esp       = (uint32_t)sp;
    t->state     = TASK_READY;
    t->owner     = (struct proc *)0;

    if (current_task) {
        t->next            = current_task->next;
        current_task->next = t;
    } else {
        t->next = t;
    }

    return t;
}

task_t *task_create_user(uint32_t user_entry,
                         uint32_t user_stack_top) {
    uint32_t i;
    task_t *t = (task_t *)0;

    for (i = 1; i < TASK_MAX; i++) {
        if (task_table[i].id == 0 &&
            task_table[i].state == TASK_BLOCKED) {
            t = &task_table[i];
            break;
        }
    }

    if (!t)
        return (task_t *)0;

    uint32_t id      = next_id++;
    uint32_t stk_top = TASK_STACK_BASE + (id * TASK_STACK_SIZE);

    /*
     * Allocate + map kernel stack
     */

    uint32_t *sp = (uint32_t *)stk_top;

    t->user_entry     = user_entry;
    t->user_stack_top = user_stack_top;

    extern void ring3_launcher(void);

    /*
     * task_switch restore path:
     *
     *   popa
     *   popf
     *   ret
     *
     * stack layout:
     *
     *   edi
     *   esi
     *   ebp
     *   esp dummy
     *   ebx
     *   edx
     *   ecx
     *   eax
     *   eflags
     *   ret/eip
     */

    /* ret target */
    *--sp = (uint32_t)ring3_launcher;

    /* eflags */
    *--sp = 0x0202;

    /* popa restore order */
    *--sp = 0; /* eax */
    *--sp = 0; /* ecx */
    *--sp = 0; /* edx */
    *--sp = 0; /* ebx */
    *--sp = 0; /* esp dummy */
    *--sp = 0; /* ebp */
    *--sp = 0; /* esi */
    *--sp = 0; /* edi */

    t->id        = id;
    t->stack_top = stk_top;
    t->esp       = (uint32_t)sp;
    t->state     = TASK_READY;
    t->owner     = (struct proc *)0;

    if (current_task) {
        t->next            = current_task->next;
        current_task->next = t;
    } else {
        t->next = t;
    }

    return t;
}
/* ── Phase 25: blocking helpers ──────────────────────────────────────────── */

/*
 * task_block — mark task as blocked so the scheduler skips it.
 * Caller is responsible for calling scheduler_run() afterward
 * if blocking the current task.
 */
void task_block(task_t *t) {
    if (!t) return;
    t->state = TASK_BLOCKED;
}

/*
 * task_wake — make a blocked task runnable again.
 * Safe to call from IRQ context (timer, keyboard).
 * No-op if already TASK_READY.
 */
void task_wake(task_t *t) {
    if (!t) return;
    if (t->state == TASK_BLOCKED)
        t->state = TASK_READY;
}

/*
 * task_clone — create a child task whose saved kernel stack is a copy
 * of the parent's current kernel stack at the moment fork() was called.
 *
 * child_kernel_esp is the ESP value pointing into the PARENT's kernel
 * stack at the syscall frame.  We allocate a fresh kernel stack for the
 * child, copy the frame, and adjust the child's saved esp accordingly.
 *
 * When the scheduler eventually switches to the child, it will execute
 * the same "return from syscall_dispatch" path, but eax will have been
 * set to 0 by sys_fork before cloning, so the child sees fork()==0.
 */
task_t *task_clone(task_t *parent, uint32_t child_kernel_esp) {
    uint32_t i;
    task_t *t = (task_t *)0;

    for (i = 1; i < TASK_MAX; i++) {
        if (task_table[i].id == 0 && task_table[i].state == TASK_BLOCKED) {
            t = &task_table[i];
            break;
        }
    }
    if (!t) return (task_t *)0;

    uint32_t id      = next_id++;
    uint32_t stk_top = TASK_STACK_BASE + (id * TASK_STACK_SIZE);

    /*
     * Copy everything from child_kernel_esp up to parent->stack_top
     * into the equivalent position in the child's kernel stack.
     */
    uint32_t frame_size = parent->stack_top - child_kernel_esp;
    uint32_t child_esp  = stk_top - frame_size;

    uint8_t *src = (uint8_t *)child_kernel_esp;
    uint8_t *dst = (uint8_t *)child_esp;
    for (i = 0; i < frame_size; i++)
        dst[i] = src[i];

    t->id             = id;
    t->stack_top      = stk_top;
    t->esp            = child_esp;
    t->state          = TASK_READY;
    t->owner          = (struct proc *)0;   /* set by proc_fork */
    t->user_entry     = parent->user_entry;
    t->user_stack_top = parent->user_stack_top;

    tss_set_kernel_stack(stk_top);

    if (current_task) {
        t->next            = current_task->next;
        current_task->next = t;
    } else {
        t->next = t;
    }

    return t;
}