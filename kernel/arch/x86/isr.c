#include "types.h"
#include "idt.h"
#include "process.h"

extern void vga_puts(const char *s);
extern void vga_puthex(uint32_t v);
extern void syscall_dispatch(regs_t *r);
extern int  paging_cow_fault(uint32_t fault_addr);

/*
 * isr_handler — called from isr_common (isr.asm) with a pointer to the
 * full register frame pushed onto the kernel stack.
 *
 * regs_t layout must match the push order in isr.asm.
 */
void isr_handler(regs_t *r) {

    vga_putchar('I');

    /* ── int 0x80: syscall ───────────────────────────────────────────────── */
    if (r->int_no == 128) {
        syscall_dispatch(r);
        return;
    }

    /* ── #PF: page fault (vector 14) ─────────────────────────────────────── */
    if (r->int_no == 14) {
        uint32_t fault_addr;
        __asm__ volatile("mov %%cr2, %0" : "=r"(fault_addr));

        /*
         * error code bits:
         *   bit 0 = present   (1 = protection violation, 0 = not-present)
         *   bit 1 = write     (1 = write access)
         *   bit 2 = user      (1 = CPL=3)
         *
         * COW trigger: present=1, write=1, user=1  →  err & 0x7 == 0x7
         * (We accept err=3 too in case PAGE_COW is set but user bit in
         *  error code depends on exact CPL — check present+write minimum.)
         */
        uint32_t err = r->err_code;

        if ((err & 0x03) == 0x03) {          /* present + write */
            if (paging_cow_fault(fault_addr))
                return;                       /* handled — resume */
        }

        /* unhandled page fault — panic */
        vga_puts("\n*** PAGE FAULT ***\n");
        vga_puts("  addr=");  vga_puthex(fault_addr);
        vga_puts("  err=");   vga_puthex(err);
        vga_puts("  eip=");   vga_puthex(r->eip);
        vga_puts("\n");
        for (;;) __asm__ volatile("hlt");
    }

    /* ── all other exceptions: panic ────────────────────────────────────── */
    vga_puts("\n*** EXCEPTION ");
    vga_puthex(r->int_no);
    vga_puts(" err=");
    vga_puthex(r->err_code);
    vga_puts(" eip=");
    vga_puthex(r->eip);
    vga_puts(" ***\n");
    for (;;) __asm__ volatile("hlt");
}