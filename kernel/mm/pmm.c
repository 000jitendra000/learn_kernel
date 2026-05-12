#include "types.h"
#include "pmm.h"
#define PMM_BITMAP_ADDR   0x00180000
static uint32_t *bitmap = (uint32_t *)PMM_BITMAP_ADDR;

/*
 * Phase 26: per-frame reference counts.
 * Lives just above the bitmap.  8192 frames × 2 bytes = 16 KB.
 * PMM_BITMAP_ADDR + 256 × 4 = 0x00400400; refcount array at 0x00400400.
 */
#define PMM_REFCOUNT_ADDR (PMM_BITMAP_ADDR + PMM_BITMAP_WORDS * sizeof(uint32_t))
static uint16_t *frame_refs = (uint16_t *)PMM_REFCOUNT_ADDR;

/* ── bitmap helpers ──────────────────────────────────────────────────────── */

static void bit_set(uint32_t frame) {
    bitmap[frame / 32] |= (1u << (frame % 32));
}

static void bit_clear(uint32_t frame) {
    bitmap[frame / 32] &= ~(1u << (frame % 32));
}

static int bit_test(uint32_t frame) {
    return (bitmap[frame / 32] >> (frame % 32)) & 1;
}

/* ── public API ──────────────────────────────────────────────────────────── */

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

    /* mark everything used */
    for (i = 0; i < PMM_BITMAP_WORDS; i++)
        bitmap[i] = 0xFFFFFFFF;

    /* zero all refcounts */
    for (i = 0; i < PMM_TOTAL_FRAMES; i++)
        frame_refs[i] = 0;

    /* free usable RAM above 2 MB */
    pmm_mark_free(0x00200000, PMM_RAM_BYTES - 0x00200000);

    /* fixed reservations */
    pmm_mark_used(PMM_BITMAP_ADDR,  PMM_BITMAP_WORDS * sizeof(uint32_t)
                                  + PMM_TOTAL_FRAMES * sizeof(uint16_t));
    pmm_mark_used(0x00500000, PAGE_SIZE);       /* page directory */
    pmm_mark_used(0x00600000, 0x00200000);      /* heap */
    pmm_mark_used(0x00800000, 0x00400000);      /* task stack arena */
}

uint32_t pmm_alloc_frame(void) {
    uint32_t i, b;
    for (i = 0; i < PMM_BITMAP_WORDS; i++) {
        if (bitmap[i] == 0xFFFFFFFF) continue;
        for (b = 0; b < 32; b++) {
            uint32_t frame = i * 32 + b;
            if (!bit_test(frame)) {
                bit_set(frame);
                frame_refs[frame] = 1;   /* Phase 26: start at 1 */
                return frame * PAGE_SIZE;
            }
        }
    }
    return 0;
}

void pmm_free_frame(uint32_t addr) {
    uint32_t frame = addr / PAGE_SIZE;
    if (frame < PMM_TOTAL_FRAMES) {
        frame_refs[frame] = 0;
        bit_clear(frame);
    }
}

uint32_t pmm_free_frames(void) {
    uint32_t i, b, count = 0;
    for (i = 0; i < PMM_BITMAP_WORDS; i++)
        for (b = 0; b < 32; b++)
            if (!((bitmap[i] >> b) & 1))
                count++;
    return count;
}

/* ── Phase 26: reference counting ───────────────────────────────────────── */

void pmm_ref(uint32_t addr) {
    uint32_t frame = addr / PAGE_SIZE;
    if (frame < PMM_TOTAL_FRAMES && frame_refs[frame] < 0xFFFF)
        frame_refs[frame]++;
}

void pmm_unref(uint32_t addr) {
    uint32_t frame = addr / PAGE_SIZE;
    if (frame >= PMM_TOTAL_FRAMES) return;
    if (frame_refs[frame] == 0)   return;

    frame_refs[frame]--;
    if (frame_refs[frame] == 0)
        bit_clear(frame);   /* return to free pool */
}

uint16_t pmm_get_ref(uint32_t addr) {
    uint32_t frame = addr / PAGE_SIZE;
    if (frame >= PMM_TOTAL_FRAMES) return 0;
    return frame_refs[frame];
}