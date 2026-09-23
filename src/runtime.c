#include "runtime.h"

void *memcpy(void *restrict dst, const void *restrict src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;

    while (n--)
        *d++ = *s++;
    return dst;
}

int memcmp(const void *left, const void *right, size_t n)
{
    const unsigned char *a = left;
    const unsigned char *b = right;

    while (n--) {
        if (*a != *b)
            return *a < *b ? -1 : 1;
        ++a;
        ++b;
    }
    return 0;
}

void *memmove(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;

    if (d < s) {
        while (n--)
            *d++ = *s++;
    } else if (d > s) {
        d += n;
        s += n;
        while (n--)
            *--d = *--s;
    }
    return dst;
}

void *memset(void *dst, int value, size_t n)
{
    unsigned char *d = dst;

    while (n--)
        *d++ = (unsigned char)value;
    return dst;
}

size_t strlen(const char *s)
{
    size_t n = 0;

    while (s[n])
        ++n;
    return n;
}
