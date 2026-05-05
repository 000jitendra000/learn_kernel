#ifndef HEAP_H
#define HEAP_H

#include "types.h"

#define HEAP_START  0x00600000
#define HEAP_SIZE   0x00200000   /* 2 MB */
#define HEAP_END    (HEAP_START + HEAP_SIZE)

void  heap_init(void);
void *kmalloc(uint32_t size);
void  kfree(void *ptr);

#endif