#ifndef MINICORE_MM_HEAP_H
#define MINICORE_MM_HEAP_H

#include <stddef.h>

void heap_init(void);
void *kmalloc(size_t size);
void kfree(void *pointer);

#endif
