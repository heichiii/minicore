#include <stddef.h>
#include <stdint.h>

#include "platform/platform.h"
#include "runtime.h"

extern char __kernel_start[], __text_end[], __rodata_end[];
extern char __data_end[], __bss_start[], __bss_end[], __kernel_end[];

static uint64_t bss_test;

static _Noreturn void panic(const char *message)
{
    console_puts("PANIC: ");
    console_puts(message);
    console_putc('\n');
    platform_exit(false);
}

_Noreturn void panic_returned_from_kernel_main(void)
{
    panic("kernel_main returned");
}

static void address(const char *name, const void *value)
{
    console_puts(name);
    console_puthex((uintptr_t)value);
    console_putc('\n');
}

_Noreturn void kernel_main(uint64_t hart_id, const void *dtb)
{
    char test[4];

    console_puts("\nMiniCore M0\nhart = ");
    console_puthex(hart_id);
    console_puts("\ndtb  = ");
    console_puthex((uintptr_t)dtb);
    console_putc('\n');

    address("kernel = ", __kernel_start);
    address("text   = ", __text_end);
    address("rodata = ", __rodata_end);
    address("data   = ", __data_end);
    address("bss    = ", __bss_start);
    address("bss end= ", __bss_end);
    address("end    = ", __kernel_end);

    memset(test, 0, sizeof(test));
    memcpy(test, "M0", 3);
    memmove(test + 1, test, 3);
    if (!dtb || bss_test != 0 || strlen(test + 1) != 2)
        panic("self-test failed");

    console_puts("M0 PASS\n");
    platform_exit(true);
}
