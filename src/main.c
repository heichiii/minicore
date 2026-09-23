#include <stddef.h>
#include <stdint.h>

#include "platform/dtb.h"
#include "platform/platform.h"
#include "runtime.h"

extern char __kernel_start[], __text_end[], __rodata_end[];
extern char __data_end[], __bss_start[], __bss_end[], __kernel_end[];

static uint64_t bss_test;
static struct boot_info boot_info;

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

static void print_range(const char *name, const struct physical_range *range)
{
    console_puts(name);
    console_puts(": base=");
    console_puthex(range->base);
    console_puts(" size=");
    console_puthex(range->size);
    console_putc('\n');
}

_Noreturn void kernel_main(uint64_t hart_id, const void *dtb)
{
    enum dtb_error error;
    char test[4];

    console_puts("\nMiniCore M1\nhart = ");
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

    error = dtb_parse(dtb, &boot_info);
    if (error != DTB_OK) {
        console_puts("DTB: ");
        console_puts(dtb_error_string(error));
        console_putc('\n');
        panic("cannot parse DTB");
    }

    console_puts("dtb size = ");
    console_puthex(boot_info.dtb_size);
    console_puts("\ntimebase = ");
    console_puthex(boot_info.timebase_frequency);
    console_putc('\n');
    for (size_t i = 0; i < boot_info.memory_count; ++i)
        print_range("ram", &boot_info.memory[i]);
    for (size_t i = 0; i < boot_info.reserved_count; ++i)
        print_range("reserved", &boot_info.reserved[i]);
    if (boot_info.uart_present)
        print_range("uart", &boot_info.uart);
    if (boot_info.plic_present)
        print_range("plic", &boot_info.plic);
    for (size_t i = 0; i < boot_info.virtio_count; ++i)
        print_range("virtio", &boot_info.virtio[i]);

    if (boot_info.memory_count == 0 || boot_info.timebase_frequency == 0 ||
        !boot_info.uart_present || !boot_info.plic_present ||
        boot_info.uart_irq == 0 || boot_info.plic_source_count == 0 ||
        boot_info.virtio_count == 0)
        panic("DTB is missing required QEMU virt hardware");

    console_puts("M1 DTB PASS\n");
    platform_exit(true);
}
