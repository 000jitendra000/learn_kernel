#include "types.h"
#include "syscall.h"
#include "idt.h"
#include "task.h"
#include "process.h"
#include "fs.h"
#include "pipe.h"
#include "kbd_buf.h"
#include "signal.h"
#include "timer.h"
#include "paging.h" 

extern task_t *current_task;
extern void scheduler_request(void);
extern void scheduler_run(void);

typedef uint32_t (*syscall_fn_t)(uint32_t, uint32_t, uint32_t);
static syscall_fn_t syscall_table[SYSCALL_MAX];

extern void vga_puts(const char *s);
extern struct proc *exec_elf(const uint8_t *image, uint32_t ppid);
extern fs_file_t *fs_find(const char *name);

/* ── SYS_WRITE (0) ───────────────────────────────────────────────────────── */
static uint32_t sys_write(uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    (void)arg2; (void)arg3;
    const char *s = (const char *)arg1;
    if (!s || (uint32_t)s < 0x01000000) return (uint32_t)-1;
    vga_puts(s);
    return 0;
}

/* ── SYS_GETPID (1) ──────────────────────────────────────────────────────── */
static uint32_t sys_getpid(uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    (void)arg1; (void)arg2; (void)arg3;
    return proc_getpid();
}

/* ── SYS_YIELD (2) ───────────────────────────────────────────────────────── */
static uint32_t sys_yield(uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    (void)arg1; (void)arg2; (void)arg3;
    scheduler_request();
    scheduler_run();
    return 0;
}

/* ── SYS_EXIT (3) ────────────────────────────────────────────────────────── */
static uint32_t sys_exit(uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    (void)arg2; (void)arg3;
    proc_exit((int32_t)arg1);
    return 0;
}

/* ── SYS_OPEN (4) ────────────────────────────────────────────────────────── */
static uint32_t sys_open(uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    (void)arg2; (void)arg3;
    const char *name = (const char *)arg1;
    if (!name || (uint32_t)name < 0x01000000) return (uint32_t)-1;

    proc_t *p = proc_current();
    if (!p) return (uint32_t)-1;

    fs_file_t *f = fs_create_or_open(name);
    if (!f) return (uint32_t)-1;

    file_handle_t *fh = fh_alloc(f);
    if (!fh) { if (f->refs > 0) f->refs--; return (uint32_t)-1; }

    fd_entry_t entry = { FD_TYPE_FILE, { .file = fh } };
    int fd = proc_alloc_fd(p, entry);
    if (fd < 0) { fh_free(fh); return (uint32_t)-1; }

    return (uint32_t)fd;
}

/* ── SYS_READ (5) ────────────────────────────────────────────────────────── */
static uint32_t sys_read(uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    int      fd  = (int)arg1;
    uint8_t *buf = (uint8_t *)arg2;
    uint32_t len = arg3;

    if (!buf || (uint32_t)buf < 0x01000000) return (uint32_t)-1;

    /* fd=0: stdin — block until a full line is ready */
    if (fd == 0) {

        while (!kbd_line_ready()) {
            __asm__ volatile("sti");
            __asm__ volatile("hlt");
        }

        return kbd_line_read((char *)buf, len);
    }
    if (fd == 1 || fd == 2) return (uint32_t)-1;

    proc_t *p = proc_current();
    if (!p || fd < PROC_FD_FIRST_USER || fd >= PROC_FD_MAX) return (uint32_t)-1;

    fd_entry_t *e = &p->fd_table[fd];

    if (e->type == FD_TYPE_FILE) {
        if (len > FS_MAX_SIZE) len = FS_MAX_SIZE;
        return (uint32_t)fh_read(e->file, buf, len);
    }
    if (e->type == FD_TYPE_PIPE)
        return (uint32_t)pipe_read(e->pipe, (char *)buf, len);

    return (uint32_t)-1;
}

/* ── SYS_FWRITE (6) ──────────────────────────────────────────────────────── */
static uint32_t sys_fwrite(uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    int            fd  = (int)arg1;
    const uint8_t *buf = (const uint8_t *)arg2;
    uint32_t       len = arg3;

    if (!buf || (uint32_t)buf < 0x01000000) return (uint32_t)-1;

    proc_t *p = proc_current();
    if (!p || fd < PROC_FD_FIRST_USER || fd >= PROC_FD_MAX) return (uint32_t)-1;

    fd_entry_t *e = &p->fd_table[fd];

    if (e->type == FD_TYPE_FILE) {
        if (len > FS_MAX_SIZE) len = FS_MAX_SIZE;
        return (uint32_t)fh_write(e->file, buf, len);
    }
    if (e->type == FD_TYPE_PIPE)
        return (uint32_t)pipe_write(e->pipe, (const char *)buf, len);

    return (uint32_t)-1;
}

/* ── SYS_CLOSE (7) ───────────────────────────────────────────────────────── */
static uint32_t sys_close(uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    (void)arg2; (void)arg3;
    int fd = (int)arg1;

    proc_t *p = proc_current();
    if (!p || fd < PROC_FD_FIRST_USER || fd >= PROC_FD_MAX) return (uint32_t)-1;
    if (p->fd_table[fd].type == FD_TYPE_NONE) return (uint32_t)-1;

    proc_close_fd(p, fd);
    return 0;
}

/* ── SYS_PIPE (8) ────────────────────────────────────────────────────────── */
static uint32_t sys_pipe(uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    (void)arg2; (void)arg3;
    int *user_fds = (int *)arg1;

    if (!user_fds || (uint32_t)user_fds < 0x01000000) return (uint32_t)-1;
    if ((uint32_t)(user_fds + 1) < 0x01000000)        return (uint32_t)-1;

    proc_t *p = proc_current();
    if (!p) return (uint32_t)-1;

    pipe_t *pipe = pipe_alloc();
    if (!pipe) return (uint32_t)-1;

    pipe_handle_t *rh = pipe_open_end(pipe, PIPE_READ);
    if (!rh) return (uint32_t)-1;

    pipe_handle_t *wh = pipe_open_end(pipe, PIPE_WRITE);
    if (!wh) { pipe_close_end(rh); return (uint32_t)-1; }

    fd_entry_t re = { FD_TYPE_PIPE, { .pipe = rh } };
    fd_entry_t we = { FD_TYPE_PIPE, { .pipe = wh } };

    int rfd = proc_alloc_fd(p, re);
    if (rfd < 0) { pipe_close_end(rh); pipe_close_end(wh); return (uint32_t)-1; }

    int wfd = proc_alloc_fd(p, we);
    if (wfd < 0) { proc_close_fd(p, rfd); pipe_close_end(wh); return (uint32_t)-1; }

    user_fds[0] = rfd;
    user_fds[1] = wfd;
    return 0;
}

static uint32_t sys_exec(uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    (void)arg2;
    (void)arg3;

    const char *name = (const char *)arg1;

    vga_puts("SYS_EXEC: ");

    if (!name) {
        vga_puts("NULL\n");
        return (uint32_t)-1;
    }

    vga_puts((char *)name);
    vga_puts("\n");

    if ((uint32_t)name < 0x01000000) {
        vga_puts("bad ptr\n");
        return (uint32_t)-1;
    }

    fs_file_t *f = fs_find(name);

    if (!f) {
        vga_puts("fs_find failed\n");
        return (uint32_t)-1;
    }

    vga_puts("fs_find ok\n");

    if (!f->size) {
        vga_puts("size=0\n");
        return (uint32_t)-1;
    }

    vga_puts("exec_elf...\n");

    proc_t *child = exec_elf(f->data, proc_getpid());

    if (!child) {
        vga_puts("exec_elf failed\n");
        return (uint32_t)-1;
    }

    vga_puts("child ok pid=");
    vga_puthex(child->pid);
    vga_puts("\n");

    scheduler_request();

    return child->pid;
}

/* ── SYS_WAIT (10) ───────────────────────────────────────────────────────── */
static uint32_t sys_wait(uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    (void)arg2; (void)arg3;
    uint32_t pid = arg1;

    proc_t *p = proc_find(pid);
    if (!p) return (uint32_t)-1;

    if (p->state != PROC_DEAD) {
        /* Phase 25: block instead of polling */
        p->waiter = current_task;
        task_block(current_task);
        scheduler_request();
        scheduler_run();
        /* woken by proc_exit of the target process */
    }

    uint32_t code = (uint32_t)p->exit_code;
    p->pid = 0;   /* reap slot */
    return code;
}

/* ── SYS_KILL (11) ───────────────────────────────────────────────────────── */
static uint32_t sys_kill(uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    (void)arg3;
    uint32_t pid = arg1;
    uint32_t sig = arg2;

    proc_t *target = proc_find(pid);
    if (!target || target->state != PROC_ALIVE) return (uint32_t)-1;

    proc_signal(target, sig);
    return 0;
}

/* ── SYS_SLEEP (12) ──────────────────────────────────────────────────────── */
static uint32_t sys_sleep(uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    (void)arg2; (void)arg3;
    uint32_t ms = arg1;

    proc_t *p = proc_current();
    if (!p) return (uint32_t)-1;

    uint32_t ticks = (ms + 9) / 10;
    if (ticks == 0) ticks = 1;

    p->sleep_until = timer_ticks() + ticks;

    /* Phase 25: block; timer IRQ will wake us when deadline passes */
    task_block(current_task);
    scheduler_request();
    scheduler_run();
    /* woken by timer_handler in timer.c */

    p->sleep_until = 0;
    return 0;
}

/* ── dispatcher ──────────────────────────────────────────────────────────── */
void syscall_dispatch(regs_t *r) {

    uint32_t num = r->eax;

    switch (num) {

        case 1:
            vga_putchar((char)r->ebx);
            r->eax = 0;
            return;

        case 2:
            r->eax = kbd_buf_get();
            return;
    }

    if (num >= SYSCALL_MAX || !syscall_table[num]) {
        r->eax = (uint32_t)-1;
        return;
    }

    if (num == SYS_FORK)
        r->eax = 0;

    r->eax = syscall_table[num](r->ebx, r->ecx, r->edx);

    proc_check_signals();
}

static uint32_t sys_fork(uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    (void)arg1; (void)arg2; (void)arg3;

    proc_t *parent = proc_current();
    if (!parent) return (uint32_t)-1;

    /*
     * We need the current kernel ESP so task_clone() can copy the
     * exact syscall return frame sitting on the stack right now.
     * Reading ESP here captures it just before we call proc_fork.
     */
    uint32_t esp_now;
    __asm__ volatile("mov %%esp, %0" : "=r"(esp_now));

    /*
     * Set eax=0 in the PARENT's regs_t frame *before* cloning,
     * so the copied frame already has child return value = 0.
     * The syscall dispatcher will overwrite parent's eax with the
     * return value of this function (child pid) after we return.
     *
     * We accomplish this by reaching back into the regs_t that
     * syscall_dispatch passed us — it sits just above our own
     * stack frame.  The cleanest way is to accept regs_t * directly.
     * We rely on syscall_dispatch passing r down to us via a wrapper.
     *
     * See note below — syscall_dispatch calls the raw handler with
     * three uint32_t args.  For fork we need the frame pointer.
     * We solve this by keeping eax patching inside syscall_dispatch.
     */
    proc_t *child = proc_fork(parent, esp_now);
    if (!child) return (uint32_t)-1;

    /* switch child to its own page directory when it is scheduled */
    /* (child's task already has child_dir; actual CR3 load happens
       at next context switch via the scheduler — see note below)    */

    scheduler_request();   /* yield so child can run soon */
    return child->pid;     /* parent receives child pid */
}

/* ── init ────────────────────────────────────────────────────────────────── */
void syscall_init(void) {
    uint32_t i;
    for (i = 0; i < SYSCALL_MAX; i++)
        syscall_table[i] = (syscall_fn_t)0;

    syscall_table[SYS_WRITE]  = sys_write;
    syscall_table[SYS_GETPID] = sys_getpid;
    syscall_table[SYS_YIELD]  = sys_yield;
    syscall_table[SYS_EXIT]   = sys_exit;
    syscall_table[SYS_OPEN]   = sys_open;
    syscall_table[SYS_READ]   = sys_read;
    syscall_table[SYS_FWRITE] = sys_fwrite;
    syscall_table[SYS_CLOSE]  = sys_close;
    syscall_table[SYS_PIPE]   = sys_pipe;
    syscall_table[SYS_EXEC]   = sys_exec;
    syscall_table[SYS_WAIT]   = sys_wait;
    syscall_table[SYS_KILL]   = sys_kill;
    syscall_table[SYS_SLEEP]  = sys_sleep;
    syscall_table[SYS_FORK]  = sys_fork;
}