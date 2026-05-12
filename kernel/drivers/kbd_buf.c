#include "types.h"
#include "kbd_buf.h"
#include "task.h"

#define KBD_BUF_SIZE 256

static char     raw_buf[KBD_BUF_SIZE];
static uint32_t raw_head = 0;
static uint32_t raw_tail = 0;

static char     line_buf[KBD_BUF_SIZE];
static uint32_t line_len  = 0;
static int      line_ready = 0;

static task_t *kbd_waiter = (task_t *)0;   /* Phase 25 */

void kbd_buf_init(void) {
    raw_head  = 0;
    raw_tail  = 0;
    line_len  = 0;
    line_ready = 0;
    kbd_waiter = (task_t *)0;
}

void kbd_set_waiter(task_t *t) {
    kbd_waiter = t;
}

/*
 * kbd_buf_push — called from the keyboard IRQ handler with the decoded char.
 *
 * Appends to line_buf; on newline, marks line_ready and wakes any
 * blocked reader.
 */
void kbd_buf_push(char c) {
    /* echo to VGA omitted here — handled in IRQ layer if desired */

    if (c == '\b') {
        if (line_len > 0) line_len--;
        return;
    }

    if (line_len < KBD_BUF_SIZE - 1) {
        line_buf[line_len++] = c;
    }

    if (c == '\n') {
        line_ready = 1;

        /* Phase 25: wake blocked reader */
        if (kbd_waiter) {
            kbd_waiter = (task_t *)0;
        }
    }
}

int kbd_line_ready(void) {
    return line_ready;
}

/*
 * kbd_line_read — copy completed line into caller's buffer.
 * Resets internal line state.  Returns bytes copied.
 */
int kbd_line_read(char *buf, uint32_t len) {
    if (!line_ready) return 0;

    uint32_t copy = (len < line_len) ? len : line_len;
    uint32_t i;
    for (i = 0; i < copy; i++)
        buf[i] = line_buf[i];

    /* strip trailing newline */
    if (copy > 0 && buf[copy - 1] == '\n')
        copy--;

    /* null terminate */
    buf[copy] = '\0';

    line_len   = 0;
    line_ready = 0;

    return (int)copy;
}