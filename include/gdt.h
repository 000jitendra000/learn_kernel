#ifndef GDT_H
#define GDT_H

#include "types.h"

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) gdt_ptr_t;

#define SEG_KERNEL_CODE  0x08
#define SEG_KERNEL_DATA  0x10
#define SEG_USER_CODE    0x18
#define SEG_USER_DATA    0x20
#define SEG_TSS          0x28

#define RPL3          0x3
#define SEL_USER_CODE (SEG_USER_CODE | RPL3)   /* 0x1B */
#define SEL_USER_DATA (SEG_USER_DATA | RPL3)   /* 0x23 */

void gdt_init(void);
void tss_set_kernel_stack(uint32_t esp0);

#endif