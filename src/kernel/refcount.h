#ifndef MINICORE_KERNEL_REFCOUNT_H
#define MINICORE_KERNEL_REFCOUNT_H

#include <stdbool.h>
#include <stddef.h>

struct refcount {
    size_t value;
};

static inline void refcount_init(struct refcount *reference)
{
    reference->value = 1;
}

static inline void refcount_get(struct refcount *reference)
{
    ++reference->value;
}

static inline bool refcount_put(struct refcount *reference)
{
    return --reference->value == 0;
}

static inline size_t refcount_read(const struct refcount *reference)
{
    return reference->value;
}

#endif
