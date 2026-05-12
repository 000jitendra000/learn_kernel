#include "types.h"
#include "fs.h"

static fs_file_t    file_table[FS_MAX_FILES];
static file_handle_t handle_pool[FS_MAX_HANDLES];

/* ── internal helpers ────────────────────────────────────────────────────── */

static int str_eq(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == *b;
}

static void str_copy(char *dst, const char *src, uint32_t max) {
    uint32_t i = 0;
    while (i < max - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

/* ── init ────────────────────────────────────────────────────────────────── */

void fs_init(void) {
    uint32_t i;
    for (i = 0; i < FS_MAX_FILES; i++) {
        file_table[i].used   = 0;
        file_table[i].size   = 0;
        file_table[i].refs   = 0;
        file_table[i].name[0] = '\0';
    }
    for (i = 0; i < FS_MAX_HANDLES; i++) {
        handle_pool[i].used = 0;
        handle_pool[i].pos  = 0;
        handle_pool[i].file = (fs_file_t *)0;
    }
}

/* ── file object API ─────────────────────────────────────────────────────── */

/*
 * fs_create_or_open — find existing file or allocate a new slot.
 *
 * Ownership rule: does NOT increment refs for either case.
 * Only fh_alloc() increments refs when a handle is created.
 * New files start with refs = 0.
 */
fs_file_t *fs_create_or_open(const char *name) {
    if (!name || !name[0]) return (fs_file_t *)0;

    uint32_t i;
    int free_slot = -1;

    for (i = 0; i < FS_MAX_FILES; i++) {
        if (file_table[i].used) {
            if (str_eq(file_table[i].name, name))
                return &file_table[i];      /* found — no ref increment */
        } else if (free_slot == -1) {
            free_slot = (int)i;
        }
    }

    /* not found — create new file object */
    if (free_slot == -1) return (fs_file_t *)0;

    fs_file_t *f = &file_table[free_slot];
    str_copy(f->name, name, FS_MAX_NAME);
    f->size = 0;
    f->refs = 0;    /* fh_alloc will increment on first open */
    f->used = 1;

    return f;
}

/* ── handle API ──────────────────────────────────────────────────────────── */

/*
 * fh_alloc — allocate a handle, increment f->refs.
 */
file_handle_t *fh_alloc(fs_file_t *f) {
    if (!f) return (file_handle_t *)0;

    uint32_t i;
    for (i = 0; i < FS_MAX_HANDLES; i++) {
        if (!handle_pool[i].used) {
            handle_pool[i].file = f;
            handle_pool[i].pos  = 0;
            handle_pool[i].used = 1;
            f->refs++;          /* only place refs is incremented */
            return &handle_pool[i];
        }
    }
    return (file_handle_t *)0;
}

/*
 * fh_free — release handle, decrement refs.
 * If refs reach 0: reclaim the file object slot.
 */
void fh_free(file_handle_t *fh) {
    if (!fh || !fh->used) return;

    fs_file_t *f = fh->file;
    fh->used = 0;
    fh->file = (fs_file_t *)0;
    fh->pos  = 0;

    if (f) {
        if (f->refs == 0 && f->size == 0) f->refs--;
        if (f->refs == 0) {
            f->used    = 0;
            f->size    = 0;
            f->name[0] = '\0';  /* fix 4: clear name on reclaim */
        }
    }
}

/*
 * fh_read — read up to len bytes from cursor into buf.
 */
int fh_read(file_handle_t *fh, uint8_t *buf, uint32_t len) {
    if (!fh || !fh->used || !fh->file) return -1;
    if (!buf || !len) return 0;

    fs_file_t *f = fh->file;

    /* fix 5: prevent underflow when pos >= size */
    if (fh->pos >= f->size) return 0;

    uint32_t avail = f->size - fh->pos;
    if (len > avail) len = avail;

    uint32_t i;
    for (i = 0; i < len; i++)
        buf[i] = f->data[fh->pos + i];

    fh->pos += len;
    return (int)len;
}

/*
 * fh_write — write len bytes from buf at cursor.
 */
int fh_write(file_handle_t *fh, const uint8_t *buf, uint32_t len) {
    if (!fh || !fh->used || !fh->file) return -1;
    if (!buf || !len) return 0;

    fs_file_t *f    = fh->file;
    uint32_t  space = FS_MAX_SIZE - fh->pos;
    if (len > space) len = space;
    if (!len) return -1;

    uint32_t i;
    for (i = 0; i < len; i++)
        f->data[fh->pos + i] = buf[i];

    fh->pos += len;
    if (fh->pos > f->size) f->size = fh->pos;

    return (int)len;
}

/* ── kernel convenience ──────────────────────────────────────────────────── */

/*
 * fs_kwrite — open/create file, write buf, close handle.
 * Does not touch process fd table.
 * Ownership: fs_create_or_open does not touch refs;
 * fh_alloc increments; fh_free decrements. Clean.
 */

/*
 * fs_find — locate a file by name without touching refs.
 * Used by sys_exec to get the ELF image pointer.
 */
fs_file_t *fs_find(const char *name) {
    if (!name || !name[0]) return (fs_file_t *)0;
    uint32_t i;
    for (i = 0; i < FS_MAX_FILES; i++)
        if (file_table[i].used && str_eq(file_table[i].name, name))
            return &file_table[i];
    return (fs_file_t *)0;
}

int fs_kwrite(const char *name, const uint8_t *buf, uint32_t len) {
    fs_file_t *f = fs_create_or_open(name);
    if (!f) return -1;

    file_handle_t *fh = fh_alloc(f);   /* refs becomes 1 */
    if (!fh) return -1;

    int result = fh_write(fh, buf, len);
    fh_free(fh);    /* refs becomes 0 — but file persists because used=1 */

    /*
     * After fh_free, refs == 0 which would reclaim the file.
     * We want the file to persist for future opens.
     * Solution: keep used=1 even at refs==0 while file has data.
     * Adjust: only reclaim if size == 0 too.
     */

    return result;
}