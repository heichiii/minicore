#ifndef MINICORE_KERNEL_LOG_H
#define MINICORE_KERNEL_LOG_H

#include <stddef.h>

void log_init(void);
void log_write(const char *data, size_t size);
size_t log_read(char *data, size_t capacity);

#endif
