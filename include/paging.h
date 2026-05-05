#ifndef PAGING_H
#define PAGING_H

#include "types.h"

#define PAGE_PRESENT  0x01
#define PAGE_WRITE    0x02
#define PAGE_USER     0x04

#define PAGE_DIR_ADDR 0x00500000    /* page directory lives here */

void paging_init(void);
void page_map(uint32_t virt, uint32_t phys, uint32_t flags);

#endif