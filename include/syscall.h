#ifndef SYSCALL_H
#define SYSCALL_H

#include "types.h"
#include "idt.h"   // <-- needed for regs_t

#define SYS_WRITE   0
#define SYS_GETPID  1
#define SYS_YIELD   2
#define SYS_EXIT    3

#define SYSCALL_MAX 16

void syscall_init(void);
void syscall_dispatch(regs_t *r);   // ✅ ADD THIS

#endif