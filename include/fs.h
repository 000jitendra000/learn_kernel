#ifndef FS_H
#define FS_H

#include "types.h"

#define FS_MAX_FILES  16
#define FS_MAX_NAME   32
#define FS_MAX_SIZE   4096
#define FS_MAX_HANDLES 64

/* ── file object — kernel-internal, persistent ───────────────────────────── */
typedef struct {
    char     name[FS_MAX_NAME];
    uint8_t  data[FS_MAX_SIZE];
    uint32_t size;
    uint32_t refs;   /* reference count — object lives while refs > 0 */
    int      used;
} fs_file_t;

/* ── file handle — per-open, owns the cursor ─────────────────────────────── */
typedef struct {
    fs_file_t *file;   /* pointer to underlying file object */
    uint32_t   pos;    /* per-open read/write cursor */
    int        used;
} file_handle_t;

/* ── file object API (kernel internal) ───────────────────────────────────── */
void        fs_init(void);
fs_file_t  *fs_create_or_open(const char *name);   /* incr refs */

/* ── file handle API ─────────────────────────────────────────────────────── */
file_handle_t *fh_alloc(fs_file_t *f);             /* alloc handle, incr refs */
void           fh_free(file_handle_t *fh);          /* decr refs, release handle */
int            fh_read(file_handle_t *fh, uint8_t *buf, uint32_t len);
int            fh_write(file_handle_t *fh, const uint8_t *buf, uint32_t len);

/* ── kernel convenience (bypass handle layer) ────────────────────────────── */
int fs_kwrite(const char *name, const uint8_t *buf, uint32_t len);
fs_file_t *fs_find(const char *name);   /* find file by name, no ref change */

#endif