#ifndef ELF_H
#define ELF_H

#include "types.h"

/* ── ELF32 types ─────────────────────────────────────────────────────────── */
typedef uint32_t Elf32_Addr;
typedef uint32_t Elf32_Off;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Word;
typedef int32_t  Elf32_Sword;

/* ── ELF identification ──────────────────────────────────────────────────── */
#define EI_NIDENT   16
#define ELFMAG0     0x7F
#define ELFMAG1     'E'
#define ELFMAG2     'L'
#define ELFMAG3     'F'
#define ELFCLASS32  1
#define ET_EXEC     2
#define EM_386      3
#define PT_LOAD     1

/* ── ELF32 header ────────────────────────────────────────────────────────── */
typedef struct {
    uint8_t    e_ident[EI_NIDENT];
    Elf32_Half e_type;
    Elf32_Half e_machine;
    Elf32_Word e_version;
    Elf32_Addr e_entry;
    Elf32_Off  e_phoff;
    Elf32_Off  e_shoff;
    Elf32_Word e_flags;
    Elf32_Half e_ehsize;
    Elf32_Half e_phentsize;
    Elf32_Half e_phnum;
    Elf32_Half e_shentsize;
    Elf32_Half e_shnum;
    Elf32_Half e_shstrndx;
} __attribute__((packed)) Elf32_Ehdr;

/* ── ELF32 program header ────────────────────────────────────────────────── */
typedef struct {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
} __attribute__((packed)) Elf32_Phdr;

/*
 * elf_load(image)
 *
 * Parse an ELF32 executable from a byte array.
 * For each PT_LOAD segment:
 *   - allocates memory via kmalloc(p_memsz)
 *   - copies p_filesz bytes from the image
 *   - zeroes the BSS region (p_memsz - p_filesz)
 *
 * Entry point is relocated to account for kmalloc placement:
 *   relocated_entry = alloc_base + (e_entry - p_vaddr)
 *
 * Returns relocated entry point, or 0 on error.
 * Only valid for single-PT_LOAD PIC executables.
 */
uint32_t elf_load(const uint8_t *image);

/*
 * exec_process(entry, ppid)
 *
 * Wrap a relocated entry point into a new process.
 * Returns proc_t *, or NULL on failure.
 */
struct proc *exec_process(void (*entry)(void), uint32_t ppid);

#endif