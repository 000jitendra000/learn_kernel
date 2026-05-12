#ifndef PAGING_H
#define PAGING_H

#include "types.h"

#define PAGE_PRESENT  0x01
#define PAGE_WRITE    0x02
#define PAGE_USER     0x04
#define PAGE_COW      0x200   /* Phase 26: software bit — page is COW-shared */

#define PAGE_DIR_ADDR 0x00500000

/*
 * Kernel region:  PDE[0]–PDE[3]   (0x00000000 – 0x00FFFFFF)
 * User region:    PDE[4]+          (0x01000000+)
 */

void      paging_init(void);
void      paging_map_page(uint32_t *dir, uint32_t virt, uint32_t phys,
                          uint32_t flags);
uint32_t *paging_create_user_dir(void);
void      paging_switch(uint32_t *dir);
void      page_map(uint32_t virt, uint32_t phys, uint32_t flags);
uint32_t *paging_get_kernel_dir(void);

/* Phase 26 */
uint32_t *paging_clone_cow(uint32_t *src_dir);
void      paging_free_user_dir(uint32_t *dir);

extern uint32_t *kernel_dir;

#endif