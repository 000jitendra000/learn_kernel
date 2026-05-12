#include "types.h"
#include "pipe.h"
#include "process.h"
#include "signal.h"
#include "task.h"

extern task_t *current_task;
extern void scheduler_request(void);
extern void scheduler_run(void);

static pipe_t        pipe_pool[PIPE_MAX];
static pipe_handle_t pipe_handle_pool[PIPE_MAX * 2];

void pipe_init(void) {
    uint32_t i;
    for (i = 0; i < PIPE_MAX; i++) {
        pipe_pool[i].read_pos      = 0;
        pipe_pool[i].write_pos     = 0;
        pipe_pool[i].len           = 0;
        pipe_pool[i].reader_count  = 0;
        pipe_pool[i].writer_count  = 0;
        pipe_pool[i].reader_waiter = (task_t *)0;   /* Phase 25 */
        pipe_pool[i].writer_waiter = (task_t *)0;   /* Phase 25 */
    }
    for (i = 0; i < PIPE_MAX * 2; i++)
        pipe_handle_pool[i].pipe = (pipe_t *)0;
}

pipe_t *pipe_alloc(void) {
    uint32_t i;
    for (i = 0; i < PIPE_MAX; i++) {
        pipe_t *p = &pipe_pool[i];
        if (p->reader_count == 0 && p->writer_count == 0) {
            p->read_pos      = 0;
            p->write_pos     = 0;
            p->len           = 0;
            p->reader_waiter = (task_t *)0;
            p->writer_waiter = (task_t *)0;
            return p;
        }
    }
    return (pipe_t *)0;
}

pipe_handle_t *pipe_open_end(pipe_t *p, pipe_end_t end) {
    if (!p) return (pipe_handle_t *)0;
    uint32_t i;
    for (i = 0; i < PIPE_MAX * 2; i++) {
        if (!pipe_handle_pool[i].pipe) {
            pipe_handle_pool[i].pipe = p;
            pipe_handle_pool[i].end  = end;
            if (end == PIPE_READ)  p->reader_count++;
            if (end == PIPE_WRITE) p->writer_count++;
            return &pipe_handle_pool[i];
        }
    }
    return (pipe_handle_t *)0;
}

void pipe_close_end(pipe_handle_t *ph) {
    if (!ph || !ph->pipe) return;
    pipe_t *p = ph->pipe;

    if (ph->end == PIPE_READ  && p->reader_count > 0) p->reader_count--;
    if (ph->end == PIPE_WRITE && p->writer_count > 0) p->writer_count--;

    /*
     * Phase 25: if the last writer closes, wake any blocked reader so it
     * can observe EOF.  If the last reader closes, wake any blocked writer
     * so it can observe the broken pipe and get SIGPIPE.
     */
    if (ph->end == PIPE_WRITE && p->writer_count == 0 && p->reader_waiter) {
        task_wake(p->reader_waiter);
        p->reader_waiter = (task_t *)0;
    }
    if (ph->end == PIPE_READ && p->reader_count == 0 && p->writer_waiter) {
        task_wake(p->writer_waiter);
        p->writer_waiter = (task_t *)0;
    }

    ph->pipe = (pipe_t *)0;
}

/*
 * pipe_read — read up to len bytes from the pipe into buf.
 *
 * Blocks (via task_block + scheduler_run) when empty and writers exist.
 * Returns 0 on EOF (empty + no writers).
 */
int pipe_read(pipe_handle_t *ph, char *buf, uint32_t len) {
    if (!ph || !ph->pipe || ph->end != PIPE_READ) return -1;
    if (!buf || !len) return 0;

    pipe_t *p = ph->pipe;

    while (p->len == 0) {
        if (p->writer_count == 0) return 0;   /* EOF */

        /* block until a writer deposits data */
        p->reader_waiter = current_task;
        task_block(current_task);
        scheduler_request();
        scheduler_run();
        /* woken by pipe_write or pipe_close_end */
    }

    uint32_t copy = (len < p->len) ? len : p->len;
    uint32_t i;
    for (i = 0; i < copy; i++) {
        buf[i]      = p->buf[p->read_pos];
        p->read_pos = (p->read_pos + 1) % PIPE_BUF_SIZE;
    }
    p->len -= copy;

    /* wake a blocked writer now that there is space */
    if (p->writer_waiter) {
        task_wake(p->writer_waiter);
        p->writer_waiter = (task_t *)0;
    }

    return (int)copy;
}

/*
 * pipe_write — append up to len bytes into the pipe.
 *
 * Blocks when the buffer is full.
 * Delivers SIGPIPE and returns -1 when no readers remain.
 */
int pipe_write(pipe_handle_t *ph, const char *buf, uint32_t len) {
    if (!ph || !ph->pipe || ph->end != PIPE_WRITE) return -1;
    if (!buf || !len) return 0;

    pipe_t *p = ph->pipe;

    if (p->reader_count == 0) {
        proc_signal(proc_current(), SIGPIPE);
        return -1;
    }

    uint32_t written = 0;

    while (written < len) {

        /* re-check for broken pipe after each potential sleep */
        if (p->reader_count == 0) {
            proc_signal(proc_current(), SIGPIPE);
            return (written > 0) ? (int)written : -1;
        }

        /* block while the buffer is full */
        while (p->len == PIPE_BUF_SIZE) {
            p->writer_waiter = current_task;
            task_block(current_task);
            scheduler_request();
            scheduler_run();
            /* woken by pipe_read */

            if (p->reader_count == 0) {
                proc_signal(proc_current(), SIGPIPE);
                return (written > 0) ? (int)written : -1;
            }
        }

        p->buf[p->write_pos] = buf[written];
        p->write_pos = (p->write_pos + 1) % PIPE_BUF_SIZE;
        p->len++;
        written++;
    }

    /* wake a blocked reader now that data is available */
    if (p->reader_waiter) {
        task_wake(p->reader_waiter);
        p->reader_waiter = (task_t *)0;
    }

    return (int)written;
}