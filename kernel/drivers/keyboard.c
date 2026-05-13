#include "types.h"
#include "idt.h"
#include "kbd_buf.h"

#define KB_DATA_PORT  0x60

static const char sc_ascii[] = {
      0,    0,  '1', '2', '3', '4', '5', '6',
    '7',  '8', '9', '0', '-', '=', '\b', '\t',
    'q',  'w', 'e', 'r', 't', 'y', 'u', 'i',
    'o',  'p', '[', ']', '\n',  0,  'a', 's',
    'd',  'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`',  0, '\\','z', 'x', 'c', 'v',
    'b',  'n', 'm', ',', '.', '/',  0,  '*',
      0,  ' '
};

#define SC_MAX ((uint8_t)(sizeof(sc_ascii) / sizeof(sc_ascii[0])))


static void kb_handler(regs_t *r) {
    (void)r;

    uint8_t sc;
    __asm__ volatile("inb %1, %0" : "=a"(sc) : "Nd"((uint16_t)KB_DATA_PORT));

    if (sc & 0x80) return;   /* key release — ignore */

    if (sc < SC_MAX && sc_ascii[sc]){
        kbd_buf_push(sc_ascii[sc]);
    }
}

void keyboard_init(void) {
    irq_install_handler(1, kb_handler);
}