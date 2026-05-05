#include "types.h"
#include "paging.h"
#include "pmm.h"

/*
 * Page directory at PAGE_DIR_ADDR (0x00500000).
 * Each PDE points to a page table allocated from the PMM.
 * Page tables are 4KB = 1024 x uint32_t entries each.
 *
 * Identity map all 32MB so virtual == physical throughout.
 * Enables paging without changing any existing address.
 */

#define PD_ENTRIES   1024
#define PT_ENTRIES   1024
#define PDE_IDX(v)   ((v) >> 22)
#define PTE_IDX(v)   (((v) >> 12) & 0x3FF)

static uint32_t *page_dir = (uint32_t *)PAGE_DIR_ADDR;

/* ── internal: get or create a page table for a given PD index ───────────── */

static uint32_t *get_or_create_pt(uint32_t pdi) {
    if (page_dir[pdi] & PAGE_PRESENT) {
        /* already exists — strip flags to get physical address */
        return (uint32_t *)(page_dir[pdi] & ~0xFFF);
    }

    /* allocate a fresh frame for this page table */
    uint32_t frame = pmm_alloc_frame();
    if (!frame) return (uint32_t *)0;   /* OOM */

    /* zero the page table */
    uint32_t *pt = (uint32_t *)frame;
    uint32_t i;
    for (i = 0; i < PT_ENTRIES; i++)
        pt[i] = 0;

    /* install into page directory */
    page_dir[pdi] = frame | PAGE_PRESENT | PAGE_WRITE;

    return pt;
}

/* ── public: map one 4KB page ─────────────────────────────────────────────── */

void page_map(uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t pdi = PDE_IDX(virt);
    uint32_t pti = PTE_IDX(virt);

    uint32_t *pt = get_or_create_pt(pdi);
    if (!pt) return;

    pt[pti] = (phys & ~0xFFF) | (flags & 0xFFF) | PAGE_PRESENT;
}

/* ── reload CR3 to flush all TLB entries ─────────────────────────────────── */

static void tlb_flush(void) {
    __asm__ volatile(
        "mov %%cr3, %%eax\n"
        "mov %%eax, %%cr3\n"
        ::: "eax"
    );
}

/* ── init ────────────────────────────────────────────────────────────────── */

void paging_init(void) {
    uint32_t addr;

    /* zero the page directory */
    uint32_t i;
    for (i = 0; i < PD_ENTRIES; i++)
        page_dir[i] = 0;

    /* identity map all 32MB: 0x00000000 – 0x01FFFFFF */
    for (addr = 0; addr < PMM_RAM_BYTES; addr += PAGE_SIZE)
        page_map(addr, addr, PAGE_PRESENT | PAGE_WRITE);

    /* load page directory into CR3 */
    __asm__ volatile("mov %0, %%cr3" :: "r"(PAGE_DIR_ADDR) : "memory");

    /* enable paging: set CR0.PG (bit 31) */
    __asm__ volatile(
        "mov %%cr0,    %%eax\n"
        "or  $0x80000000, %%eax\n"
        "mov %%eax,    %%cr0\n"
        ::: "eax"
    );

    /* flush TLB after enabling */
    tlb_flush();
}