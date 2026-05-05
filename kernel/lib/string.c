#include "../../include/types.h"

size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

void *memset(void *ptr, int val, size_t n) {
    uint8_t *p = (uint8_t *)ptr;
    while (n--) *p++ = (uint8_t)val;
    return ptr;
}

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
    return dst;
}