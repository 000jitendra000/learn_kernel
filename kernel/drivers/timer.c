#include "types.h"
#include "idt.h"

#define PIT_CHANNEL0  0x40
#define PIT_CMD       0x43
#define PIT_BASE_HZ   1193180

static volatile uint32_t tick = 0;

extern void scheduler_request(void);

uint32_t timer_ticks(void) {
    return tick;
}

static void timer_handler(regs_t *r) {
    (void)r;
    tick++;
    scheduler_request();    /* signal scheduler — deferred switch in normal context */
}

void timer_init(uint32_t hz) {
    if (hz == 0)       hz = 100;
    if (hz > PIT_BASE_HZ) hz = PIT_BASE_HZ;

    uint16_t divisor = (uint16_t)(PIT_BASE_HZ / hz);

    irq_install_handler(0, timer_handler);

    __asm__ volatile("outb %0, %1" :: "a"((uint8_t)0x36),             "Nd"((uint16_t)PIT_CMD));
    __asm__ volatile("outb %0, %1" :: "a"((uint8_t)(divisor & 0xFF)), "Nd"((uint16_t)PIT_CHANNEL0));
    __asm__ volatile("outb %0, %1" :: "a"((uint8_t)(divisor >> 8)),   "Nd"((uint16_t)PIT_CHANNEL0));
}