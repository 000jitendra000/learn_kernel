#include "types.h"
#include "timer.h"
#include "process.h"
#include "task.h"
#include "idt.h"

extern void vga_putchar(char c);

static volatile uint32_t tick_count = 0;

void timer_handler(regs_t *r) {
    (void)r;

    tick_count++;
}

uint32_t timer_ticks(void) {
    return tick_count;
}

void timer_init(uint32_t hz) {

    irq_install_handler(0, timer_handler);

    uint32_t divisor = 1193180 / hz;

    __asm__ volatile("outb %0, $0x43" : : "a"((uint8_t)0x36));

    uint8_t lo = (uint8_t)(divisor & 0xFF);
    uint8_t hi = (uint8_t)((divisor >> 8) & 0xFF);

    __asm__ volatile("outb %0, $0x40" : : "a"(lo));
    __asm__ volatile("outb %0, $0x40" : : "a"(hi));
}