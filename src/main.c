#include <stddef.h>
#include <stdint.h>

#include "arch/riscv/sbi.h"
#include "arch/riscv/csr.h"
#include "arch/riscv/trap.h"
#include "kernel/user.h"
#include "kernel/log.h"
#include "kernel/m4_test.h"
#include "kernel/scheduler.h"
#include "mm/layout.h"
#include "mm/page_alloc.h"
#include "mm/vm.h"
#include "mm/heap.h"
#include "platform/dtb.h"
#include "platform/plic.h"
#include "platform/platform.h"
#include "runtime.h"

extern char __kernel_start[], __text_end[], __rodata_end[];
extern char __data_end[], __bss_start[], __bss_end[], __kernel_end[];
extern char __text_start[], __boot_stack_guard[];

static uint64_t bss_test;
static struct boot_info boot_info;
static struct page_table kernel_page_table;

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

static bool run_vm_tests(void)
{
    const uint64_t test_virtual = UINT64_C(0xffffffd000000000);
    struct page_table temporary;
    size_t baseline = page_free_count();
    uint64_t page = page_alloc();
    uint64_t physical;
    uint64_t flags;

    if (!page || !vm_create(&temporary))
        return false;
    if (!vm_map(&temporary, test_virtual, page, PTE_R | PTE_W) ||
        !vm_query(&temporary, test_virtual + 17, &physical, &flags) ||
        physical != page + 17 || (flags & (PTE_R | PTE_W | PTE_A | PTE_D)) !=
                                     (PTE_R | PTE_W | PTE_A | PTE_D) ||
        !vm_protect(&temporary, test_virtual, PTE_R) ||
        !vm_unmap(&temporary, test_virtual))
        return false;
    vm_destroy(&temporary);
    page_free(page);
    if (page_free_count() != baseline)
        return false;

    page = page_alloc();
    if (!page || !vm_map(&kernel_page_table, test_virtual, page,
                         PTE_R | PTE_W))
        return false;
    *(volatile uint64_t *)(uintptr_t)test_virtual = UINT64_C(0x123456789abcdef0);
    if (*(volatile uint64_t *)(uintptr_t)test_virtual !=
            UINT64_C(0x123456789abcdef0) ||
        !vm_unmap(&kernel_page_table, test_virtual) ||
        !trap_expect_page_fault((volatile uint64_t *)(uintptr_t)test_virtual,
                                true))
        return false;
    page_free(page);

    if (!trap_expect_page_fault((volatile uint64_t *)__text_start, true) ||
        !trap_expect_page_fault((volatile uint64_t *)__boot_stack_guard,
                                false) ||
        page_free_count() != baseline)
        return false;
    return true;
}

_Noreturn void kernel_main(uint64_t hart_id, const void *dtb)
{
    enum dtb_error error;
    char test[4];

    console_puts("\nMiniCore M4\nhart = ");
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
    boot_info.dtb_base = virt_to_phys((const void *)boot_info.dtb_base);

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
    if (!plic_init(&boot_info, hart_id))
        panic("cannot initialize PLIC interface");
    if (!sbi_probe_extension(SBI_EXT_TIME))
        panic("SBI TIME extension is unavailable");
    trap_init();
    if (!trap_run_m1_tests(boot_info.timebase_frequency))
        panic("trap/timer self-test failed");
    console_puts("M1 TRAP PASS\n");
    console_puts("M1 PASS\n");

    if (!page_allocator_init(&boot_info, virt_to_phys(__kernel_start),
                             virt_to_phys(__kernel_end), boot_info.dtb_base,
                             boot_info.dtb_size))
        panic("cannot initialize physical page allocator");
    console_puts("free pages = ");
    console_puthex(page_free_count());
    console_putc('\n');
    if (!vm_build_kernel(&kernel_page_table, &boot_info))
        panic("cannot build kernel page table");
    vm_activate(&kernel_page_table);
    if ((csr_read_satp() >> 60) != 8)
        panic("Sv39 is not active");
    console_puts("M2 SATP PASS\n");
    if (!run_vm_tests())
        panic("physical page/Sv39 self-test failed");
    console_puts("M2 VM PASS\n");
    console_puts("M2 PASS\n");
    heap_init();
    log_init();
    if (!scheduler_init(boot_info.timebase_frequency))
        panic("cannot initialize scheduler");
    if (!user_run_m3_tests(&kernel_page_table))
        panic("U-mode/syscall self-test failed");
    console_puts("M3 COPY PASS\n");
    console_puts("M3 ISOLATION PASS\n");
    console_puts("M3 PASS\n");
    if (!run_m4_tests())
        panic("M4 scheduler/synchronization self-test failed");
    console_puts("M4 THREAD PASS\n");
    console_puts("M4 PREEMPT PASS\n");
    console_puts("M4 WAIT PASS\n");
    console_puts("M4 PASS\n");
    scheduler_stop_timer();
    platform_exit(true);
}
