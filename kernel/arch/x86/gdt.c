#include "gdt.h"
#include "tss.h"
#include "types.h"

#define GDT_ENTRIES 6

static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_ptr_t   gdt_ptr;

tss_t kernel_tss;

extern void gdt_load(uint32_t);
extern void tss_load(void);

static void gdt_set_gate(int i, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t gran) {
    gdt[i].base_low    = base & 0xFFFF;
    gdt[i].base_mid    = (base >> 16) & 0xFF;
    gdt[i].base_high   = (base >> 24) & 0xFF;
    gdt[i].limit_low   = limit & 0xFFFF;
    gdt[i].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[i].access      = access;
}

static void tss_install(int i, uint32_t base, uint32_t limit) {
    gdt[i].limit_low   = limit & 0xFFFF;
    gdt[i].base_low    = base & 0xFFFF;
    gdt[i].base_mid    = (base >> 16) & 0xFF;
    gdt[i].base_high   = (base >> 24) & 0xFF;
    gdt[i].access      = 0x89;
    gdt[i].granularity = 0x00;
}

void tss_set_kernel_stack(uint32_t esp0) {
    kernel_tss.esp0 = esp0;
}

void gdt_init(void) {
    gdt_set_gate(0, 0, 0, 0, 0);                        /* null */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);         /* kernel code */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);         /* kernel data */
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);         /* user code */
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);         /* user data */

    uint32_t tss_base  = (uint32_t)&kernel_tss;
    uint32_t tss_limit = sizeof(tss_t) - 1;

    uint8_t *p = (uint8_t *)&kernel_tss;
    uint32_t i;
    for (i = 0; i < sizeof(tss_t); i++) p[i] = 0;

    kernel_tss.ss0        = SEG_KERNEL_DATA;
    kernel_tss.esp0       = 0;
    kernel_tss.iomap_base = sizeof(tss_t);

    tss_install(5, tss_base, tss_limit);

    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base  = (uint32_t)&gdt;

    gdt_load((uint32_t)&gdt_ptr);
    tss_load();
}