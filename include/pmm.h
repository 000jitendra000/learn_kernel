#ifndef PMM_H
#define PMM_H

#include "types.h"

#define PAGE_SIZE        4096
#define PMM_BITMAP_ADDR  0x00400000
#define PMM_RAM_MB       32
#define PMM_RAM_BYTES    (PMM_RAM_MB * 1024 * 1024)
#define PMM_TOTAL_FRAMES (PMM_RAM_BYTES / PAGE_SIZE)       /* 8192 */
#define PMM_BITMAP_WORDS ((PMM_TOTAL_FRAMES + 31) / 32)   /* 256  */
#define PMM_BITMAP_ADDR   0x00180000

void     pmm_init(void);
uint32_t pmm_alloc_frame(void);
void     pmm_free_frame(uint32_t addr);
void     pmm_mark_used(uint32_t addr, uint32_t len);
void     pmm_mark_free(uint32_t addr, uint32_t len);
uint32_t pmm_free_frames(void);

/* Phase 26: per-frame reference counting for COW */
void     pmm_ref(uint32_t addr);       /* increment refcount */
void     pmm_unref(uint32_t addr);     /* decrement; free if reaches 0 */
uint16_t pmm_get_ref(uint32_t addr);   /* inspect refcount */

#endif