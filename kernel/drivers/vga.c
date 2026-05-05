#include "../../include/types.h"

#define VGA_BASE   ((volatile uint16_t *)0xB8000)
#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_ATTR   0x0F          /* white on black */

static uint32_t col = 0;
static uint32_t row = 0;

static void vga_write(uint32_t r, uint32_t c, char ch, uint8_t attr) {
    VGA_BASE[r * VGA_WIDTH + c] = (uint16_t)((attr << 8) | (uint8_t)ch);
}

static void scroll(void) {
    uint32_t r, c;
    for (r = 1; r < VGA_HEIGHT; r++)
        for (c = 0; c < VGA_WIDTH; c++)
            VGA_BASE[(r - 1) * VGA_WIDTH + c] = VGA_BASE[r * VGA_WIDTH + c];
    for (c = 0; c < VGA_WIDTH; c++)
        vga_write(VGA_HEIGHT - 1, c, ' ', VGA_ATTR);
    row = VGA_HEIGHT - 1;
}

void vga_clear(void) {
    uint32_t r, c;
    for (r = 0; r < VGA_HEIGHT; r++)
        for (c = 0; c < VGA_WIDTH; c++)
            vga_write(r, c, ' ', VGA_ATTR);
    col = 0;
    row = 0;
}

void vga_putchar(char ch) {
    if (ch == '\n') {
        col = 0;
        if (++row >= VGA_HEIGHT) scroll();
        return;
    }
    if (ch == '\r') {
        col = 0;
        return;
    }
    if (ch == '\b') {
        if (col > 0) {
            col--;
            vga_write(row, col, ' ', VGA_ATTR);
        }
    return;
}
    vga_write(row, col, ch, VGA_ATTR);
    if (++col >= VGA_WIDTH) {
        col = 0;
        if (++row >= VGA_HEIGHT) scroll();
    }
}

void vga_puts(const char *s) {
    while (*s)
        vga_putchar(*s++);
}

void vga_puthex(uint32_t val) {
    const char *hex = "0123456789ABCDEF";
    int i;
    vga_puts("0x");
    for (i = 28; i >= 0; i -= 4)
        vga_putchar(hex[(val >> i) & 0xF]);
}