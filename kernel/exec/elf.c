#include "types.h"
#include "elf.h"
#include "process.h"

extern void *kmalloc(uint32_t size);
extern void  vga_puts(const char *s);

/* ── minimal memory helpers (no libc) ─────────────────────────────────── */

static void el_memcpy(uint8_t *dst, const uint8_t *src, uint32_t n) {
    uint32_t i;
    for (i = 0; i < n; i++)
        dst[i] = src[i];
}

static void el_memzero(uint8_t *dst, uint32_t n) {
    uint32_t i;
    for (i = 0; i < n; i++)
        dst[i] = 0;
}

/* ── ELF validation ────────────────────────────────────────────────────── */

static int elf_valid(const Elf32_Ehdr *hdr) {
    /* magic */
    if (hdr->e_ident[0] != ELFMAG0 ||
        hdr->e_ident[1] != ELFMAG1 ||
        hdr->e_ident[2] != ELFMAG2 ||
        hdr->e_ident[3] != ELFMAG3) {
        vga_puts("elf: bad magic\n");
        return 0;
    }

    /* 32-bit ELF */
    if (hdr->e_ident[4] != ELFCLASS32) {
        vga_puts("elf: not 32-bit\n");
        return 0;
    }

    /* executable */
    if (hdr->e_type != ET_EXEC) {
        vga_puts("elf: not ET_EXEC\n");
        return 0;
    }

    /* x86 */
    if (hdr->e_machine != EM_386) {
        vga_puts("elf: not x86\n");
        return 0;
    }

    return 1;
}

/* ── loader ────────────────────────────────────────────────────────────── */

uint32_t elf_load(const uint8_t *image) {
    const Elf32_Ehdr *hdr = (const Elf32_Ehdr *)image;

    if (!elf_valid(hdr))
        return 0;

    /* sanity: program header table must exist */
    if (hdr->e_phoff == 0 || hdr->e_phnum == 0) {
        vga_puts("elf: no program headers\n");
        return 0;
    }

    /*
     * Phase 15:
     * load only first PT_LOAD segment.
     *
     * Real ELF may contain multiple loadable segments:
     * text, rodata, data, bss.
     *
     * We keep it simple for now.
     */
    uint32_t i;
    for (i = 0; i < hdr->e_phnum; i++) {
        const Elf32_Phdr *ph =
            (const Elf32_Phdr *)(image +
                                 hdr->e_phoff +
                                 i * hdr->e_phentsize);

        if (ph->p_type != PT_LOAD)
            continue;

        if (ph->p_memsz == 0)
            continue;

        /* ELF invariant: file size cannot exceed memory size */
        if (ph->p_filesz > ph->p_memsz) {
            vga_puts("elf: invalid segment size\n");
            return 0;
        }

        /* overflow check: p_offset + p_filesz */
        if (ph->p_offset + ph->p_filesz < ph->p_offset) {
            vga_puts("elf: segment overflow\n");
            return 0;
        }

        /* allocate destination (never trust p_vaddr directly) */
        uint8_t *dst = (uint8_t *)kmalloc(ph->p_memsz);
        if (!dst) {
            vga_puts("elf: kmalloc failed\n");
            return 0;
        }

        /* copy initialized bytes */
        if (ph->p_filesz > 0) {
            el_memcpy(dst,
                      image + ph->p_offset,
                      ph->p_filesz);
        }

        /* zero uninitialized bytes (.bss) */
        if (ph->p_memsz > ph->p_filesz) {
            el_memzero(dst + ph->p_filesz,
                       ph->p_memsz - ph->p_filesz);
        }

        /*
         * Validate entry point belongs inside this segment.
         * Prevent unsigned underflow in relocation.
         */
        if (hdr->e_entry < ph->p_vaddr ||
            hdr->e_entry >= ph->p_vaddr + ph->p_memsz) {
            vga_puts("elf: bad entry\n");
            return 0;
        }

        /*
         * Relocate entry:
         *
         * original: p_vaddr ... e_entry
         * loaded:   dst     ... relocated
         */
        uint32_t relocated =
            (uint32_t)dst +
            (hdr->e_entry - ph->p_vaddr);

        return relocated;
    }

    vga_puts("elf: no PT_LOAD segment\n");
    return 0;
}

/* ── exec ──────────────────────────────────────────────────────────────── */

struct proc *exec_process(void (*entry)(void), uint32_t ppid) {
    return proc_create(entry, ppid);
}