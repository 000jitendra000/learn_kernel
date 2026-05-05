#include "task.h"
#include "types.h"

extern task_t *current_task;

/* deferred scheduling flag */
static volatile uint32_t need_schedule = 0;

void scheduler_init(void) {
    need_schedule = 0;
}

/* called by timer IRQ only */
void scheduler_request(void) {
    need_schedule = 1;
}

/* called later in normal kernel context */
void scheduler_run(void) {
    if (!need_schedule)
        return;

    need_schedule = 0;

    if (!current_task)
        return;

    task_t *next = current_task->next;

    /* skip idle task */
    if (next->id == 0)
        next = next->next;

    /* find next READY task */
    for (uint32_t i = 0; i < TASK_MAX; i++) {
        if (next->id != 0 && next->state == TASK_READY)
            break;
        next = next->next;
    }

    /* IMPORTANT: don't switch to yourself */
    if (next == current_task)
        return;

    task_t *prev = current_task;
    current_task = next;

    task_switch(&prev->esp, next->esp);
}