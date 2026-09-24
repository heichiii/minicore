#include "platform.h"

#include "../mm/layout.h"

#define UART0 ((volatile uint8_t *)(KERNEL_DIRECT_BASE + UINT64_C(0x10000000)))
#define QEMU_TEST \
    ((volatile uint32_t *)(KERNEL_DIRECT_BASE + UINT64_C(0x00100000)))

void console_putc(char ch)
{
    if (ch == '\n')
        console_putc('\r');
    while ((UART0[5] & (1 << 5)) == 0) {
    }
    UART0[0] = (uint8_t)ch;
}

void console_puts(const char *s)
{
    while (*s)
        console_putc(*s++);
}

void console_puthex(uint64_t value)
{
    static const char digits[] = "0123456789abcdef";

    console_puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4)
        console_putc(digits[(value >> shift) & 0xf]);
}

_Noreturn void platform_exit(bool success)
{
    *QEMU_TEST = success ? 0x5555 : 0x3333;
    for (;;)
        __asm__ volatile("wfi");
}
