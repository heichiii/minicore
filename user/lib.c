#include "lib.h"

long call(long number, long a, long b, long c, long d, long e, long f)
{
    register long a0 __asm__("a0") = a;
    register long a1 __asm__("a1") = b;
    register long a2 __asm__("a2") = c;
    register long a3 __asm__("a3") = d;
    register long a4 __asm__("a4") = e;
    register long a5 __asm__("a5") = f;
    register long a7 __asm__("a7") = number;
    __asm__ volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3),
                     "r"(a4), "r"(a5), "r"(a7) : "memory");
    return a0;
}

void exit(long status)
{
    sc1(SYS_EXIT, status);
    for (;;) { }
}

/* Pipes and bounded tmpfs writes may return a prefix; callers that promise
 * complete output must retry using only the unconsumed suffix. */
int write_all(long fd, const void *buffer, size_t size)
{
    const char *p = buffer;
    while (size) {
        long n = write(fd, p, size);
        if (n <= 0) return -1;
        p += n;
        size -= (size_t)n;
    }
    return 0;
}

int puts(const char *s) { return write_all(1, s, strlen(s)); }
int equal(const char *a, const char *b)
{
    return strlen(a) == strlen(b) && !memcmp(a, b, strlen(a));
}
