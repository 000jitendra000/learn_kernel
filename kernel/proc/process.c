#include "types.h"
#include "process.h"
#include "task.h"
#include "fs.h"
#include "pipe.h"
#include "signal.h"
#include "paging.h"   /* Phase 26: paging_clone_cow, paging_free_user_dir */
#include "pmm.h"

extern task_t *current_task;
extern void scheduler_request(void);
extern void scheduler_run(void);

static proc_t   proc_table[PROC_MAX];
static uint32_t next_pid = 1;

/* ── internal helpers ────────────────────────────────────────────────────── */

static void proc_clear_fds(proc_t *p) {
    int i;
    for (i = 0; i < PROC_FD_MAX; i++) {
        p->fd_table[i].type = FD_TYPE_NONE;
        p->fd_table[i].file = (file_handle_t *)0;
    }
}

static proc_t *alloc_proc_slot(void) {
    uint32_t i;
    for (i = 0; i < PROC_MAX; i++)
        if (proc_table[i].state == PROC_DEAD && proc_table[i].pid == 0)
            return &proc_table[i];
    return (proc_t *)0;
}

/* ── init ────────────────────────────────────────────────────────────────── */

void proc_init(void) {
    uint32_t i;
    for (i = 0; i < PROC_MAX; i++) {
        proc_table[i].pid             = 0;
        proc_table[i].ppid            = 0;
        proc_table[i].task            = (task_t *)0;
        proc_table[i].state           = PROC_DEAD;
        proc_table[i].exit_code       = 0;
        proc_table[i].page_dir        = (uint32_t *)0;
        proc_table[i].pending_signals = 0;
        proc_table[i].sleep_until     = 0;
        proc_table[i].waiter          = (task_t *)0;
        proc_clear_fds(&proc_table[i]);
    }
}

/* ── fd table helpers ────────────────────────────────────────────────────── */

int proc_alloc_fd(proc_t *p, fd_entry_t entry) {
    int i;
    for (i = PROC_FD_FIRST_USER; i < PROC_FD_MAX; i++) {
        if (p->fd_table[i].type == FD_TYPE_NONE) {
            p->fd_table[i] = entry;
            return i;
        }
    }
    return -1;
}

void proc_close_fd(proc_t *p, int fd) {
    if (fd < 0 || fd >= PROC_FD_MAX) return;
    fd_entry_t *e = &p->fd_table[fd];
    if (e->type == FD_TYPE_FILE)  fh_free(e->file);
    else if (e->type == FD_TYPE_PIPE) pipe_close_end(e->pipe);
    e->type = FD_TYPE_NONE;
    e->file = (file_handle_t *)0;
}

/* ── process creation ────────────────────────────────────────────────────── */

proc_t *proc_create(void (*entry)(void), uint32_t ppid) {
    proc_t *p = alloc_proc_slot();
    if (!p) return (proc_t *)0;

    task_t *t = task_create(entry);
    if (!t) return (proc_t *)0;

    p->pid             = next_pid++;
    p->ppid            = ppid;
    p->task            = t;
    p->state           = PROC_ALIVE;
    p->exit_code       = 0;
    p->page_dir        = (uint32_t *)0;
    p->pending_signals = 0;
    p->sleep_until     = 0;
    p->waiter          = (task_t *)0;
    proc_clear_fds(p);
    t->owner = p;
    return p;
}

proc_t *proc_create_user(uint32_t entry,
                         uint32_t user_stack_top,
                         uint32_t *page_dir,
                         uint32_t ppid) {

    uint32_t i;
    proc_t *p = (proc_t *)0;

    for (i = 1; i < PROC_MAX; i++) {
        if (proc_table[i].pid == 0) {
            p = &proc_table[i];
            break;
        }
    }

    if (!p)
        return (proc_t *)0;

    task_t *t = task_create_user(entry, user_stack_top);
    if (!t)
        return (proc_t *)0;

    /*
     * IMPORTANT:
     * Child task kernel stack must ALSO exist inside
     * the child page directory.
     *
     * Otherwise first context switch into child CR3
     * causes RET to fetch garbage -> eip=0x2D.
     */

    uint32_t stack_vaddr = t->stack_top - PAGE_SIZE;

    uint32_t stack_phys = pmm_alloc_frame();
    if (!stack_phys)
        return (proc_t *)0;

    /*
     * Map kernel task stack into child address space
     */
    paging_map_page(page_dir,
                    stack_vaddr,
                    stack_phys,
                    PAGE_PRESENT | PAGE_WRITE);

    /*
     * Copy initial fabricated kernel context frame
     * from kernel mapping into child mapping.
     *
     * Physical memory is identity mapped in kernel.
     */

    uint8_t *src = (uint8_t *)stack_vaddr;
    uint8_t *dst = (uint8_t *)stack_phys;

    for (i = 0; i < PAGE_SIZE; i++) {
        dst[i] = src[i];
    }

    p->pid       = next_pid++;
    p->ppid      = ppid;
    p->task      = t;
    p->page_dir  = page_dir;
    p->exit_code = 0;

    t->owner = p;

    return p;
}

/* ── runtime ─────────────────────────────────────────────────────────────── */

proc_t *proc_find(uint32_t pid) {
    uint32_t i;
    if (!pid) return (proc_t *)0;
    for (i = 0; i < PROC_MAX; i++)
        if (proc_table[i].pid == pid)
            return &proc_table[i];
    return (proc_t *)0;
}

proc_t *proc_get_slot(uint32_t index) {
    if (index >= PROC_MAX) return (proc_t *)0;
    return &proc_table[index];
}

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

    int i;
    for (i = PROC_FD_FIRST_USER; i < PROC_FD_MAX; i++)
        proc_close_fd(p, i);

    /* Phase 26: release the process's user address space */
    if (p->page_dir) {
        paging_free_user_dir(p->page_dir);
        p->page_dir = (uint32_t *)0;
    }

    p->state     = PROC_DEAD;
    p->exit_code = code;

    if (p->waiter) {
        task_wake(p->waiter);
        p->waiter = (task_t *)0;
    }

    if (p->task) {
        p->task->state = TASK_BLOCKED;
        p->task->id    = 0;
    }

    scheduler_request();
    scheduler_run();

    for (;;) __asm__ volatile("hlt");
}

/* ── Phase 26: fork ──────────────────────────────────────────────────────── */

/*
 * proc_fork — create a child process as a COW clone of `parent`.
 *
 * child_kernel_esp is the current kernel ESP at the point sys_fork()
 * calls us, used by task_clone() to copy the syscall return frame.
 *
 * Returns the new child proc_t, or NULL on failure.
 * The PARENT's eax is set to child->pid by sys_fork.
 * The CHILD's eax (already on its copied kernel stack) was set to 0
 * by sys_fork before calling proc_fork.
 */
proc_t *proc_fork(proc_t *parent, uint32_t child_kernel_esp) {
    proc_t *child = alloc_proc_slot();
    if (!child) return (proc_t *)0;

    /* 1. clone address space COW */
    uint32_t *child_dir = paging_clone_cow(parent->page_dir);
    if (!child_dir) return (proc_t *)0;

    /* flush parent's TLB — parent PDEs now have read-only COW PTEs */
    paging_switch(parent->page_dir);

    /* 2. clone kernel task (copies syscall frame) */
    task_t *child_task = task_clone(parent->task, child_kernel_esp);
    if (!child_task) {
        paging_free_user_dir(child_dir);
        return (proc_t *)0;
    }

    /* 3. fill child proc slot */
    child->pid             = next_pid++;
    child->ppid            = parent->pid;
    child->task            = child_task;
    child->state           = PROC_ALIVE;
    child->exit_code       = 0;
    child->page_dir        = child_dir;
    child->pending_signals = 0;
    child->sleep_until     = 0;
    child->waiter          = (task_t *)0;

    child_task->owner = child;

    /* 4. duplicate fd table — increment refs on all open handles */
    int i;
    for (i = 0; i < PROC_FD_MAX; i++) {
        child->fd_table[i] = parent->fd_table[i];

        if (parent->fd_table[i].type == FD_TYPE_FILE) {
            /* increment file handle ref so fh_free() on either side
               doesn't pull the rug from the other */
            if (parent->fd_table[i].file)
                parent->fd_table[i].file->file->refs++;
        } else if (parent->fd_table[i].type == FD_TYPE_PIPE) {
            /* increment the appropriate pipe end counter */
            pipe_handle_t *ph = parent->fd_table[i].pipe;
            if (ph && ph->pipe) {
                if (ph->end == PIPE_READ)  ph->pipe->reader_count++;
                if (ph->end == PIPE_WRITE) ph->pipe->writer_count++;
            }
        }
    }

    return child;
}

/* ── signals ─────────────────────────────────────────────────────────────── */

void proc_signal(proc_t *p, uint32_t sig) {
    if (!p) return;
    p->pending_signals |= (1u << sig);
}

void proc_check_signals(void) {
    proc_t *p = proc_current();
    if (!p || !p->pending_signals) return;

    if (p->pending_signals & (1u << SIGKILL)) {
        p->pending_signals &= ~(1u << SIGKILL);
        proc_exit(-SIGKILL);
    }
    if (p->pending_signals & (1u << SIGPIPE)) {
        p->pending_signals &= ~(1u << SIGPIPE);
        proc_exit(-SIGPIPE);
    }
}