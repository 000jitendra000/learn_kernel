#ifndef SYSCALL_H
#define SYSCALL_H

#include "types.h"
#include "idt.h"

#define SYS_WRITE   0
#define SYS_GETPID  1
#define SYS_YIELD   2
#define SYS_EXIT    3
#define SYS_OPEN    4
#define SYS_READ    5
#define SYS_FWRITE  6
#define SYS_CLOSE   7
#define SYS_PIPE    8
#define SYS_EXEC    9
#define SYS_WAIT   10
#define SYS_KILL   11
#define SYS_SLEEP  12
#define SYS_FORK   13   /* Phase 26: → eax=child_pid (parent) or 0 (child) */

#define SYSCALL_MAX 16

void syscall_init(void);
void syscall_dispatch(regs_t *r);

#endif