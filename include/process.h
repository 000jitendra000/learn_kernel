#ifndef PROCESS_H
#define PROCESS_H

#include "types.h"
#include "task.h"
#include "fs.h"
#include "pipe.h"

#define PROC_MAX           16
#define PROC_FD_MAX        16
#define PROC_FD_FIRST_USER  3

#define FD_TYPE_NONE  0
#define FD_TYPE_FILE  1
#define FD_TYPE_PIPE  2

typedef struct {
    uint32_t type;
    union {
        file_handle_t *file;
        pipe_handle_t *pipe;
    };
} fd_entry_t;

typedef enum {
    PROC_ALIVE = 0,
    PROC_DEAD  = 1
} proc_state_t;

typedef struct proc {
    uint32_t     pid;
    uint32_t     ppid;
    task_t      *task;
    proc_state_t state;
    int32_t      exit_code;
    uint32_t    *page_dir;
    fd_entry_t   fd_table[PROC_FD_MAX];
    uint32_t     pending_signals;
    uint32_t     sleep_until;
    task_t      *waiter;
} proc_t;

void     proc_init(void);
proc_t  *proc_create(void (*entry)(void), uint32_t ppid);
proc_t  *proc_current(void);
uint32_t proc_getpid(void);
void     proc_exit(int32_t code);

proc_t  *proc_create_user(uint32_t user_entry,
                           uint32_t user_stack_top,
                           uint32_t *page_dir,
                           uint32_t  ppid);

int     proc_alloc_fd(proc_t *p, fd_entry_t entry);
proc_t *proc_find(uint32_t pid);
proc_t *proc_get_slot(uint32_t index);
void    proc_close_fd(proc_t *p, int fd);

void proc_signal(proc_t *p, uint32_t sig);
void proc_check_signals(void);

/* Phase 26 */
proc_t *proc_fork(proc_t *parent, uint32_t child_kernel_esp);

#endif