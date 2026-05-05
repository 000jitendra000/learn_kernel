#include "task.h"
#include "types.h"

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
    t->esp       = 0;       /* filled on first switch-out */
    t->next      = t;
    current_task = t;
    return t;
}

/*
 * Build synthetic stack frame matching switch.asm RESTORE order exactly:
 *
 *   switch.asm does on restore:
 *     popa      → pops edi esi ebp esp_ ebx edx ecx eax  (8 dwords)
 *     popf      → pops eflags
 *     ret       → pops eip
 *
 *   So memory at t->esp when first switched-in must be (low→high):
 *     [esp+0]  = edi
 *     [esp+4]  = esi
 *     [esp+8]  = ebp
 *     [esp+12] = esp_dummy
 *     [esp+16] = ebx
 *     [esp+20] = edx
 *     [esp+24] = ecx
 *     [esp+28] = eax
 *     [esp+32] = eflags
 *     [esp+36] = eip   ← ret target = task entry point
 *
 *   Build by pushing in reverse (eip first = deepest):
 */
task_t *task_create(void (*entry)(void)) {
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
    uint32_t *sp     = (uint32_t *)stk_top;

    /* push in reverse restore order — eip deepest */
    *--sp = (uint32_t)entry;    /* eip  — ret pops this */
    *--sp = 0x0202;             /* eflags — IF=1 */
    /* pusha registers — popa restores edi..eax */
    *--sp = 0;                  /* eax */
    *--sp = 0;                  /* ecx */
    *--sp = 0;                  /* edx */
    *--sp = 0;                  /* ebx */
    *--sp = stk_top;            /* esp dummy — popa discards */
    *--sp = 0;                  /* ebp */
    *--sp = 0;                  /* esi */
    *--sp = 0;                  /* edi  ← esp points here */

    t->id        = id;
    t->stack_top = stk_top;
    t->esp       = (uint32_t)sp;
    t->state     = TASK_READY;

    if (current_task) {
        t->next            = current_task->next;
        current_task->next = t;
    } else {
        t->next = t;
    }

    return t;
}