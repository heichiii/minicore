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

/* 链接脚本导出的地址边界。数组名在表达式中给出符号地址，
 * 可用于打印段布局、保留内核占用的物理页，以及检查段的访问权限。 */
extern char __kernel_start[], __text_end[], __rodata_end[];
extern char __data_end[], __bss_start[], __bss_end[], __kernel_end[];
extern char __text_start[], __boot_stack_guard[];

/* 启动汇编负责清零 .bss，进入 C 后用这个未显式初始化的变量抽查结果。 */
static uint64_t bss_test;
static struct boot_info boot_info;
static struct page_table kernel_page_table;

/* 启动早期也可能失败，因此直接用控制台报告原因，再以失败状态退出。 */
static _Noreturn void panic(const char *message)
{
    console_puts("PANIC: ");
    console_puts(message);
    console_putc('\n');
    platform_exit(false);
}

/* head.S 在调用 kernel_main 的下一条指令处设置了这个兜底入口。 */
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

/* 先用独立页表检查映射接口，再用当前内核页表验证 CPU 的实际访存行为。 */
static bool run_vm_tests(void)
{
    const uint64_t test_virtual = UINT64_C(0xffffffd000000000);
    struct page_table temporary;
    size_t baseline = page_free_count();
    uint64_t page = page_alloc();
    uint64_t physical;
    uint64_t flags;

    /* 临时页表无需激活。查询页内第 17 字节应得到相同的物理偏移；
     * 可写映射还应带有 A（已访问）和 D（已修改）位。随后改为只读并解除映射。 */
    if (!page || !vm_create(&temporary))
        return false;
    if (!vm_map(&temporary, test_virtual, page, PTE_R | PTE_W) ||
        !vm_query(&temporary, test_virtual + 17, &physical, &flags) ||
        physical != page + 17 || (flags & (PTE_R | PTE_W | PTE_A | PTE_D)) !=
                                     (PTE_R | PTE_W | PTE_A | PTE_D) ||
        !vm_protect(&temporary, test_virtual, PTE_R) ||
        !vm_unmap(&temporary, test_virtual))
        return false;
    /* 页表页和被映射的数据页分别回收，两者都归还后空闲页数应恢复。 */
    vm_destroy(&temporary);
    page_free(page);
    if (page_free_count() != baseline)
        return false;

    /* 在活动页表中复用测试地址。volatile 保留实际的读写指令，
     * 先读回写入值，再对已解除映射的地址写入，检查是否触发写缺页异常。 */
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

    /* 正式页表应禁止写代码段，并让启动栈的保护页保持未映射。
     * 分别尝试写和读，要求异常处理程序观察到对应地址和类型的缺页异常。 */
    if (!trap_expect_page_fault((volatile uint64_t *)__text_start, true) ||
        !trap_expect_page_fault((volatile uint64_t *)__boot_stack_guard,
                                false) ||
        page_free_count() != baseline)
        return false;
    return true;
}

/* head.S 切换到高半区启动栈并清零 .bss 后进入这里。hart_id 标识启动
 * 硬件线程，dtb 是设备树在内核直接映射区中的虚拟地址，可直接解引用。 */
_Noreturn void kernel_main(uint64_t hart_id, const void *dtb)
{
    enum dtb_error error;
    char test[4];

    console_puts("\nMiniCore M6\nhart = ");
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

    /* 连同字符串末尾的空字符复制三个字节，再在重叠区间内右移一字节。
     * 右移后的字符串应从 test + 1 开始，长度仍为 2。 */
    memset(test, 0, sizeof(test));
    memcpy(test, "M0", 3);
    memmove(test + 1, test, 3);
    if (!dtb || bss_test != 0 || strlen(test + 1) != 2)
        panic("self-test failed");

    /* 从设备树提取 RAM、保留区、设备寄存器范围和时钟频率。 */
    error = dtb_parse(dtb, &boot_info);
    if (error != DTB_OK) {
        console_puts("DTB: ");
        console_puts(dtb_error_string(error));
        console_putc('\n');
        panic("cannot parse DTB");
    }
    /* 解析器记录的是传入的虚拟地址；页分配器按物理地址排除 DTB 占用页。 */
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

    /* 设备树格式正确后，还要确认当前 QEMU virt 启动流程需要的硬件均已描述。 */
    if (boot_info.memory_count == 0 || boot_info.timebase_frequency == 0 ||
        !boot_info.uart_present || !boot_info.plic_present ||
        boot_info.uart_irq == 0 || boot_info.plic_source_count == 0 ||
        boot_info.virtio_count == 0)
        panic("DTB is missing required QEMU virt hardware");

    console_puts("M1 DTB PASS\n");

    /* 选择当前 hart 的 S 模式 PLIC 上下文，并设置外部中断优先级阈值。 */
    if (!plic_init(&boot_info, hart_id))
        panic("cannot initialize PLIC interface");

    /* 通过 SBI TIME 设置定时器前先确认固件支持。trap_init 安装 S 模式
     * 陷阱入口，随后用同步异常和定时器中断检查异常分派及寄存器保存。 */
    if (!sbi_probe_extension(SBI_EXT_TIME))
        panic("SBI TIME extension is unavailable");
    trap_init();
    if (!trap_run_m1_tests(boot_info.timebase_frequency))
        panic("trap/timer self-test failed");
    console_puts("M1 TRAP PASS\n");
    console_puts("M1 PASS\n");

    /* 从 RAM 中收集可用的完整物理页，跳过固件、内核镜像、DTB 和
     * 设备树声明的保留区。链接符号先转成物理地址，以便与 RAM 范围比较。 */
    if (!page_allocator_init(&boot_info, virt_to_phys(__kernel_start),
                             virt_to_phys(__kernel_end), boot_info.dtb_base,
                             boot_info.dtb_size))
        panic("cannot initialize physical page allocator");
    console_puts("free pages = ");
    console_puthex(page_free_count());
    console_putc('\n');

    /* 用物理页分配器建立正式页表，映射 RAM 和设备寄存器，再替换
     * head.S 的临时启动页表。RV64 的 satp 高四位为 8 时表示 Sv39。 */
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

    /* 内存自检通过后初始化堆和日志，再将时钟频率和内核页表交给
     * 调度器。M4 自检在此基础上验证线程切换、抢占以及等待与唤醒。 */
    heap_init();
    log_init();
    if (!scheduler_init(boot_info.timebase_frequency, &kernel_page_table))
        panic("cannot initialize scheduler");
    if (!run_m4_tests())
        panic("M4 scheduler/synchronization self-test failed");
    console_puts("M4 THREAD PASS\n");
    console_puts("M4 PREEMPT PASS\n");
    console_puts("M4 WAIT PASS\n");
    console_puts("M4 PASS\n");

    if (boot_info.interactive_shell)
        console_puts("Starting /bin/sh on the console\n");
    if (!user_run_init(&kernel_page_table, boot_info.interactive_shell))
        panic("/init failed");
    if (!boot_info.interactive_shell) {
        console_puts("M3 COPY PASS\n");
        console_puts("M3 ISOLATION PASS\n");
        console_puts("M3 PASS\n");
        console_puts("M5 INITRAMFS PASS\n");
        console_puts("M5 ELF PASS\n");
        console_puts("M5 LIFECYCLE PASS\n");
        console_puts("M5 PASS\n");
        console_puts("M6 RESOURCE RECLAIM PASS\nM6 PASS\n");
    }
    /* 所有自检完成，先停用调度定时器，再向平台报告成功。 */
    scheduler_stop_timer();
    platform_exit(true);
}
