#ifndef MINICORE_PLATFORM_H
#define MINICORE_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

void console_putc(char ch);
int console_getc(void); /* Nonblocking byte read, or -1 when UART RX is empty. */
void console_puts(const char *s);
void console_puthex(uint64_t value);
_Noreturn void platform_exit(bool success);

#endif
