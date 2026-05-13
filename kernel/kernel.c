#include "types.h"
#include "idt.h"
#include "pmm.h"
#include "paging.h"
#include "heap.h"
#include "task.h"
#include "process.h"
#include "syscall.h"
#include "elf.h"
#include "gdt.h"
#include "fs.h"
#include "pipe.h"
#include "kbd_buf.h"

void vga_clear(void);
void vga_puts(const char *s);
void vga_puthex(uint32_t val);
void vga_putchar(char c);
void keyboard_init(void);
void timer_init(uint32_t hz);
void scheduler_run(void);

extern uint8_t _binary_build_shell_elf_start[];
extern uint8_t _binary_build_shell_elf_end[];

extern uint8_t _binary_build_hello_elf_start[];
extern uint8_t _binary_build_hello_elf_end[];



void kernel_main(void) {
    vga_clear();
    vga_puts("MyKernel v0.1\n");
    vga_puts("Phase 26: Copy-on-Write Fork\n\n");

    gdt_init();
    idt_init();
    pmm_init();
    paging_init();
    heap_init();
    fs_init();
    pipe_init();
    kbd_buf_init();

    task_init();
    task_adopt_current();
    proc_init();
    scheduler_init();
    syscall_init();

    timer_init(100);
    keyboard_init();

    __asm__ volatile("sti");

    /* -------------------------------------------------- */
    /* Embedded ELF binaries from build/*.elf             */
    /* -------------------------------------------------- */

    extern uint8_t _binary_build_shell_elf_start[];
    extern uint8_t _binary_build_shell_elf_end[];

    extern uint8_t _binary_build_hello_elf_start[];
    extern uint8_t _binary_build_hello_elf_end[];

    uint32_t shell_size =
        (uint32_t)(_binary_build_shell_elf_end -
                   _binary_build_shell_elf_start);

    uint32_t hello_size =
        (uint32_t)(_binary_build_hello_elf_end -
                   _binary_build_hello_elf_start);

    fs_kwrite("shell",
              _binary_build_shell_elf_start,
              shell_size);

    fs_kwrite("hello",
              _binary_build_hello_elf_start,
              hello_size);

    vga_puts("Programs in ramfs: shell, hello\n\n");

    /* -------------------------------------------------- */
    /* Spawn first userspace process                      */
    /* -------------------------------------------------- */
    vga_puts("ELF=");
    vga_puthex(*(uint32_t *)_binary_build_hello_elf_start);
    vga_puts("\n");

    vga_puts("SHELL=");
    vga_puthex(*(uint32_t *)_binary_build_shell_elf_start);
    vga_puts("\n");
    struct proc *sh =
        exec_elf(_binary_build_hello_elf_start, 0);

    if (!sh) {
        vga_puts("Failed to spawn shell\n");
        for (;;)
            __asm__ volatile("hlt");
    }

    scheduler_request();

    for (;;) {
        scheduler_run();
        __asm__ volatile("hlt");
    }
}