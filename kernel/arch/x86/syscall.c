#include "types.h"
#include "syscall.h"
#include "idt.h"
#include "task.h"
#include "process.h"

typedef uint32_t (*syscall_fn_t)(uint32_t, uint32_t, uint32_t);

static syscall_fn_t syscall_table[SYSCALL_MAX];

extern void vga_puts(const char *s);
extern void scheduler_request(void);
extern void scheduler_run(void);

/* sys_write(ebx = char *) */
static uint32_t sys_write(uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    (void)arg2; (void)arg3;
    const char *s = (const char *)arg1;
    if (!s) return (uint32_t)-1;
    vga_puts(s);
    return 0;
}

/* sys_getpid() — reads from process table via O(1) owner pointer */
static uint32_t sys_getpid(uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    (void)arg1; (void)arg2; (void)arg3;
    return proc_getpid();
}

/* sys_yield() — cooperative yield, safe in kernel context */
static uint32_t sys_yield(uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    (void)arg1; (void)arg2; (void)arg3;
    scheduler_request();
    scheduler_run();
    return 0;
}

/* sys_exit(ebx = exit_code) — marks process dead, never returns */
static uint32_t sys_exit(uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    (void)arg2; (void)arg3;
    proc_exit((int32_t)arg1);
    return 0;   /* unreachable */
}

void syscall_dispatch(regs_t *r) {
    uint32_t num = r->eax;

    if (num >= SYSCALL_MAX || !syscall_table[num]) {
        r->eax = (uint32_t)-1;
        return;
    }

    r->eax = syscall_table[num](r->ebx, r->ecx, r->edx);
}

void syscall_init(void) {
    uint32_t i;
    for (i = 0; i < SYSCALL_MAX; i++)
        syscall_table[i] = (syscall_fn_t)0;

    syscall_table[SYS_WRITE]  = sys_write;
    syscall_table[SYS_GETPID] = sys_getpid;
    syscall_table[SYS_YIELD]  = sys_yield;
    syscall_table[SYS_EXIT]   = sys_exit;
}