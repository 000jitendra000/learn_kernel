#include "types.h"
#include "paging.h"
#include "pmm.h"

extern void vga_puts(const char *s);
extern void vga_puthex(uint32_t v);

uint32_t *kernel_dir = (uint32_t *)PAGE_DIR_ADDR;

static void memzero(uint8_t *p, uint32_t n);

#define PD_ENTRIES  1024
#define PT_ENTRIES  1024
#define PDE_IDX(v)  ((v) >> 22)
#define PTE_IDX(v)  (((v) >> 12) & 0x3FF)

/* First user PDE index — everything at 0x01000000+ lives here */
#define USER_PDE_START 4


/* ── internal helpers ────────────────────────────────────────────────────── */


static uint32_t *get_or_create_pt(uint32_t *dir, uint32_t pdi, uint32_t flags) {
    if (dir[pdi] & PAGE_PRESENT)
        return (uint32_t *)(dir[pdi] & ~0xFFF);

    uint32_t frame = pmm_alloc_frame();
    if (!frame) return (uint32_t *)0;

    uint32_t *pt = (uint32_t *)frame;

    memzero((uint8_t *)pt, PAGE_SIZE);

    dir[pdi] = frame | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;

    return pt;
}

static void memcpy32(uint8_t *dst, const uint8_t *src, uint32_t n) {
    uint32_t i;
    for (i = 0; i < n; i++) dst[i] = src[i];
}

static void memzero(uint8_t *p, uint32_t n) {
    uint32_t i;
    for (i = 0; i < n; i++) p[i] = 0;
}

/* ── public paging API (unchanged from Phase 25) ────────────────────────── */

void paging_map_page(uint32_t *dir, uint32_t virt, uint32_t phys,
                     uint32_t flags) {
    uint32_t pdi = PDE_IDX(virt);
    uint32_t pti = PTE_IDX(virt);

    uint32_t *pt = get_or_create_pt(dir, pdi, flags);
    if (!pt) return;

    pt[pti] = (phys & ~0xFFF) | flags | PAGE_PRESENT;
}

void page_map(uint32_t virt, uint32_t phys, uint32_t flags) {
    paging_map_page(kernel_dir, virt, phys, flags);
}

uint32_t *paging_create_user_dir(void) {

    uint32_t frame = pmm_alloc_frame();
    if (!frame)
        return (uint32_t *)0;

    uint32_t *dir = (uint32_t *)frame;

    uint32_t i;

    /*
     * clear directory
     */
    memzero((uint8_t *)dir, PAGE_SIZE);

    /*
     * Copy ALL present kernel mappings.
     *
     * This keeps:
     *   - kernel code/data
     *   - heap
     *   - PMM bitmap
     *   - embedded ELF images
     *   - VGA memory
     * visible after CR3 switch.
     */

    for (i = 0; i < PD_ENTRIES; i++) {

        if (kernel_dir[i] & PAGE_PRESENT)
            dir[i] = kernel_dir[i];
    }

    return dir;
}

void paging_switch(uint32_t *dir) {
    __asm__ volatile("mov %0, %%cr3" :: "r"((uint32_t)dir) : "memory");
}

void paging_init(void) {
    uint32_t addr, i;

    /*
     * Clear page directory FIRST
     */
    for (i = 0; i < PD_ENTRIES; i++)
        kernel_dir[i] = 0;

    /*
     * Identity-map RAM
     */
    for (addr = 0; addr < PMM_RAM_BYTES; addr += PAGE_SIZE)
        paging_map_page(kernel_dir,
                        addr,
                        addr,
                        PAGE_PRESENT | PAGE_WRITE);

    /*
     * Explicit task stack arena mapping
     */
    for (addr = 0x00800000;
         addr < 0x00C00000;
         addr += PAGE_SIZE) {

        paging_map_page(kernel_dir,
                        addr,
                        addr,
                        PAGE_PRESENT | PAGE_WRITE);
    }

    __asm__ volatile("mov %0, %%cr3"
                     :: "r"(PAGE_DIR_ADDR)
                     : "memory");

    __asm__ volatile(
        "mov %%cr0, %%eax\n"
        "or $0x80000000, %%eax\n"
        "mov %%eax, %%cr0\n"
        ::: "eax"
    );
}

uint32_t *paging_get_kernel_dir(void) { return kernel_dir; }

/* ── Phase 26: COW address-space cloning ────────────────────────────────── */

/*
 * paging_clone_cow — produce a child page directory that shares all
 * user pages with the parent read-only.
 *
 * For each user PDE that is present:
 *   1. allocate a fresh page table for the child
 *   2. for each present PTE:
 *        a. clear PAGE_WRITE in the child PTE
 *        b. set PAGE_COW  in the child PTE
 *        c. clear PAGE_WRITE in the PARENT PTE too  (parent must fault too)
 *        d. set PAGE_COW  in the parent PTE
 *        e. call pmm_ref() on the shared physical frame
 *   3. install child PT in child directory
 *
 * Kernel PDEs [0..USER_PDE_START-1] are shared directly (pointer copy).
 */
uint32_t *paging_clone_cow(uint32_t *src_dir) {
    uint32_t child_frame = pmm_alloc_frame();
    if (!child_frame) return (uint32_t *)0;

    uint32_t *child_dir = (uint32_t *)child_frame;
    uint32_t i, j;

    /* zero child directory */
    for (i = 0; i < PD_ENTRIES; i++) child_dir[i] = 0;

    /* share kernel PDEs directly */
    for (i = 0; i < USER_PDE_START; i++)
        child_dir[i] = src_dir[i];

    /* walk user PDEs */
    for (i = USER_PDE_START; i < PD_ENTRIES; i++) {
        if (!(src_dir[i] & PAGE_PRESENT)) continue;

        uint32_t *src_pt = (uint32_t *)(src_dir[i] & ~0xFFF);

        /* allocate a fresh page table for the child */
        uint32_t child_pt_frame = pmm_alloc_frame();
        if (!child_pt_frame) {
            /* out of memory — leak; caller should handle */
            return (uint32_t *)0;
        }
        uint32_t *child_pt = (uint32_t *)child_pt_frame;

        for (j = 0; j < PT_ENTRIES; j++) {
            if (!(src_pt[j] & PAGE_PRESENT)) {
                child_pt[j] = 0;
                continue;
            }

            uint32_t phys  = src_pt[j] & ~0xFFF;
            uint32_t flags = src_pt[j] &  0xFFF;

            /* mark both parent and child read-only + COW */
            flags &= ~PAGE_WRITE;
            flags |=  PAGE_COW;

            src_pt[j]   = phys | flags;   /* parent becomes read-only */
            child_pt[j] = phys | flags;   /* child shares same frame  */

            pmm_ref(phys);                /* bump refcount for shared frame */
        }

        /* flush parent's stale writable TLB entries for this PT */
        /* (full CR3 reload done by caller after clone returns)  */

        /* install child PT — inherit USER flag from parent PDE */
        uint32_t pde_flags = (src_dir[i] & 0xFFF) | PAGE_PRESENT | PAGE_USER;
        child_dir[i] = child_pt_frame | pde_flags;
    }

    return child_dir;
}

/*
 * paging_free_user_dir — walk a user directory, unref every user frame,
 * free each user page table, then free the directory itself.
 * Kernel PDEs are skipped (shared).
 * Called by proc_exit when a process's address space is torn down.
 */
void paging_free_user_dir(uint32_t *dir) {
    if (!dir) return;
    uint32_t i, j;

    for (i = USER_PDE_START; i < PD_ENTRIES; i++) {
        if (!(dir[i] & PAGE_PRESENT)) continue;

        uint32_t *pt = (uint32_t *)(dir[i] & ~0xFFF);

        for (j = 0; j < PT_ENTRIES; j++) {
            if (!(pt[j] & PAGE_PRESENT)) continue;
            uint32_t phys = pt[j] & ~0xFFF;
            pmm_unref(phys);
        }

        pmm_unref((uint32_t)pt);   /* free the page table itself */
    }

    pmm_unref((uint32_t)dir);      /* free the directory page */
}

/* ── Phase 26: COW page-fault break-out ─────────────────────────────────── */

/*
 * paging_cow_fault — called by the page-fault handler when:
 *   error_code & 3 == 3  (present + write)   and
 *   error_code & 4       (user mode)
 *
 * Returns 1 if the fault was handled (COW break), 0 if it should panic.
 */
int paging_cow_fault(uint32_t fault_addr) {
    /* find the current process's page directory */
    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    uint32_t *dir = (uint32_t *)cr3;

    uint32_t pdi = PDE_IDX(fault_addr);
    uint32_t pti = PTE_IDX(fault_addr);

    if (!(dir[pdi] & PAGE_PRESENT)) return 0;

    uint32_t *pt = (uint32_t *)(dir[pdi] & ~0xFFF);
    uint32_t  pte = pt[pti];

    if (!(pte & PAGE_PRESENT)) return 0;
    if (!(pte & PAGE_COW))     return 0;   /* not a COW page — real fault */

    uint32_t old_phys = pte & ~0xFFF;

    if (pmm_get_ref(old_phys) == 1) {
        /*
         * We are the only remaining owner of this frame.
         * Simply make it writable again — no copy needed.
         */
        pt[pti] = old_phys | (pte & ~(uint32_t)PAGE_COW) | PAGE_WRITE;
    } else {
        /* allocate a private copy */
        uint32_t new_phys = pmm_alloc_frame();
        if (!new_phys) return 0;   /* OOM — caller will panic */

        memcpy32((uint8_t *)new_phys, (const uint8_t *)old_phys, PAGE_SIZE);

        /* remap this process's PTE to the private copy, writable */
        uint32_t flags = (pte & 0xFFF) & ~(uint32_t)PAGE_COW;
        flags |= PAGE_WRITE;
        pt[pti] = new_phys | flags;

        pmm_unref(old_phys);   /* release our share of the old frame */
    }

    /* invalidate TLB for this page */
    __asm__ volatile("invlpg (%0)" :: "r"(fault_addr) : "memory");

    return 1;   /* handled */
}