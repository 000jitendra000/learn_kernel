#ifndef PIPE_H
#define PIPE_H

#include "types.h"
#include "task.h"      /* Phase 25: task_t for waiters */

#define PIPE_MAX      16
#define PIPE_BUF_SIZE 1024

typedef enum {
    PIPE_READ  = 0,
    PIPE_WRITE = 1
} pipe_end_t;

typedef struct {
    char     buf[PIPE_BUF_SIZE];
    uint32_t read_pos;
    uint32_t write_pos;
    uint32_t len;
    uint32_t writer_count;
    uint32_t reader_count;
    task_t  *reader_waiter;   /* Phase 25: task blocked waiting for data */
    task_t  *writer_waiter;   /* Phase 25: task blocked waiting for space */
} pipe_t;

typedef struct {
    pipe_t    *pipe;
    pipe_end_t end;
} pipe_handle_t;

void           pipe_init(void);
pipe_t        *pipe_alloc(void);
pipe_handle_t *pipe_open_end(pipe_t *p, pipe_end_t end);
void           pipe_close_end(pipe_handle_t *ph);
int            pipe_read(pipe_handle_t *ph, char *buf, uint32_t len);
int            pipe_write(pipe_handle_t *ph, const char *buf, uint32_t len);

#endif