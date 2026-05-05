#include "types.h"
#include "pmm.h"

static uint32_t *bitmap = (uint32_t *)PMM_BITMAP_ADDR;

static void bit_set(uint32_t frame) {
    bitmap[frame / 32] |= (1u << (frame % 32));
}

static void bit_clear(uint32_t frame) {
    bitmap[frame / 32] &= ~(1u << (frame % 32));
}

static int bit_test(uint32_t frame) {
    return (bitmap[frame / 32] >> (frame % 32)) & 1;
}

void pmm_mark_used(uint32_t addr, uint32_t len) {
    uint32_t frame     = addr / PAGE_SIZE;
    uint32_t frame_end = (addr + len + PAGE_SIZE - 1) / PAGE_SIZE;
    for (; frame < frame_end && frame < PMM_TOTAL_FRAMES; frame++)
        bit_set(frame);
}

void pmm_mark_free(uint32_t addr, uint32_t len) {
    uint32_t frame     = addr / PAGE_SIZE;
    uint32_t frame_end = (addr + len + PAGE_SIZE - 1) / PAGE_SIZE;
    for (; frame < frame_end && frame < PMM_TOTAL_FRAMES; frame++)
        bit_clear(frame);
}

void pmm_init(void) {
    uint32_t i;

    /* start fully used */
    for (i = 0; i < PMM_BITMAP_WORDS; i++)
        bitmap[i] = 0xFFFFFFFF;

    /*
     * Only free memory above 1MB for kernel use.
     * Do NOT free low conventional RAM (0x1000-0x9FFFF).
     * Reason: paging_init() allocates page table frames from the PMM.
     * If those frames come from low memory, get_or_create_pt() zeroes
     * them, stomping on BIOS data and the real-mode IVT, which can
     * cause unpredictable faults even in protected mode.
     * All kernel structures (page tables, heap, stacks) live above 1MB.
     */
    pmm_mark_free(0x00200000, PMM_RAM_BYTES - 0x00200000);

    /* fixed reservations above 1MB */
    pmm_mark_used(PMM_BITMAP_ADDR, PMM_BITMAP_WORDS * sizeof(uint32_t));
    pmm_mark_used(0x00500000, PAGE_SIZE);       /* page directory */
    pmm_mark_used(0x00600000, 0x00200000);      /* heap */
    pmm_mark_used(0x00800000, 0x00400000);      /* task stack arena */
}

uint32_t pmm_alloc_frame(void) {
    uint32_t i, b;
    for (i = 0; i < PMM_BITMAP_WORDS; i++) {
        if (bitmap[i] == 0xFFFFFFFF)
            continue;
        for (b = 0; b < 32; b++) {
            uint32_t frame = i * 32 + b;
            if (!bit_test(frame)) {
                bit_set(frame);
                return frame * PAGE_SIZE;
            }
        }
    }
    return 0;
}

void pmm_free_frame(uint32_t addr) {
    uint32_t frame = addr / PAGE_SIZE;
    if (frame < PMM_TOTAL_FRAMES)
        bit_clear(frame);
}

uint32_t pmm_free_frames(void) {
    uint32_t i, b, count = 0;
    for (i = 0; i < PMM_BITMAP_WORDS; i++)
        for (b = 0; b < 32; b++)
            if (!((bitmap[i] >> b) & 1))
                count++;
    return count;
}