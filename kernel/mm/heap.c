#include "heap.h"
#include "types.h"

typedef struct block {
    uint32_t      size;   /* usable bytes after header */
    uint32_t      free;   /* 1 = free, 0 = used */
    struct block *next;   /* next block in list */
} block_t;

#define BLOCK_HDR  sizeof(block_t)
#define MIN_SPLIT  (BLOCK_HDR + 8)   /* min leftover to justify a split */

static block_t *heap_head = (block_t *)HEAP_START;

void heap_init(void) {
    heap_head->size = HEAP_SIZE - BLOCK_HDR;
    heap_head->free = 1;
    heap_head->next = (block_t *)0;
}

void *kmalloc(uint32_t size) {
    if (!size)
        return (void *)0;

    /* requirement 2: 8-byte alignment */
    size = (size + 7) & ~7u;

    block_t *cur = heap_head;
    while (cur) {
        if (cur->free && cur->size >= size) {
            /* split only if remainder can hold a header + minimum payload */
            if (cur->size >= size + MIN_SPLIT) {
                block_t *split = (block_t *)((uint8_t *)(cur + 1) + size);
                split->size = cur->size - size - BLOCK_HDR;
                split->free = 1;
                split->next = cur->next;
                cur->next   = split;
                cur->size   = size;
            }
            cur->free = 0;
            return (void *)(cur + 1);
        }
        cur = cur->next;
    }

    return (void *)0;   /* out of heap */
}

void kfree(void *ptr) {
    if (!ptr)
        return;

    block_t *blk = (block_t *)ptr - 1;
    blk->free = 1;

    /* coalesce forward — merge with consecutive free blocks */
    while (blk->next && blk->next->free) {
        blk->size += BLOCK_HDR + blk->next->size;
        blk->next  = blk->next->next;
    }
}