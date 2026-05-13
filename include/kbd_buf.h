#ifndef KBD_BUF_H
#define KBD_BUF_H

#include "types.h"
#include "task.h"

void kbd_buf_init(void);
void kbd_buf_push(char c);      /* called from keyboard IRQ */
int  kbd_line_ready(void);
int  kbd_line_read(char *buf, uint32_t len);
int kbd_buf_get(void);

void kbd_set_waiter(task_t *t); /* Phase 25: register task to wake on newline */

#endif