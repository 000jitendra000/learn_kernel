#include "task.h"
#include "types.h"
#include "process.h"
#include "paging.h"

extern task_t *current_task;
extern void task_switch(uint32_t *old_esp, uint32_t new_esp);

static int scheduler_pending = 0;

void scheduler_init(void) {
    scheduler_pending = 0;
}

void scheduler_request(void) {
    scheduler_pending = 1;
}

/*
 * scheduler_run — cooperative context switch.
 *
 * Walks the circular task list from current_task->next looking for
 * the first TASK_READY task.  If none is found (all blocked), we
 * idle with sti+hlt until an IRQ makes progress, then try again.
 *
 * Phase 25 invariant: TASK_BLOCKED tasks are never switched to.
 */
void scheduler_run(void) {

    if (!scheduler_pending) return;
    scheduler_pending = 0;

    if (!current_task) return;

    task_t *start = current_task;
    task_t *next  = current_task->next;

    /* walk the ring looking for a ready task */
    while (next != start) {
        if (next->state == TASK_READY)
            goto found;
        next = next->next;
    }

    /* next == start: check if start itself is still ready */
    if (start->state == TASK_READY) {
        /* only one ready task — stay on it, no switch needed */
        return;
    }

    /*
     * All tasks are blocked.  Idle until an IRQ wakes someone,
     * then re-enter so the newly-ready task can be scheduled.
     */
    while (1) {
        __asm__ volatile("sti");
        __asm__ volatile("hlt");

        /* After hlt returns (IRQ fired), scan again */
        task_t *scan = current_task->next;
        while (scan != current_task) {
            if (scan->state == TASK_READY) {
                next = scan;
                goto found;
            }
            scan = scan->next;
        }
        /* current_task itself might have been woken by an IRQ */
        if (current_task->state == TASK_READY)
            return;
    }

found:
    if (next == current_task) return;   /* nothing to switch to */

    task_t *prev  = current_task;
    current_task  = next;
    if (current_task->owner && current_task->owner->page_dir)
        paging_switch(current_task->owner->page_dir);

    tss_set_kernel_stack(next->stack_top);
    task_switch(&prev->esp, next->esp);
}