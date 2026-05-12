#include "types.h"
#include "elf.h"
#include "process.h"
#include "paging.h"
#include "pmm.h"
#define TEMP_MAP_BASE 0x00C00000

extern uint32_t *kernel_dir;
extern void vga_puts(const char *s);
extern void vga_puthex(uint32_t val);

static void el_memcpy(uint8_t *dst, const uint8_t *src, uint32_t n) {
    uint32_t i;
    for (i = 0; i < n; i++) dst[i] = src[i];
}

static void el_memzero(uint8_t *dst, uint32_t n) {
    uint32_t i;
    for (i = 0; i < n; i++) dst[i] = 0;
}

static int elf_valid(const Elf32_Ehdr *hdr) {
    if (hdr->e_ident[0] != ELFMAG0 || hdr->e_ident[1] != ELFMAG1 ||
        hdr->e_ident[2] != ELFMAG2 || hdr->e_ident[3] != ELFMAG3) {
        vga_puts("elf: bad magic\n"); return 0;
    }
    if (hdr->e_ident[4] != ELFCLASS32) { vga_puts("elf: not 32-bit\n"); return 0; }
    if (hdr->e_type    != ET_EXEC)     { vga_puts("elf: not ET_EXEC\n"); return 0; }
    if (hdr->e_machine != EM_386)      { vga_puts("elf: not x86\n");     return 0; }
    return 1;
}

/*
 * elf_load_into(image, dir)
 *
 * For each PT_LOAD segment:
 *   1. page-align vaddr down, round memsz up
 *   2. allocate one physical frame per page from PMM
 *   3. map each frame into `dir` at the segment's virtual address with PAGE_USER
 *   4. copy file data, zero BSS
 *
 * Returns hdr->e_entry directly (no relocation — we map at p_vaddr).
 * Returns 0 on error.
 */
uint32_t elf_load_into(const uint8_t *image, uint32_t *dir) {
    const Elf32_Ehdr *hdr = (const Elf32_Ehdr *)image;

    if (!elf_valid(hdr)) return 0;
    if (!hdr->e_phoff || !hdr->e_phnum) {
        vga_puts("elf: no phdrs\n"); return 0;
    }

    uint32_t i;
    for (i = 0; i < hdr->e_phnum; i++) {
        const Elf32_Phdr *ph = (const Elf32_Phdr *)(
            image + hdr->e_phoff + i * hdr->e_phentsize);

        if (ph->p_type != PT_LOAD) continue;
        if (ph->p_memsz == 0)     continue;
        if (ph->p_filesz > ph->p_memsz) { vga_puts("elf: bad seg\n"); return 0; }

        /* page-align the mapping region */
        uint32_t vaddr_aligned = ph->p_vaddr & ~0xFFF;
        uint32_t offset_into_page = ph->p_vaddr - vaddr_aligned;
        uint32_t map_size = (ph->p_memsz + offset_into_page + 0xFFF) & ~0xFFF;
        uint32_t num_pages = map_size / PAGE_SIZE;

        vga_puts("elf: load vaddr="); vga_puthex(ph->p_vaddr);
        vga_puts(" pages=");          vga_puthex(num_pages);
        vga_puts("\n");

        uint32_t p;

            for (p = 0; p < num_pages; p++) {

                uint32_t frame = pmm_alloc_frame();
                if (!frame) {
                    vga_puts("elf: PMM OOM\n");
                    return 0;
                }

                uint32_t vpage = vaddr_aligned + p * PAGE_SIZE;

                /*
                * Map user page into child address space
                */
                paging_map_page(dir,
                                vpage,
                                frame,
                                PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
            }

            /*
            * Now temporarily switch into child CR3
            * and write directly into mapped userspace.
            */

            uint32_t *old_dir = paging_get_kernel_dir();

            paging_switch(dir);
            __asm__ volatile("mov %%cr3, %%eax\nmov %%eax, %%cr3" ::: "eax");

            /*
            * Zero full segment memory
            */
            el_memzero((uint8_t *)ph->p_vaddr,
                    ph->p_memsz);

            /*
            * Copy ELF bytes
            */
            el_memcpy((uint8_t *)ph->p_vaddr,
                    image + ph->p_offset,
                    ph->p_filesz);

            /*
            * Restore kernel CR3
            */
            paging_switch(old_dir);
        }
    

    return hdr->e_entry;   /* exact virtual entry — no relocation */
}

/* ── exec: map ELF + stack, create user process ─────────────────────────── */

#define USER_STACK_PAGE  0x00500000
#define USER_STACK_TOP   0x00501000

struct proc *exec_elf(const uint8_t *image, uint32_t ppid) {
    /* 1. create a fresh user page directory (clones kernel PDEs[0-3]) */
    uint32_t *dir = paging_create_user_dir();
    if (!dir) { vga_puts("exec: no dir\n"); return (struct proc *)0; }

    /* 2. load ELF segments into the user directory */
    uint32_t entry = elf_load_into(image, dir);
    if (!entry) { vga_puts("exec: elf_load failed\n"); return (struct proc *)0; }

    vga_puts("exec: entry="); vga_puthex(entry); vga_puts("\n");
    uint32_t pdi = entry >> 22;
    uint32_t pti = (entry >> 12) & 0x3FF;

    vga_puts("PDE=");
    vga_puthex(dir[pdi]);
    vga_puts("\n");

    vga_puts(" PTE=");

    uint32_t *pt = (uint32_t *)(dir[pdi] & ~0xFFF);

    if (pt)
        vga_puthex(pt[pti]);
    else
        vga_puts("NULL");

    vga_puts("\n");

    /* 3. map user stack (one page, user-accessible) */
    uint32_t stack_frame = pmm_alloc_frame();
    if (!stack_frame) { vga_puts("exec: no stack frame\n"); return (struct proc *)0; }
    el_memzero((uint8_t *)stack_frame, PAGE_SIZE);
    paging_map_page(dir, USER_STACK_PAGE, stack_frame,
                    PAGE_PRESENT | PAGE_WRITE | PAGE_USER);

    vga_puts("exec: stack page="); vga_puthex(USER_STACK_PAGE);
    vga_puts(" top=");             vga_puthex(USER_STACK_TOP);
    vga_puts("\n");

    /* 4. create user process with this directory */
    return proc_create_user(entry, USER_STACK_TOP, dir, ppid);
}