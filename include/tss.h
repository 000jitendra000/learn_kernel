#ifndef TSS_H
#define TSS_H

#include "types.h"

/*
 * Minimal x86 TSS — we only use ss0/esp0.
 * All other fields zero.
 */
typedef struct {
    uint32_t prev_tss;
    uint32_t esp0;      /* kernel stack pointer — updated on every task switch */
    uint32_t ss0;       /* kernel stack segment = SEG_KERNEL_DATA (0x10)       */
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed)) tss_t;

extern tss_t kernel_tss;

#endif