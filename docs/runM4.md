# Codex conversation

## User

我应该如何理解这个项目，我希望我能说出每个函数的细节，请你先帮我设计思路

## Assistant

我先看一下项目结构和主要入口，再按这个项目的实际代码，帮你设计一条从整体理解到逐个函数讲清楚的学习路线。

## Activity

    $ pwd && rg --files -g 'AGENTS.md' -g 'README*' -g 'Cargo.toml' -g 'Makefile' -g '*.c' -g '*.h' -g '*.rs' -g '*.md' -g 'CMakeLists.txt' -g '*.S' -g '*.cpp' -g '*.py' -g '*.toml'
    /home/celestee/prj/minicore
    user/user.S
    docs/m1.md
    docs/m2.md
    docs/syscall-abi.md
    docs/roadmap-v2.md
    src/arch/riscv/csr.h
    src/arch/riscv/sbi.h
    src/arch/riscv/sbi.c
    src/arch/riscv/trap.h
    src/arch/riscv/head.S
    src/arch/riscv/context.S
    src/arch/riscv/trap.S
    src/arch/riscv/trap.c
    docs/notes/device-tree.md
    docs/notes/run.md
    docs/notes/riscv.md
    docs/notes/csr.md
    docs/notes/plic.md
    docs/notes/why-S-mode-can-manipulate-UART0?.md
    docs/notes/Linux-ISAs.md
    docs/boot.md
    docs/m4.md
    docs/linux-comparison.md
    docs/m3.md
    docs/memory-layout.md
    src/kernel/log.h
    src/kernel/sync.c
    src/kernel/user.h
    src/kernel/list.h
    src/kernel/sync.h
    src/kernel/m4_test.c
    src/kernel/refcount.h
    src/kernel/scheduler.c
    src/kernel/user.c
    src/kernel/m4_test.h
    src/kernel/scheduler.h
    src/kernel/log.c
    src/runtime.h
    src/main.c
    src/platform/plic.c
    src/platform/plic.h
    src/platform/qemu-virt.c
    src/platform/dtb.h
    src/platform/dtb.c
    src/platform/platform.h
    README.md
    Makefile
    src/runtime.c
    tools/generate-layout.py
    src/mm/heap.h
    src/mm/vm.h
    src/mm/page_alloc.h
    src/mm/heap.c
    src/mm/page_alloc.c
    src/mm/layout.h
    src/mm/vm.c
    ✓ • 0ms

## Activity

    $ ls -la && rg --files -g 'AGENTS.md' /home/celestee/prj /home/celestee/.codex 2>/dev/null
    total 48
    drwxrwxr-x 10 celestee celestee 4096 Sep 25 14:18 .
    drwxrwxr-x 10 celestee celestee 4096 Sep 23 21:22 ..
    dr-xr-xr-x  2 celestee celestee   40 Sep 25 14:18 .agents
    dr-xr-xr-x  2 celestee celestee   40 Sep 25 14:18 .codex
    drwxrwxr-x  8 celestee celestee 4096 Sep 24 18:27 .git
    -rw-rw-r--  1 celestee celestee   26 Sep 24 01:35 .gitignore
    -rw-rw-r--  1 celestee celestee 4654 Sep 24 18:34 Makefile
    -rw-rw-r--  1 celestee celestee 1584 Sep 24 18:37 README.md
    drwxrwxr-x  2 celestee celestee 4096 Sep 24 18:37 build
    drwxrwxr-x  4 celestee celestee 4096 Sep 24 18:34 docs
    drwxrwxr-x  7 celestee celestee 4096 Sep 24 16:06 src
    drwxrwxr-x  3 celestee celestee 4096 Sep 23 22:01 tools
    drwxrwxr-x  2 celestee celestee 4096 Sep 24 16:06 user
    ✗ (1) • 0ms

## Activity

    $ sed -n '1,200p' README.md && sed -n '1,240p' src/main.c && sed -n '1,180p' Makefile && sed -n '1,200p' docs/roadmap-v2.md
    # README
    
    This is a mini os referencing rCore with a dead predecessor https://github.com/heichiii/mycore.
    
    ## Build and run
    
    The kernel targets RV64 S-mode on QEMU `virt` and boots through OpenSBI:
    
    ```sh
    make
    make run
    ```
    
    During boot the high-half kernel checks DTB discovery, deliberately handles
    illegal-instruction, breakpoint, and load-access-fault exceptions, then
    delivers three SBI timer interrupts while verifying every saved integer
    register. It then installs its final Sv39 page table and tests physical-page
    allocation, mapping, protection, unmapping, page-table reclamation, read-only
    text, and the kernel-stack guard page. It then runs an embedded U-mode program
    through the `write`, `exit`, and `yield` syscall ABI and checks cross-page copies, bad
    pointers, kernel isolation, RX user text, and complete address-space teardown.
    A successful boot then exercises the M4 heap, kernel threads, register-safe
    round-robin switching, timer preemption, sleeping, and wait-queue based
    producer/consumer synchronization. It prints `M4 PASS` and exits QEMU.
    `make test` runs this acceptance test with both 256 MiB and 32 MiB of RAM.
    
    ## Memory layout report
    
    Build the kernel and generate a self-contained HTML visualization of its ELF
    sections, load segments, RAM usage, and the QEMU `virt` physical address space:
    
    ```sh
    make layout
    ```
    
    Open `build/layout.html` in a browser. The report is generated from the current
    ELF and a QEMU monitor snapshot rather than from hard-coded addresses. Alternate
    RAM sizes can be inspected with, for example, `make layout QEMU_MEMORY=512M`.
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
    CROSS ?= riscv64-unknown-elf-
    CC := $(CROSS)gcc
    QEMU ?= qemu-system-riscv64
    GDB ?= gdb-multiarch
    PYTHON ?= python3
    QEMU_MACHINE ?= virt
    QEMU_MEMORY ?= 256M
    
    TARGET := build/kernel.elf
    LINKER_SCRIPT := src/linker/linker.ld
    OBJS := build/main.o build/runtime.o build/dtb.o build/qemu-virt.o \
        build/plic.o build/sbi.o build/trap.o build/page_alloc.o build/vm.o \
        build/heap.o build/sync.o build/log.o build/scheduler.o build/m4-test.o \
        build/user.o build/user-image.o build/context-asm.o build/trap-asm.o build/head.o
    ARCH := -march=rv64imac_zicsr_zicntr -mabi=lp64 -mcmodel=medany
    CFLAGS := $(ARCH) -std=c11 -O2 -g -Wall -Wextra -Werror \
        -ffreestanding -fno-builtin -fno-stack-protector -fno-pie
    
    .PHONY: all run test debug gdb layout clean
    all: $(TARGET)
    
    $(TARGET): $(OBJS) $(LINKER_SCRIPT)
        $(CC) $(ARCH) -nostdlib -static -no-pie -Wl,--build-id=none \
            -T $(LINKER_SCRIPT) $(OBJS) -lgcc -o $@
    
    build/main.o: src/main.c src/arch/riscv/sbi.h src/arch/riscv/trap.h \
            src/kernel/user.h src/platform/dtb.h src/platform/plic.h \
            src/platform/platform.h src/runtime.h
        @mkdir -p build
        $(CC) $(CFLAGS) -c $< -o $@
    
    build/runtime.o: src/runtime.c src/runtime.h
        @mkdir -p build
        $(CC) $(CFLAGS) -c $< -o $@
    
    build/dtb.o: src/platform/dtb.c src/platform/dtb.h src/runtime.h
        @mkdir -p build
        $(CC) $(CFLAGS) -c $< -o $@
    
    build/qemu-virt.o: src/platform/qemu-virt.c src/platform/platform.h
        @mkdir -p build
        $(CC) $(CFLAGS) -c $< -o $@
    
    build/plic.o: src/platform/plic.c src/platform/plic.h src/platform/dtb.h
        @mkdir -p build
        $(CC) $(CFLAGS) -c $< -o $@
    
    build/sbi.o: src/arch/riscv/sbi.c src/arch/riscv/sbi.h
        @mkdir -p build
        $(CC) $(CFLAGS) -c $< -o $@
    
    build/trap.o: src/arch/riscv/trap.c src/arch/riscv/trap.h \
            src/arch/riscv/csr.h src/arch/riscv/sbi.h src/kernel/user.h \
            src/platform/platform.h
        @mkdir -p build
        $(CC) $(CFLAGS) -c $< -o $@
    
    build/page_alloc.o: src/mm/page_alloc.c src/mm/page_alloc.h src/mm/layout.h \
            src/platform/dtb.h
        @mkdir -p build
        $(CC) $(CFLAGS) -c $< -o $@
    
    build/vm.o: src/mm/vm.c src/mm/vm.h src/mm/page_alloc.h src/mm/layout.h \
            src/platform/dtb.h src/runtime.h
        @mkdir -p build
        $(CC) $(CFLAGS) -c $< -o $@
    
    build/heap.o: src/mm/heap.c src/mm/heap.h src/mm/page_alloc.h \
            src/kernel/list.h src/kernel/sync.h
        @mkdir -p build
        $(CC) $(CFLAGS) -c $< -o $@
    
    build/sync.o: src/kernel/sync.c src/kernel/sync.h src/kernel/scheduler.h
        @mkdir -p build
        $(CC) $(CFLAGS) -c $< -o $@
    
    build/log.o: src/kernel/log.c src/kernel/log.h src/kernel/sync.h
        @mkdir -p build
        $(CC) $(CFLAGS) -c $< -o $@
    
    build/scheduler.o: src/kernel/scheduler.c src/kernel/scheduler.h \
            src/kernel/sync.h src/mm/heap.h src/mm/page_alloc.h
        @mkdir -p build
        $(CC) $(CFLAGS) -c $< -o $@
    
    build/m4-test.o: src/kernel/m4_test.c src/kernel/m4_test.h \
            src/kernel/scheduler.h src/kernel/sync.h src/mm/heap.h
        @mkdir -p build
        $(CC) $(CFLAGS) -c $< -o $@
    
    build/user.o: src/kernel/user.c src/kernel/user.h src/arch/riscv/csr.h \
            src/arch/riscv/trap.h src/mm/vm.h src/mm/page_alloc.h \
            src/mm/layout.h src/platform/platform.h src/runtime.h
        @mkdir -p build
        $(CC) $(CFLAGS) -c $< -o $@
    
    build/user-program.o: user/user.S
        @mkdir -p build
        $(CC) $(ARCH) -g -c $< -o $@
    
    build/user.elf: build/user-program.o user/user.ld
        $(CC) $(ARCH) -nostdlib -static -no-pie -Wl,--build-id=none \
            -T user/user.ld build/user-program.o -o $@
    
    build/user.bin: build/user.elf
        $(CROSS)objcopy -O binary $< $@
    
    build/user-image.o: build/user.bin
        $(CROSS)objcopy -I binary -O elf64-littleriscv -B riscv \
            --rename-section .data=.rodata.user,alloc,load,readonly,data,contents \
            $< $@
    
    build/trap-asm.o: src/arch/riscv/trap.S
        @mkdir -p build
        $(CC) $(ARCH) -g -c $< -o $@
    
    build/context-asm.o: src/arch/riscv/context.S
        @mkdir -p build
        $(CC) $(ARCH) -g -c $< -o $@
    
    build/head.o: src/arch/riscv/head.S
        @mkdir -p build
        $(CC) $(ARCH) -g -c $< -o $@
    
    run: $(TARGET)
        $(QEMU) -machine virt -m 256M -smp 1 -nographic \
            -bios default -kernel $(TARGET)
    
    test: $(TARGET)
        @set -e; for memory in 256M 32M; do \
            log=build/test-$$memory.log; \
            timeout 20s $(QEMU) -machine virt -m $$memory -smp 1 -nographic \
                -bios default -kernel $(TARGET) >$$log 2>&1; \
            grep -q "M4 PASS" $$log; \
            echo "M4 $$memory PASS"; \
        done
    
    debug: $(TARGET)
        $(QEMU) -machine virt -m 256M -smp 1 -nographic \
            -bios default -kernel $(TARGET) -S -s
    
    gdb: $(TARGET)
        $(GDB) $(TARGET) -ex 'target remote :1234'
    
    layout: $(TARGET)
        $(PYTHON) tools/generate-layout.py \
            --elf $(TARGET) \
            --readelf $(CROSS)readelf \
            --nm $(CROSS)nm \
            --qemu $(QEMU) \
            --machine $(QEMU_MACHINE) \
            --memory $(QEMU_MEMORY) \
            --output build/layout.html
    
    clean:
        rm -rf build
    # MiniCore 学习路线（修订版）
    
    目标：在 QEMU `virt` 上，从 OpenSBI 启动一个 RV64 S-mode 内核，逐步运行用户程序，最后拥有进程、文件描述符、VFS、磁盘文件系统和多核支持。每一阶段都应留下可运行的镜像和自动化验收用例。
    
    本文是实施顺序，不要求一开始就实现 Linux/POSIX 兼容。每完成一个里程碑，记录对应的 Linux 概念、主要数据结构和不同设计的原因。
    
    ## 固定边界与工程约定
    
    - 平台：QEMU `virt`，先 `-smp 1`；OpenSBI 提供 M-mode 固件，内核运行在 S-mode。
    - 硬件目标可以是 RV64GC；在支持浮点上下文切换前，内核和首批用户程序用 `-march=rv64imac -mabi=lp64` 编译。以后启用 F/D 时，再明确用户 ABI 和浮点寄存器保存策略。不要混用 LP64 与 LP64D 目标文件。
    - C11 freestanding，少量 RISC-V 汇编；页大小 4 KiB；第一版使用 Sv39。
    - `arch/` 负责 CSR、trap、页表格式等架构细节；`platform/` 负责 QEMU 设备和地址；内核通用代码不直接使用固定 MMIO 地址。
    - SBI 调用集中封装。启动时探测需要的扩展；定时器通过 SBI TIME 请求，不从 S-mode 直接编程 CLINT。
    - 内核最终采用高半区映射，保留 RAM direct map。启动时允许短暂恒等映射；切换 `satp` 前后都要保证当前代码、栈、DTB 和必要 MMIO 可访问。
    - 用户 syscall ABI 先自定义并写入 `docs/syscall-abi.md`。先实现确实用到的调用；Linux 兼容列为后期目标。
    - 每个内核对象定义所有权、释放时机和锁规则。错误路径与正常路径同样纳入测试。
    
    建议先写简短的 `docs/boot.md`、`docs/memory-layout.md`、`docs/syscall-abi.md`；锁规则在引入调度器前补齐。不必在第一行代码前写完所有设计文档。
    
    ## M0：可重复的构建与启动
    
    实现 linker script、`_start`、boot stack、清零 BSS、最小 C runtime、panic、轮询 UART 和 QEMU 测试退出接口。保存 OpenSBI 传入的 hart ID 与 DTB 指针。先固定单核和一个 QEMU 版本用于回归测试。
    
    启动命令示例：
    
    ```sh
    qemu-system-riscv64 -machine virt -m 256M -smp 1 \
      -nographic -bios default -kernel build/kernel.elf
    ```
    
    **验收：** `kernel_main` 稳定打印段地址、hart ID 和 DTB 地址；panic 停机；测试程序能以可识别的成功/失败状态退出；GDB 能在 `_start` 和 `kernel_main` 断点。
    
    ## M1：DTB、trap 与时间
    
    先做足够小的 DTB reader：验证头部和边界，读取 RAM、`/reserved-memory`、memory reservation table、UART、PLIC、VirtIO 节点以及 `timebase-frequency`。只有确实使用到的属性才需要解析；保留原始 DTB，直到相关信息复制完成。
    
    接着实现 CSR 辅助函数、S-mode trap 入口、完整整数寄存器 trap frame、异常分发和 `sret` 返回。通过 SBI TIME 设置下一次 timer event。PLIC 和外部中断先建立接口；在引入 UART 接收中断或 VirtIO 中断时再完成端到端测试。
    
    **验收：** illegal instruction、breakpoint 和故意触发的访问异常能分类输出 `scause/sepc/stval`；定时器反复触发且返回后寄存器保持正确；修改 QEMU 内存大小后，DTB 解析结果随之变化。
    
    ## M2：物理页与 Sv39
    
    根据 DTB 建立可分配 RAM 范围，排除内核镜像、boot stack、DTB、固件和所有保留区。先用仅供启动的 bump allocator 建页表与元数据，再实现正式的物理页分配器。第一版可以用简单 free list；确实需要连续多页时再增加 buddy/order 分配。
    
    实现 Sv39 `map/unmap/protect/query`、页表页回收、`sfence.vma`、内核高半区、RAM direct map、MMIO 映射和内核栈 guard page。显式处理 Sv39 地址规范化、PTE 权限和 A/D 位。若要求内核代码真正不可写，direct map 中对应的物理页也不能留下可写别名。先采用本地完整 TLB flush；ASID 和跨核 shootdown 留到确有需要时。
    
    **验收：** 开启 `satp` 后继续运行；`.text` 不可写、guard page 必定 fault；map → write → unmap → fault 成立；页表销毁后页数恢复；`-m 32M` 下仍可启动。
    
    ## M3：第一个 U-mode 程序
    
    创建最小用户地址空间、用户栈和 U-mode trap 返回路径。第一个用户程序作为构建产物嵌入内核镜像，用它测试 `ecall`、非法指针、用户页权限和异常退出；这种嵌入方式只用于测试，不是最终程序加载接口。
    
    实现固定且文档化的少量 syscall：`exit` 和 `write`。`write` 从第一天就通过轻量 `file_ops` 与 fd 表访问 console；后续 VFS 可以复用这个接口。实现 `copy_from_user/copy_to_user`，检查溢出、权限和跨页访问，并定义 S-mode 访问用户页时 `SUM` 的使用范围及 fault 恢复方式。
    
    **验收：** 用户程序可以输出、调用 syscall、正常退出；用户态无法访问内核页；坏指针只让 syscall 失败或结束该进程，不让内核崩溃。
    
    ## M4：内核对象、线程与调度
    
    加入 `kmalloc/kfree`（简单 size class 或 slab 均可）、引用计数、intrusive list、日志缓冲。定义 interrupt disable/restore、spinlock、wait queue，以及中断上下文能否睡眠的规则。
    
    先实现内核线程、上下文切换、idle 线程和简单 round-robin 调度；先用显式 `yield` 验证切换，再接上 timer 抢占和 sleep/wakeup。此时再向用户态提供 `yield` syscall。优先级、多级队列、复杂锁诊断可以等正确性稳定后再加。
    
    **验收：** 多个线程交替运行且寄存器不丢失；sleep 不忙等；定时器抢占正常；producer/consumer 测试无丢失唤醒。调度器不得持有会在切换后无法正确释放的锁。
    
    ## M5：只读用户空间与进程生命周期
    
    加入最小只读 initramfs，并在其上建立 VFS 的基础对象与路径查找。实现 ELF64 静态可执行文件加载、段权限、用户栈上的 `argc/argv/envp`，然后从 initramfs 启动 `/init`。从这一阶段开始，用户程序不再依赖内核中的嵌入测试镜像。
    
    建立 `process` 和 `thread` 的关系、PID、父子关系，以及 `getpid/exec/exit/waitpid` syscall。随后实现 eager-copy `fork`；COW 延后。ELF loader 只依赖读取文件的抽象，不依赖磁盘驱动。初期可让 `/init` 启动固定路径的测试程序。
    
    **验收：** `/init` 能运行另一个 ELF；`fork/exec/exit/waitpid` 生命周期闭合；退出后用户页、页表、内核栈和 fd 均被释放；用户程序崩溃不会终止内核。
    
    ## M6：VFS、fd 与完整的内存 syscall
    
    扩展 VFS 的 `vnode/file/mount`、路径解析、cwd/root 和 fd 表。实现 `openat/close/read/write/dup`、目录读取、pipe 与重定向。补齐 `brk`、`mmap`、`munmap`、VMA 和用户内存区域管理；避免让 syscall 层直接调用具体设备或文件系统。
    
    先把 initramfs、console 和 pipe 接到同一套 file 接口，再增加新文件系统。目录、链接、mount 语义应按你定义的 ABI 写成测试，不必一次追齐 POSIX。
    
    **验收：** shell 能启动外部程序，并完成管道和重定向；文件 offset、dup 共享关系、关闭时机和并发引用计数正确；坏路径与坏指针有稳定错误码。
    
    ## M7：VirtIO 块设备与持久化文件系统
    
    从 DTB 匹配 VirtIO-MMIO 设备，完成 feature negotiation、split virtqueue、descriptor 生命周期、DMA 内存规则和内存屏障。先实现轮询 I/O，再接 PLIC 中断完成与 wait queue；两种模式共用 block request 接口。
    
    在通用 block layer 上建 buffer cache。先实现简单、可检查的一种磁盘文件系统；自研小 inode FS 或 ext2 都合适。需要写入时再明确缓存写回、`fsync` 和崩溃一致性承诺。保留 initramfs 作为启动根文件系统或恢复环境。
    
    **验收：** 跨 sector 和非对齐读写、并发请求、设备不存在及 feature 协商失败都能正确处理；descriptor 不泄漏；重启后已同步的数据仍可读。
    
    ## M8：SMP
    
    使用 SBI HSM 启动 secondary hart，设置每核栈和 per-CPU idle 线程。增加跨核唤醒、IPI、run queue 锁及远端 TLB shootdown。逐项审查先前依赖“关本地中断”的临界区。
    
    **验收：** 同一套内存、进程、VFS 与块设备压力测试在 `-smp 1/2/4` 下通过；没有把本地关中断当成全局互斥。
    
    ## M9：按兴趣扩展
    
    从 COW fork、demand paging、signal、终端行规、网络、Linux syscall 子集兼容中选一两个专题。每增加一个专题，都先写可观察行为和失败路径，再实现机制。
    
    ## 建议的仓库布局
    
    ```text
    arch/riscv/          CSR、trap、上下文切换、SBI、PTE 格式
    platform/qemu-virt/  DTB 平台发现、UART、PLIC、测试退出
    mm/                  物理页、内核堆、页表、VMA
    kernel/              线程、调度、进程、syscall、同步
    drivers/             VirtIO、block
    fs/                  VFS、initramfs、磁盘 FS、pipe
    user/                crt、libc-like 包装、init、shell、命令
    tests/               宿主机测试与 QEMU 集成测试
    docs/                ABI、内存布局、启动和锁约定
    ```
    
    依赖以接口为准：驱动依赖内存和同步原语；文件系统依赖通用 block 接口；syscall 依赖 VM、进程与 VFS 接口。允许这些模块在同一个里程碑中逐步长成，但不要跨层直接访问硬件寄存器。
    
    ## 每个里程碑的工作循环
    
    1. 写一个能够失败的 QEMU 验收用例，并记录预期输出或退出码。
    2. 实现最小机制；在 GDB 中确认关键状态变化。
    3. 测试正常、错误、资源耗尽和重复初始化/释放路径。
    4. 在 `docs/linux-comparison.md` 中记下 Linux 对应概念，以及 MiniCore 做了哪些简化。
    5. 保留该里程碑可启动的提交或 tag，再进入下一阶段。
    
    参考规范：[QEMU `virt`](https://www.qemu.org/docs/master/system/riscv/virt.html)、[RISC-V Supervisor ISA](https://docs.riscv.org/reference/isa/priv/supervisor.html)、[SBI](https://github.com/riscv-non-isa/riscv-sbi-doc)、[RISC-V ELF psABI](https://github.com/riscv-non-isa/riscv-elf-psabi-doc)。
    ✓ • 0ms

## Activity

    $ sed -n '1,130p' src/arch/riscv/head.S && sed -n '1,150p' src/mm/page_alloc.c && sed -n '1,180p' src/kernel/scheduler.h && sed -n '1,130p' docs/m4.md
        .section .text.boot, "ax", @progbits
        .align 2
        .globl _start
        .type _start, @function
    _start:
        /* OpenSBI 在 MMU 关闭（satp.MODE=Bare）的状态下跳到这里：
         *   a0 = 当前 hart ID
         *   a1 = DTB 的物理地址
         * 此时还没有可用栈，因此先用被调用者保存寄存器保留启动参数。 */
        mv s0, a0
        mv s1, a1
    
        /* boot_root_page 是一个 4 KiB 的 Sv39 根页表（L2 页表）。
         * 每个 PTE 为 8 字节，所以一张表有 4096 / 8 = 512 个表项。
         * 先逐项清零，保证未显式设置的地址范围全部无效。 */
        la t0, boot_root_page
        li t1, 512
    .Lzero_root:
        sd zero, 0(t0)
        addi t0, t0, 8
        addi t1, t1, -1
        bnez t1, .Lzero_root
    
        /* 在 L2 根页表中直接设置 R/W/X 的表项会成为叶子 PTE。
         * 一个 L2 表项控制 1 GiB：512 个 L1 表项 × 2 MiB。
         *
         * 0xcf = V | R | W | X | A | D：
         *   V：PTE 有效；R/W/X：可读、可写、可执行；
         *   A/D：已经访问、已经写入。
         * 这些 RWX 大页只供启动过渡使用，正式页表会收紧权限。 */
        la t0, boot_root_page
        li t1, 0xcf                 /* V|R|W|X|A|D */
    
        /* root[0]：VA 0x00000000..0x3fffffff
         *          -> PA 0x00000000..0x3fffffff。
         * 这一区间包含 QEMU test finisher、PLIC、UART 和 VirtIO MMIO。 */
        sd t1, 0(t0)
    
        /* PTE 的 PPN 从 bit 10 开始，而物理页号是 PA >> 12，所以：
         *   (PA >> 12) << 10 = PA >> 2。
         * 下列物理基址都按 1 GiB 对齐，满足 L2 大页对齐要求。 */
    
        /* root[1]：VA 0x40000000..0x7fffffff -> 相同 PA。 */
        li t2, 0x40000000
        srli t2, t2, 2
        or t2, t2, t1
        sd t2, 8(t0)
    
        /* root[2]：VA 0x80000000..0xbfffffff -> 相同 PA。
         * 当前低地址启动代码和 QEMU RAM 位于这个 1 GiB 区间内。 */
        li t2, 0x80000000
        srli t2, t2, 2
        or t2, t2, t1
        sd t2, 16(t0)
    
        /* root[3]：VA 0xc0000000..0xffffffff -> 相同 PA。 */
        li t2, 0xc0000000
        srli t2, t2, 2
        or t2, t2, t1
        sd t2, 24(t0)
    
        /* 再把相同的 4 GiB 物理空间映射到高半区 direct map：
         *   root[256] -> PA 0x00000000
         *   root[257] -> PA 0x40000000
         *   root[258] -> PA 0x80000000
         *   root[259] -> PA 0xc0000000
         * root[256] 对应的规范化 Sv39 虚拟地址是
         * 0xffffffc000000000，即 KERNEL_DIRECT_BASE。
         *
         * root[256] 的字节偏移为 256 * 8 = 2048，但 store 的 12 位
         * 有符号立即数最大只能表示 2047，所以先让 t0 前进 2040 字节，
         * 再用 8、16、24、32 作为剩余偏移。 */
        addi t0, t0, 2040
        sd t1, 8(t0)                /* root[256] = root[0] */
        ld t2, -2032(t0)            /* 读取 root[1] */
        sd t2, 16(t0)               /* root[257] = root[1] */
        ld t2, -2024(t0)            /* 读取 root[2] */
        sd t2, 24(t0)               /* root[258] = root[2] */
        ld t2, -2016(t0)            /* 读取 root[3] */
        sd t2, 32(t0)               /* root[259] = root[3] */
    
        /* 恢复 t0，使它再次指向根页表的物理起始地址。 */
        addi t0, t0, -2040
    
        /* 构造 satp：
         *   bits 63:60 MODE = 8，表示 Sv39；
         *   bits 59:44 ASID = 0；
         *   bits 43:0  PPN = 根页表物理地址 >> 12。
         * 写 satp 后地址翻译立即生效；低地址恒等映射保证当前指令流
         * 仍能继续执行，随后再显式跳到高半区。 */
        srli t0, t0, 12
        li t1, 8
        slli t1, t1, 60
        or t0, t0, t1
        csrw satp, t0
        /* 丢弃旧的地址翻译，并保证后续取指/访存使用刚写入的页表。 */
        sfence.vma zero, zero
    
        /* .Lhigh_entry 位于高半区，与当前低地址 PC 相距太远，普通直接跳转
         * 无法编码。链接器把完整的 64 位高地址写入下面的 .dword，这里先
         * 从仍由恒等映射访问的 .text.boot 中读出它，再间接跳转。 */
        la t0, .Lhigh_entry_address
        ld t0, 0(t0)
        jr t0
    
        .align 3
    .Lhigh_entry_address:
        .dword .Lhigh_entry
        /* _start 的尺寸只计算低地址 .text.boot 部分；高地址入口属于 .text。 */
        .size _start, . - _start
    
        .section .text, "ax", @progbits
        .align 2
    .Lhigh_entry:
        /* 现在 PC 已经是高半区虚拟地址。初始化 gp 时必须关闭 linker
         * relaxation，否则“给 gp 赋初值”的指令本身可能被优化成依赖 gp。 */
        .option push
        .option norelax
        la gp, __global_pointer$
        .option pop
    
        /* 切换到链接器脚本预留的高半区启动栈。栈向低地址增长。 */
        la sp, __boot_stack_top
    
        /* .bss 是 ELF 的 NOBITS 区域，不在镜像中存放零字节，因此内核负责
         * 清零。此循环本身不使用栈，所以即使启动栈也位于 .bss 中仍安全。 */
        la t0, __bss_start
        la t1, __bss_end
    .Lclear_bss:
        bgeu t0, t1, .Lbss_done
    #include "page_alloc.h"
    
    #include "layout.h"
    
    struct free_page {
        struct free_page *next;
    };
    
    static struct free_page *free_list;
    static size_t free_pages;
    static size_t total_pages;
    
    static uint64_t align_up(uint64_t value)
    {
        if (value > UINT64_MAX - PAGE_MASK)
            return UINT64_MAX & ~PAGE_MASK;
        return (value + PAGE_MASK) & ~PAGE_MASK;
    }
    
    static bool overlaps(uint64_t base, uint64_t end, uint64_t other_base,
                         uint64_t other_size)
    {
        uint64_t other_end;
    
        if (other_size == 0)
            return false;
        other_end = other_base > UINT64_MAX - other_size
                        ? UINT64_MAX
                        : other_base + other_size;
        return base < other_end && other_base < end;
    }
    
    static bool page_reserved(const struct boot_info *info, uint64_t base,
                              uint64_t kernel_start, uint64_t kernel_end,
                              uint64_t dtb_start, uint64_t dtb_size,
                              uint64_t firmware_start)
    {
        uint64_t end = base + PAGE_SIZE;
    
        if (overlaps(base, end, kernel_start, kernel_end - kernel_start) ||
            overlaps(base, end, dtb_start, dtb_size) ||
            (base >= firmware_start && base < kernel_start))
            return true;
        for (size_t i = 0; i < info->reserved_count; ++i) {
            if (overlaps(base, end, info->reserved[i].base,
                         info->reserved[i].size))
                return true;
        }
        return false;
    }
    
    bool page_allocator_init(const struct boot_info *info, uint64_t kernel_start,
                             uint64_t kernel_end, uint64_t dtb_start,
                             uint64_t dtb_size)
    {
        uint64_t firmware_start = kernel_start;
    
        if (!info || kernel_start >= kernel_end)
            return false;
        free_list = 0;
        free_pages = 0;
        total_pages = 0;
    
        /* The RAM range containing the loaded kernel also contains OpenSBI below
         * it.  Treat that prefix as firmware-owned even if the DTB omits it. */
        for (size_t i = 0; i < info->memory_count; ++i) {
            uint64_t end = info->memory[i].base + info->memory[i].size;
    
            if (kernel_start >= info->memory[i].base && kernel_start < end) {
                firmware_start = info->memory[i].base;
                break;
            }
        }
    
        for (size_t i = 0; i < info->memory_count; ++i) {
            uint64_t base = align_up(info->memory[i].base);
            uint64_t raw_end = info->memory[i].base + info->memory[i].size;
            uint64_t end = raw_end & ~PAGE_MASK;
    
            if (raw_end < info->memory[i].base)
                return false;
            for (uint64_t page = base; page < end; page += PAGE_SIZE) {
                struct free_page *node;
    
                if (page_reserved(info, page, kernel_start, kernel_end,
                                  dtb_start, dtb_size, firmware_start))
                    continue;
                node = phys_to_virt(page);
                node->next = free_list;
                free_list = node;
                ++free_pages;
                ++total_pages;
            }
        }
        return free_pages != 0;
    }
    
    uint64_t page_alloc(void)
    {
        struct free_page *page = free_list;
    
        if (!page)
            return 0;
        free_list = page->next;
        --free_pages;
        return virt_to_phys(page);
    }
    
    void page_free(uint64_t physical)
    {
        struct free_page *page;
    
        if ((physical & PAGE_MASK) != 0 || physical == 0)
            return;
        page = phys_to_virt(physical);
        page->next = free_list;
        free_list = page;
        ++free_pages;
    }
    
    size_t page_free_count(void)
    {
        return free_pages;
    }
    
    size_t page_total_count(void)
    {
        return total_pages;
    }
    #ifndef MINICORE_KERNEL_SCHEDULER_H
    #define MINICORE_KERNEL_SCHEDULER_H
    
    #include <stdbool.h>
    #include <stdint.h>
    
    #include "list.h"
    
    typedef void (*thread_entry)(void *argument);
    
    bool scheduler_init(uint64_t timebase_frequency);
    bool thread_create(const char *name, thread_entry entry, void *argument);
    void scheduler_yield(void);
    void scheduler_sleep(uint64_t ticks);
    uint64_t scheduler_ticks(void);
    uint64_t scheduler_preemptions(void);
    void scheduler_tick(void);
    bool scheduler_timer_active(void);
    void scheduler_stop_timer(void);
    void scheduler_reap(void);
    
    /* Synchronization back-end.  Callers hold interrupts disabled. */
    void scheduler_block_current(struct list_node *queue);
    void scheduler_wake_node(struct list_node *node);
    void scheduler_switch(void);
    
    bool scheduler_register_probe(unsigned iterations);
    
    #endif
    # M4: kernel objects, threads, and scheduling
    
    MiniCore now has a reclaiming size-class kernel heap (16 through 2048 bytes),
    intrusive lists, a small reference counter, and a locked 1 KiB log ring.  The
    single-hart synchronization contract is explicit: code disables and later
    restores local interrupts around scheduler and wait-queue state; spinlocks
    never sleep; `wait_queue_sleep` atomically queues the current thread before
    dropping the caller's condition lock.
    
    Kernel threads own one physical-page stack and a callee-saved RISC-V context.
    The scheduler maintains a round-robin thread list and an idle thread.  A thread
    may yield, sleep until a timer tick, or block on a wait queue.  The SBI TIME
    interrupt wakes timed sleepers and preempts the current kernel or user
    execution.  Dead threads are reaped only from another stack.
    
    The M4 acceptance test checks heap reclamation, list/reference/log objects,
    callee-saved registers across repeated switches, timer-only preemption, timed
    sleep, and a bounded producer/consumer queue.  The latter runs with a capacity
    of four and verifies all 64 values and their checksum, exercising both full
    and empty wait paths without lost wakeups.  User syscall 3 exposes `yield`.
    
    The design is intentionally single-hart.  Disabling local interrupts is enough
    to protect scheduler state only until M8 introduces SMP; run-queue locking and
    cross-hart wakeups will then replace that assumption.
    ✓ • 0ms

## Assistant

我建议把目标定成：**能从一次真实执行出发，讲清楚每个函数为什么被调用、如何改变系统状态，以及为什么这样写。** 逐行翻译代码只是其中一部分。

我看了项目入口、启动汇编、内存分配器和调度接口。MiniCore 当前代码已经串起 M0～M4：启动、异常与中断、内存管理、用户态、线程调度。它很适合按“运行主线 → 模块 → 函数 → 验证”的顺序学习。

### 1. 先建立整个项目的执行地图

第一轮以 [kernel_main](/home/celestee/prj/minicore/src/main.c) 为目录，只理解各阶段在做什么：

```text
OpenSBI 交接
  → _start：建立启动映射，进入高地址，准备 C 运行环境
  → kernel_main
  → 发现硬件，初始化异常和定时器
  → 初始化物理页分配，建立正式页表
  → 初始化堆、日志和调度器
  → 运行用户态与系统调用测试
  → 运行线程、抢占与同步测试
  → 输出 M4 PASS，退出 QEMU
```

这一轮需要回答：谁把控制权交给内核？各模块依赖什么？哪些代码提供功能，哪些代码负责测试？

特别注意：[roadmap-v2.md](/home/celestee/prj/minicore/docs/roadmap-v2.md) 包含未来规划，理解当前行为要以代码为准。

### 2. 按依赖顺序深入，分七个专题

| 顺序 | 阅读范围 | 学完必须能回答的问题 |
|---|---|---|
| ① 构建与启动 | `Makefile`、`linker.ld`、`head.S`、`runtime.c` | C 代码运行之前，谁准备栈、地址映射和 BSS？ |
| ② 平台与输出 | `qemu-virt.c`、`sbi.c`、`dtb.c`、`plic.c` | 内核如何发现硬件、打印字符、请求定时器？ |
| ③ 异常与中断 | `csr.h`、`trap.h`、`trap.S`、`trap.c` | 执行为什么突然进入内核处理函数，又如何恢复？ |
| ④ 内存管理 | `layout.h`、`page_alloc.c`、`vm.c`、`heap.c` | 物理页、虚拟地址和堆对象之间是什么关系？ |
| ⑤ 用户态 | `user.c`、`user/user.S`、syscall ABI 文档 | 如何进入用户态？`ecall` 后如何处理参数并返回？ |
| ⑥ 线程与同步 | `list.h`、`refcount.h`、`context.S`、`scheduler.c`、`sync.c`、`log.c` | 线程如何暂停、恢复、阻塞和唤醒？ |
| ⑦ 验收代码 | `main.c` 中的测试、`m4_test.c` 等 | 每个测试证明了什么，还有什么没有被证明？ |

测试应随对应专题一起阅读，最后再做一次整体复盘。遇到依赖后面模块的函数，先记录接口契约，深入该模块时再补全。

### 3. 给每个函数建立同一套“讲解卡”

建议每个函数都回答下面七个问题：

1. **职责**：它解决什么具体问题？
2. **调用现场**：谁调用它？启动阶段还是运行阶段？中断是否开启？
3. **输入输出**：参数、返回值是什么含义？地址是物理地址还是虚拟地址？
4. **执行过程**：分几步？每一步为什么必要？
5. **状态与所有权**：修改了哪些全局变量、链表、寄存器或页表？资源由谁释放？
6. **边界与失败**：空值、越界、资源耗尽时怎样？依赖调用者保证什么？
7. **验证方法**：哪个测试覆盖它？调试时观察什么才能确认理解正确？

例如 [page_alloc()](/home/celestee/prj/minicore/src/mm/page_alloc.c:103)，不能只讲“从链表取一页”。还要能讲出：

- 空闲页自身存放链表节点，`free_list` 使用内核虚拟地址。
- 分配时移除头节点，并减少空闲页计数。
- 返回前转换为物理地址，链表为空时返回 `0`。
- 这个函数本身没有清零页面，也没有加锁。
- 因而调用者需要考虑页面内容初始化，以及分配操作是否可能被并发打断。

这才是“知道函数细节”的深度。

### 4. 用跨函数的执行过程检验理解

每学完一个专题，选一个具体场景，从头讲到尾：

- 打印一个字符。
- 分配一页，并把它映射到一个虚拟地址。
- 用户程序执行一次 `write`。
- 定时器打断一个正在运行的线程。
- 消费者发现队列为空，睡眠后被生产者唤醒。

讲解时始终追踪四样东西：**控制流、数据、资源归属、硬件状态**。这会把单独记住的函数连成一个系统。

### 5. 每轮学习都以“脱离代码复述”结束

建议每轮只覆盖一个小机制和几个相关函数：

> 看接口和数据结构 → 阅读实现 → 手推一个例子 → 用已有测试或 GDB 核对 → 合上代码复述。

完成标准可以设为：你能预测正常路径和失败路径，能解释关键语句删除后的后果，能指出理解所依赖的前提。函数清单则覆盖 `.c`、头文件中的内联函数、汇编入口，以及构建辅助脚本，逐项记录“未读／能解释／已验证”。

**第一课建议从“按下 `make run` 后，怎样抵达 `kernel_main`”开始。** 阅读范围限定为 `Makefile → linker.ld → head.S → kernel_main`，先画出启动时地址、栈和控制权的变化，再逐个拆解函数。

## User

请你作为老师自己给自己制定一个教学计划，我希望你引导我，以聊天的方式带我学会，不要让我自己痛苦的在函数中跳转

## Assistant

可以。之后我负责组织阅读顺序、串联调用关系，把需要看的代码带到聊天里。你不用自己在文件之间来回找，我们每次围绕一个具体问题推进。

我的教学安排是：

| 阶段 | 我带你弄懂的核心问题 |
|---|---|
| 启动 | 一个编译出来的文件，怎么变成正在运行的内核？ |
| 硬件交互 | 内核如何打印文字、发现内存、设置定时器？ |
| 异常与中断 | CPU 怎样暂停当前代码，处理事件，再回来？ |
| 内存 | 一个地址如何找到物理内存？内核怎样分配和回收它？ |
| 用户态 | 内核怎样运行用户程序，又限制它的权限？ |
| 调度与同步 | 多个线程如何轮流运行，又如何安全地等待彼此？ |
| 全项目复盘 | 把每个函数放回完整执行过程，补齐边界和失败路径。 |

我会维护学习进度。先用具体场景建立直觉，再引入术语，最后落到实际代码。每次只拿一小段必要代码，讲清它执行前后的变化；涉及其他函数时，我会直接补上相关实现和上下文。

每讲完一个关键点，我会问一个很短的推理问题，用来判断我的讲解是否清楚。答错了我们就换个角度讲，不要求你提前查资料。已经理解的内容加速，卡住的地方拆小。最终连辅助函数、汇编入口和错误路径也会覆盖，但不把所有细节一次压给你。

现在开始第一课：**我们的内核，最开始是谁运行起来的？**

平时写 C 程序，你可能习惯从 `main()` 开始理解。但 CPU 并不认识 `main` 这个名字。它只会从程序计数器 `PC` 指向的地址取指令，然后执行。

MiniCore 的启动过程，可以先抓住三个角色：

- **QEMU**：模拟一台 RISC-V 计算机，包括 CPU、内存和设备。
- **OpenSBI**：先运行的固件，做启动准备，再把控制权交给内核。
- **MiniCore**：接手后建立自己的运行环境，随后进入 C 代码。

在这个项目中，OpenSBI 交给 MiniCore 的第一个入口是汇编标签 `_start`，位于 `head.S`。稍后才会进入 `kernel_main()`。

交接时有两个重要信息，已经放在 CPU 寄存器里：

```text
a0 = 当前 CPU 核心的编号，也叫 hart ID
a1 = 硬件描述数据 DTB 的物理地址
```

你可以先把寄存器理解为 CPU 内部几个有名字的小存储格。OpenSBI 把信息放进去，MiniCore 接着读取，就完成了参数传递。DTB 则像一份机器配置清单，我们后面会拆开看。

项目开头的实际代码是：

```asm
_start:
    mv s0, a0
    mv s1, a1
```

`mv 目标, 来源` 表示复制数值。所以这两行只做了一件事：把启动参数暂存到 `s0`、`s1`，为后续启动过程保留它们。这里没有复制 DTB 本身，只是复制了它的地址。

此时还有一个关键条件：**内核尚未建立自己的可用栈。** 而接下来，启动代码需要准备地址映射、设置栈，并清零一些内存，才能让 C 代码正常运行。

我们先停在这里。我想了解一下你的基础，以便决定下一步讲多细：你对“C 函数调用为什么需要栈”的理解是比较清楚、只知道大概，还是还没接触过？

## User

我的理解是：C函数调用的时候，是把参数、返回地址等内容按一定顺序放在栈上，然后跳转到调用的函数，执行函数的时候，从栈上弹出参数保存到内存供函数使用，函数调用结束后，从栈上获取返回地址，跳转回去继续执行。

## Assistant

你抓住了关键：调用函数时要传递参数、记住返回位置，结束后再继续执行调用者。不过，你描述的是主要通过栈传参的模型；MiniCore 使用的 RISC-V 调用约定，优先使用寄存器。

先修正两个细节：**栈本身就是内存的一部分**；函数通常通过地址读取栈上的数据，并不需要先把参数“弹出来”。

假设有这样一段代码：

```c
int add(int x, int y)
{
    return x + y;
}

int result = add(3, 5);
```

在 RISC-V 上，可以把它的执行过程简化理解为：

```asm
li a0, 3       # 第一个参数放入 a0
li a1, 5       # 第二个参数放入 a1
call add       # 返回地址放入 ra，然后跳到 add
               # 返回后，a0 中是结果 8

add:
    add a0, a0, a1
    ret        # 跳到 ra 保存的地址
```

这里有三个约定：

- `a0`～`a7` 优先用于传递整数、指针等参数；超出寄存器承载能力的参数才需要通过栈传递。
- `ra` 保存返回地址。
- `a0` 也用于保存这里的返回值。

注意，这个简单的 `add` **可以完全不使用栈**。

那么，为什么通常还需要栈？我们给例子增加一层调用：

```text
调用者 → outer → add
```

进入 `outer` 时，`ra` 保存的是“回到调用者的位置”。但是，当 `outer` 再调用 `add` 时，`ra` 会被改写为“回到 outer 的位置”。

于是，`outer` 必须在调用 `add` 之前，把自己的返回地址保存起来。常见做法就是保存到栈中：

```text
进入 outer：ra = 回到调用者的位置
    ↓
outer 把这个 ra 保存到自己的栈空间
    ↓
调用 add：ra 被改成回到 outer 的位置
    ↓
add 返回 outer
    ↓
outer 从栈中恢复原来的 ra
    ↓
outer 返回调用者
```

除了返回地址，栈还可以保存放不下的局部数据，以及当前函数需要保留的寄存器值。`sp` 寄存器负责标记当前栈的位置，函数通过调整 `sp` 为自己留出空间。

这就接回了 MiniCore：启动代码不能假定普通 C 函数都像 `add` 一样不需要栈。因此，在进入 `kernel_main()` 前，它要先把 `sp` 指向一块准备好的内存。

我们用一个小问题确认这段过程：**如果 `outer` 没有保存原来的 `ra` 就调用了 `add`，那么 `add` 返回以后，`outer` 再执行 `ret`，它会尝试跳到哪里？**

## User

跳到add返回outer后的地方。ra是谁在什么时候设置的？

## Assistant

对，会跳回 `outer` 中调用 `add` 之后的位置，而不是回到 `outer` 的调用者。

**`ra` 是 CPU 在执行调用指令时设置的。** 编译器负责生成调用指令，CPU 执行它时，把“调用结束后应继续执行的地址”写进 `ra`。

我们来看一个具体例子。假设调用指令位于 `0x1000`：

```asm
# outer 中的代码
0x1000:  jal ra, add
0x1004:  ...              # add 返回后应该从这里继续
```

这里的 `jal` 是 RISC-V 的真实指令，名字是 *Jump And Link*，意思是“跳转并保存返回地址”。这条指令一次完成两件事：

```text
ra ← 0x1004
PC ← add 的入口地址
```

所以，**不是进入 `add` 之后，`add` 才设置返回地址；而是在跳过去的那一刻，调用指令已经保存好了。**

`add` 执行完后遇到：

```asm
ret
```

它的作用可以理解为：

```text
PC ← ra
```

于是 CPU 回到 `0x1004`。`ret` 不会自动从栈上恢复旧的 `ra`；如果之前保存过，必须由函数中的其他指令显式恢复。

还有一个和刚才示例有关的细节：之前写的 `call add` 是汇编器提供的**伪指令**，最终可能由一条或多条机器指令完成调用，但同样会把返回地址存入 `ra`。上面用四字节的 `jal` 举例，所以返回地址是 `0x1000 + 4`。

你现在可以把它记成一对：

> 调用指令：记下回来的位置，再跳过去。
> 返回指令：读取记下的位置，跳回来。

这也解释了嵌套调用的问题：`ra` 只是一个寄存器，只能保留一个地址；每次新的调用都会改写它，旧地址需要由软件保存。

## User

好的，继续前面的流程

## Assistant

我们回到 MiniCore 的启动现场。此时 OpenSBI 已经把控制权交给 `_start`：

```asm
_start:
    mv s0, a0
    mv s1, a1
```

启动参数已经暂存在 `s0`、`s1` 中，但还没有准备好栈。

按一般直觉，接下来可以先设置栈。不过这个项目先做了另一件事：**建立启动页表，让内核能够在它预定的高虚拟地址上运行。** 栈随后也会使用高虚拟地址。

这里需要先分清两种地址。

**物理地址**表示内存硬件上的位置。**虚拟地址**是程序执行时使用的地址；开启地址翻译后，CPU 会按照页表记录的规则，把虚拟地址转换成物理地址。

可以把页表暂时理解为一份对应关系：

```text
程序使用的虚拟地址 → 实际访问的物理地址
```

在 `_start` 刚开始执行时，地址翻译还没有开启。接下来它要建立这份对应关系。项目中的下一段代码是：

```asm
la t0, boot_root_page
li t1, 512

.Lzero_root:
    sd zero, 0(t0)
    addi t0, t0, 8
    addi t1, t1, -1
    bnez t1, .Lzero_root
```

这段代码只做一件事：**把启动根页表的 512 个表项全部清零。**

我们逐步读：

```asm
la t0, boot_root_page
```

把 `boot_root_page` 的地址放入 `t0`。注意，取的是地址，没有读取这个地址里的内容。`t0`、`t1` 是供程序临时使用的寄存器。

```asm
li t1, 512
```

把数字 `512` 放入 `t1`，作为循环计数。

接下来是循环体：

```asm
sd zero, 0(t0)
```

向 `t0 + 0` 指向的内存写入 **8 字节的零**。这里的 `zero` 是 RISC-V 的特殊寄存器，读取它永远得到零；`sd` 表示存储一个 64 位数值。

```asm
addi t0, t0, 8
addi t1, t1, -1
```

指针向后移动 8 字节，计数减一。

```asm
bnez t1, .Lzero_root
```

如果计数还不为零，就跳回循环开头。

用你熟悉的 C 表达，大致等价于：

```c
uint64_t *p = boot_root_page;

for (int remaining = 512; remaining != 0; --remaining) {
    *p = 0;
    ++p;
}
```

每个表项占 8 字节，一共 512 项，因此清零的大小是：

```text
512 × 8 = 4096 字节，也就是 4 KiB
```

为什么先清零？因为页表项里有一个“有效位”。全零表示该表项无效。这样，只有我们随后明确填写的映射才会生效，不会把内存中原本残留的数值误认为地址映射。

再联系刚才的栈：这段循环只使用寄存器和明确指定的内存位置，没有调用函数，也没有使用栈，所以它能在 `sp` 尚未准备好时执行。

到这里，内核只是准备了一张空表，**还没有开启地址翻译**。

继续之前，试着判断一下：上面的 `la t0, boot_root_page` 和 `sd zero, 0(t0)`，哪一条修改了 `t0`，哪一条修改了 `t0` 指向的内存？

## User

第一条修改t0,第二条修改内存

## Assistant

对，第一条把地址放进 `t0`，第二条通过这个地址修改内存。这个区别在后面读页表代码时会经常用到。

现在启动根页表已经清零，我们要往里面填写映射。不过先解决一个问题：**CPU 怎么知道应该查这张表里的哪一项？**

MiniCore 使用 RISC-V 的 **Sv39** 地址翻译方式。一般情况下，它用三级页表逐级查找。这里先记住最顶层，也叫 **L2 根页表**：

- 根页表有 512 项。
- 每一项负责一段大小为 **1 GiB** 的虚拟地址范围。
- 一个表项可以指向下一级页表，也可以直接给出物理内存映射。

启动阶段选择后者，直接用 1 GiB 的大范围映射，代码比较简单。

看项目接下来的代码：

```asm
la t0, boot_root_page
li t1, 0xcf
sd t1, 0(t0)
```

第一行重新让 `t0` 指向页表开头。因为刚才清零循环不断增加 `t0`，循环结束时，它已经指向表的末尾之后了。

第二行把 `0xcf` 放入 `t1`。这个数字并不是内存地址，而是一组页表标志位：

```text
0xcf = V | R | W | X | A | D
```

其中：

| 标志 | 含义 |
|---|---|
| V | 表项有效 |
| R / W / X | 允许读、写、执行 |
| A / D | 已访问、已写入标志，这里预先置位 |

这里设置了读、写、执行权限，说明它直接映射物理内存，是一个**叶子表项**，不需要再向下查表。启动时先使用宽松权限，正式页表会在后面收紧。

第三行把这个数值写入 `root[0]`。页表项除了标志位，还包含物理页号；这里物理页号部分都是零，所以它指向从物理地址 `0` 开始的区域。

最终建立的映射是：

```text
虚拟地址 0x00000000～0x3fffffff
    ↓
物理地址 0x00000000～0x3fffffff
```

虚拟地址与物理地址数值相同，这叫**恒等映射**。

举个例子：将来开启地址翻译后，访问虚拟地址 `0x10000000`，仍然会访问物理地址 `0x10000000`。这个范围包含项目使用的 UART 等设备的寄存器地址，因此设备访问也需要考虑映射。

这里容易产生一个疑问：既然地址没变，为什么还要填写映射？

因为开启地址翻译以后，CPU 会按页表寻找地址。**数值相同也需要明确建立映射，不会自动放行。** 恒等映射能让启动代码在切换地址翻译的过程中继续访问原来的地址。

不过，目前我们只是在内存中填写页表，CPU 还没开始使用它。

我们用一个具体地址往下推：`root[0]` 负责前 1 GiB，`root[1]` 负责接下来的 1 GiB。那么，位于物理地址 `0x80000000`，也就是 **2 GiB** 处的内存，如果要建立恒等映射，你觉得应该填 `root[几]`？

## User

2

## Assistant

对，是 `root[2]`。它负责从 `0x80000000` 开始的那段 1 GiB 虚拟地址范围，正好覆盖 QEMU 中 RAM 的起始位置。

接下来看看代码如何让这个表项指向物理地址 `0x80000000`：

```asm
li t2, 0x80000000
srli t2, t2, 2
or t2, t2, t1
sd t2, 16(t0)
```

此时 `t0` 指向根页表开头，`t1` 中仍然是权限标志 `0xcf`。我们逐条拆开。

```asm
li t2, 0x80000000
```

先把目标物理地址放入 `t2`。

```asm
srli t2, t2, 2
```

把它逻辑右移两位。这里不是把地址除以四后作为映射目标，而是**把物理地址转换成页表项要求的编码格式**。

原因是：物理页号等于物理地址右移 12 位，而页表项从第 10 位开始存放物理页号。因此：

```text
页表项中的地址部分
    = (物理地址 >> 12) << 10
    = 物理地址 >> 2
```

这里的物理地址按 1 GiB 对齐，也就满足了上述换算所需的对齐条件。

接着：

```asm
or t2, t2, t1
```

把地址部分和权限标志合并：

```text
地址部分：0x80000000 >> 2 = 0x20000000
权限标志：                  0x000000cf
合并结果：                  0x200000cf
```

最后：

```asm
sd t2, 16(t0)
```

把结果写入 `root[2]`。每项占 8 字节，所以第 2 项距离表头的偏移是 `2 × 8 = 16` 字节。

这样就得到了：

```text
虚拟地址 0x80000000～0xbfffffff
    ↓
物理地址 0x80000000～0xbfffffff
```

CPU 翻译地址时，会把表项给出的物理基址与地址在这段范围内的偏移组合起来。例如：

```text
访问虚拟地址：0x80200000
范围内偏移：  0x00200000
物理基址：    0x80000000
最终物理地址：0x80200000
```

项目用相同方法填写了 `root[0]` 到 `root[3]`，建立前 4 GiB 的恒等映射。到这里，低地址部分就准备好了。

下一步会把**同一片物理内存再映射到高虚拟地址**。这意味着两个不同的虚拟地址，可以访问同一个物理位置，并不需要复制内存。

先确认一个关键点：`sd t2, 16(t0)` 修改的是**根页表里的一个表项**，还是物理地址 `0x80000000` 处的内容？

## User

表项。为什么物理页号等于物理地址右移12位？解释临时页表的情况下，完整的一次地址访问。

## Assistant

对，写的是表项。你这两个问题正好能把“页表里存什么”和“CPU 怎么使用它”连起来。

先说为什么右移 12 位。

Sv39 的基本页大小是 **4 KiB，也就是 4096 = 2¹² 字节**。因此，可以把物理地址拆成两部分：

```text
物理地址 = 物理页号 × 4096 + 页内偏移
```

这和“第几栋楼、楼内第几个房间”类似：页号确定哪一页，偏移确定页里的哪个字节。

例如物理地址 `0x80200123`：

```text
物理页号：0x80200
页内偏移：0x123

0x80200 × 0x1000 + 0x123 = 0x80200123
```

4096 是 2 的 12 次方，所以地址的低 12 位恰好表示页内偏移，剩下的高位表示页号：

```c
物理页号 = 物理地址 >> 12;
页内偏移 = 物理地址 & 0xfff;
```

**右移 12 位就是去掉页内偏移，留下物理页号。**

你可能马上会问：启动页表用的是 1 GiB 大页，为什么还按 4 KiB 算物理页号？

因为 **PTE（页表项）中物理页号的编码单位仍然是 4 KiB**。实际映射大小由“在哪一级遇到了叶子表项”决定。我们在最顶层 L2 就遇到叶子，因此它映射 1 GiB；相应地，物理基址必须按 1 GiB 对齐。

---

现在走一次完整的地址访问。假设启动页表已经填好，并通过 `satp` 启用了 Sv39；我们访问：

```text
虚拟地址 VA = 0x0000000080200123
```

为了看清页表过程，先假设 TLB 中没有缓存这条映射。

**第一步：CPU 拆分虚拟地址。**

Sv39 的地址用于查表的部分是：

```text
位号       38 … 30      29 … 21      20 … 12      11 … 0
用途       L2 索引      L1 索引      L0 索引      4 KiB 页内偏移
位数         9            9            9              12
```

每级索引都是 9 位，所以每张页表有 `2⁹ = 512` 项。

对于这个地址：

```text
L2 索引 = 2
L1 索引 = 1
L0 索引 = 0
低 12 位 = 0x123
```

**第二步：CPU 从 `satp` 找到根页表。**

`satp` 不只记录“使用 Sv39”，还记录根页表的物理页号：

```text
根页表物理地址 = satp 中的根页表页号 << 12
```

硬件据此读取页表，寻找根页表时使用的是物理地址，不需要先翻译这个地址。

**第三步：读取 `root[2]`。**

每个表项占 8 字节：

```text
表项的物理地址 = 根页表物理地址 + 2 × 8
```

我们刚刚看到，启动代码在这里写入了：

```text
PTE = 0x200000cf
```

CPU 解析这个表项：

```text
V = 1             → 表项有效
R / W / X = 1     → 是叶子表项，具有读写执行权限
物理页号部分       → 对应物理基址 0x80000000
```

因为在 **L2 就遇到了叶子表项**，查表到这里结束，**不会继续访问 L1、L0 页表**。

**第四步：检查权限和对齐条件，组成物理地址。**

L2 叶子映射的是 1 GiB，即 `2³⁰` 字节。因此，虚拟地址的**低 30 位**都作为这张大页内的偏移：

```text
低 30 位 = L1 索引 + L0 索引 + 低 12 位
```

这里的加号表示拼接这些位，它们不再用于访问下一级页表。

本例中：

```text
物理基址：       0x80000000
1 GiB 内的偏移： 0x00200123
最终物理地址：   0x80200123
```

CPU 检查表项允许此次访问、物理基址满足大页对齐等条件后，就可以访问这个物理位置。若检查不通过，则会触发异常。

把整个过程串起来：

```text
VA 0x80200123
    → 从 satp 找到根页表
    → 用 L2 索引 2 读取 root[2]
    → 发现它是 1 GiB 叶子映射
    → 物理基址 0x80000000 + 偏移 0x200123
    → PA 0x80200123
```

翻译结果还可以被缓存在 TLB 中，后续访问命中时不必重新读取页表。

这份启动页表还让 `root[258]` 指向同一个物理基址，所以：

```text
低地址 VA：0x0000000080200123 ─┐
                             ├→ PA：0x80200123
高地址 VA：0xffffffc080200123 ─┘
```

这就是内核能从低地址跳到高地址、却仍然执行同一份物理内存中的代码的基础。

这里最值得记牢的是两个不同的“偏移”：**PTE 用 4 KiB 为单位编码物理页号；L2 叶子映射则用虚拟地址的低 30 位作为大页内偏移。**

## User

物理基址是什么时候如何确定的？

## Assistant

**物理基址是启动代码填写页表时指定的。CPU 查表时只是读取并解码它。**

这里有两个不同的“决定”，我们分开看。

第一层：**机器的 RAM 放在哪里？**

QEMU `virt` 平台规定 RAM 从物理地址 `0x80000000` 开始。这是虚拟机器的硬件布局，页表不负责决定它。内核建立映射，是让虚拟地址能够访问这些已经存在的物理位置。

第二层：**这个页表项映射到哪里？**

启动代码选择物理基址，并把它编码到表项中。对于 `root[2]`，就是我们看过的这四行：

```asm
li t2, 0x80000000       # 这里明确指定物理基址
srli t2, t2, 2         # 转成 PTE 中的地址编码
or t2, t2, t1          # 加入权限标志 0xcf
sd t2, 16(t0)          # 写入 root[2]
```

所以表项最终是：

```text
root[2] = 0x200000cf
          └ 地址编码 + 标志位
```

CPU 后来读取它，会取出 PTE 中的物理页号，再按 4 KiB 的单位还原地址。对这个表项，可以这样算：

```text
取出物理页号：0x200000cf >> 10 = 0x80000
还原物理基址：0x80000 << 12    = 0x80000000
```

这里右移 10 位，是因为 PTE 的低 10 位留给标志等信息；左移 12 位，是把物理页号转换回字节地址。这个表项没有设置更高位的其他字段，因此可以直接这样计算。

特别注意：**`root[2]` 的下标决定虚拟地址范围，表项里的物理页号决定物理地址范围。两者可以独立选择。**

| 内容 | 决定什么 | 本例 |
|---|---|---|
| 写到 `root[2]` | 哪段虚拟地址使用这条映射 | 从 `0x80000000` 开始的 1 GiB |
| 表项中的物理页号 | 映射到哪段物理地址 | 从 `0x80000000` 开始的 1 GiB |

它们在这里相同，是因为启动代码主动建立了恒等映射。假如把同样的表项写入 `root[3]`，那就是：

```text
虚拟地址从 0xc0000000 开始
    → 物理地址从 0x80000000 开始
```

物理内存不会因此移动，只是多了一条访问它的地址对应关系。

因此，回答“什么时候确定”：**在执行 `li t2, 0x80000000` 时选定目标，在执行 `sd` 时把这个选择写入页表；开启地址翻译后，CPU 按这个选择进行访问。**

## User

给出页表项格式和对于虚拟地址的解析格式

## Assistant

我会把 Sv39 的虚拟地址和页表项按位对齐列出来，并对照项目中的启动大页解释。

## Activity

    Searched the web for site.docs.riscv.org Sv39 PTE bits 53 10 virtual address VPN

## Assistant

下面都按 MiniCore 使用的 **Sv39** 来看。位号从 `0` 开始，左边是高位。

**1. 虚拟地址：决定“查哪一项”和“页内偏移”**

CPU 使用的是 64 位地址，但 Sv39 要求高 25 位全部等于第 38 位：

```text
 63               39 38       30 29       21 20       12 11          0
┌───────────────────┬───────────┬───────────┬───────────┬─────────────┐
│ 全部复制 bit 38   │  VPN[2]   │  VPN[1]   │  VPN[0]   │ 页内偏移    │
│      25 位        │   9 位    │   9 位    │   9 位    │   12 位     │
└───────────────────┴───────────┴───────────┴───────────┴─────────────┘
                       L2 索引     L1 索引     L0 索引
```

`VPN` 是虚拟页号的意思，这里的三个部分分别用作三级页表的索引。高位不满足上述规则会触发页故障，并不是直接忽略。[RISC-V 官方规范](https://docs.riscv.org/reference/isa/priv/supervisor.html)

用代码提取就是：

```c
vpn2   = (va >> 30) & 0x1ff;  // 取 9 位
vpn1   = (va >> 21) & 0x1ff;
vpn0   = (va >> 12) & 0x1ff;
offset =  va        & 0xfff;  // 取 12 位
```

例如我们之前的地址：

```text
VA = 0x0000000080200123

VPN[2] = 2
VPN[1] = 1
VPN[0] = 0
offset = 0x123
```

**2. 页表项 PTE：记录“到哪里去”和“允许做什么”**

一个 Sv39 页表项占 **64 位，也就是 8 字节**：

```text
 63     54 53           28 27       19 18       10 9     8 7             0
┌─────────┬───────────────┬───────────┬───────────┬───────┬───────────────┐
│扩展/保留│    PPN[2]     │  PPN[1]   │  PPN[0]   │  RSW  │ D A G U X W R V│
│ 10 位   │    26 位      │   9 位    │   9 位    │ 2 位  │ 各占 1 位     │
└─────────┴───────────────┴───────────┴───────────┴───────┴───────────────┘
```

`PPN` 是**物理页号**，三个部分合起来共 44 位。高位中的 bit 63 是 N、bits 62:61 是 PBMT，其余 bits 60:54 保留；我们正在读的启动表项把这些位都置零。[RISC-V 官方格式](https://docs.riscv.org/reference/isa/_attachments/riscv-privileged.pdf)

低位的含义如下：

| 位号 | 字段 | 含义 |
|---|---|---|
| 9:8 | RSW | 留给软件使用，硬件地址翻译忽略它 |
| 7 | D | 脏标志，表示发生过写入 |
| 6 | A | 访问标志 |
| 5 | G | 全局映射标志 |
| 4 | U | 是否允许用户态访问 |
| 3 | X | 可执行 |
| 2 | W | 可写 |
| 1 | R | 可读 |
| 0 | V | 表项有效 |

判断表项时，先检查有效性和合法编码，再判断是否为叶子：

```text
V = 0              → 无效
R = 0、W = 1       → 非法编码
R/W/X 全为 0       → 指向下一级页表
R = 1 或 X = 1     → 叶子表项，直接映射物理内存
```

以上是基础 Sv39 的相关规则。[RISC-V 官方规范](https://docs.riscv.org/reference/isa/priv/supervisor.html)

**3. 把两种格式连起来**

虚拟地址中的 `VPN` 用来**选择表项**；被选中的表项里的 `PPN` 用来**确定物理位置**。

```text
虚拟地址 VPN[2]
      │ 作为索引
      ▼
根页表中的 PTE
      │
      ├─ 非叶子 → PPN 指向下一级页表，再用 VPN[1] 查找
      │
      └─ 叶子   → PPN 提供物理基址，结合虚拟地址中的偏移
```

在哪里遇到叶子，决定映射大小，以及多少位属于偏移：

| 叶子所在层级 | 映射大小 | 物理基址对齐要求 | 虚拟地址中作为偏移的部分 |
|---|---|---|---|
| L2，根页表 | 1 GiB | 1 GiB | 低 30 位 |
| L1 | 2 MiB | 2 MiB | 低 21 位 |
| L0 | 4 KiB | 4 KiB | 低 12 位 |

大页对应的低位 PPN 必须为零以满足对齐，否则触发页故障。[RISC-V 官方规范](https://docs.riscv.org/reference/isa/priv/supervisor.html)

对于项目中的启动映射，完整对应关系就是：

```text
VA = 0x80200123
     │
     ├─ VPN[2] = 2 → 读取 root[2] = 0x200000cf
     │                              │
     │                              ├─ L2 叶子：映射 1 GiB
     │                              └─ 物理基址：0x80000000
     │
     └─ 低 30 位 = 0x200123 ────────────┐
                                        ▼
                   PA = 0x80000000 + 0x200123
                      = 0x80200123
```

这里 `VPN[1]`、`VPN[0]` 没有继续用来查表，而是一起成为了 **1 GiB 大页内的偏移**。

## User

物理页号有什么用

## Assistant

**物理页号用来告诉 CPU：这条映射对应哪一块物理内存。** 它是物理地址去掉低 12 位之后的编号，以 4 KiB 为单位。

例如：

```text
物理页号 PPN = 0x80200
       ↓ 左移 12 位
物理页起始地址 = 0x80200000
```

如果要访问这一页内偏移 `0x123` 的字节：

```text
物理地址 = (PPN << 12) + 偏移
         = 0x80200000 + 0x123
         = 0x80200123
```

为什么页表项存“页号”，而不存完整的物理地址？因为页的起始地址按 4 KiB 对齐，低 12 位必然为零，没必要重复存储。**存页号就足够还原起始地址，省下来的位可以用于权限等信息。**

不过，PPN 具体指向什么，要看这个表项是哪一种：

| 表项类型 | PPN 的用途 |
|---|---|
| 非叶子表项 | 找到**下一级页表**所在的物理页 |
| 叶子表项 | 找到**实际映射的内存区域**的物理基址 |

所以 CPU 查表时，会反复做这样的事情：

```text
读到非叶子 → 用 PPN 找下一张表
读到叶子   → 用 PPN 和访问地址的偏移，算出最终物理地址
```

对我们正在读的临时页表，`root[2]` 是一个 L2 叶子：

```text
root[2] 中的 PPN = 0x80000
物理基址        = 0x80000 << 12 = 0x80000000
```

它映射 1 GiB，因此再加上虚拟地址的低 30 位偏移。

你可以这样区分我们遇到的两个编号：**VPN 用来选择页表中的哪一项，PPN 用来指出这一项通向哪里。**

## User

PPN为什么分成三段？

## Assistant

**PPN 分成三段，是为了对应三级页表，让 CPU 能按不同页大小拼出最终物理地址。** 三段合起来仍然是一个物理页号，并不是拿它们分别去查三张表。

把虚拟地址和物理地址并排看：

```text
虚拟地址：
┌─────────┬─────────┬─────────┬──────────┐
│ VPN[2]  │ VPN[1]  │ VPN[0]  │ 页内偏移 │
│  9 位   │  9 位   │  9 位   │  12 位  │
└─────────┴─────────┴─────────┴──────────┘

物理地址：
┌─────────┬─────────┬─────────┬──────────┐
│ PPN[2]  │ PPN[1]  │ PPN[0]  │ 页内偏移 │
│  26 位  │  9 位   │  9 位   │  12 位  │
└─────────┴─────────┴─────────┴──────────┘
```

关键是：**映射的页越大，最终物理地址中直接来自虚拟地址的部分就越多。**

如果走到 L0 才遇到叶子，映射的是 **4 KiB 页**。整个物理页号都由 PTE 提供，只有低 12 位来自虚拟地址：

```text
物理地址 = [PTE.PPN[2] | PTE.PPN[1] | PTE.PPN[0] | VA低12位]
```

如果在 L1 就遇到叶子，映射的是 **2 MiB 大页**。需要 21 位表示页内偏移，因此虚拟地址中的 `VPN[0]` 也成为偏移的一部分：

```text
物理地址 = [PTE.PPN[2] | PTE.PPN[1] | VA.VPN[0] | VA低12位]
```

这时要求表项的 `PPN[0] = 0`，保证物理基址按 2 MiB 对齐。

如果在 L2 就遇到叶子，也就是我们正在读的启动映射，映射的是 **1 GiB 大页**。需要 30 位表示偏移：

```text
物理地址 = [PTE.PPN[2] | VA.VPN[1] | VA.VPN[0] | VA低12位]
```

这时要求表项的 `PPN[1]`、`PPN[0]` 都为零，保证物理基址按 1 GiB 对齐。上面的 `|` 表示位段拼接。[RISC-V 官方规范](https://docs.riscv.org/reference/isa/priv/supervisor.html)

所以，把 PPN 分成三段后，就能清楚表达：**哪些位由页表替换，哪些位保留虚拟地址的原值。**

对 MiniCore 的临时页表而言，可以直接记住：L2 叶子只提供最终物理地址的高位，低 30 位从虚拟地址原样带过去。

## User

PPN和页内偏移加起来才56位，不够64啊

## Assistant

你算得对：**Sv39 的页表格式能编码 56 位物理地址，并不是 64 位。** 我前面的图没有标明这个宽度，容易让你以为还缺了 8 位。

“64 位 CPU”并不意味着虚拟地址和物理地址都能使用完整的 64 位。这里有三个不同的宽度：

| 项目 | Sv39 中的情况 |
|---|---|
| 通用寄存器 | 64 位 |
| 虚拟地址 | 用 64 位保存，但高 25 位必须复制 bit 38 |
| 页表可编码的物理地址 | 44 位 PPN + 12 位偏移，共 56 位 |

因此，把翻译出来的物理地址放在一个 64 位无符号数里，可以画成：

```text
 63      56 55             30 29       21 20       12 11         0
┌──────────┬────────────────┬───────────┬───────────┬────────────┐
│    0     │     PPN[2]     │  PPN[1]   │  PPN[0]   │ 页内偏移   │
│   8 位   │     26 位      │   9 位    │   9 位    │   12 位    │
└──────────┴────────────────┴───────────┴───────────┴────────────┘
```

这表示该格式最多能编码 `2⁵⁶` 字节，也就是 **64 PiB 的物理地址空间**；具体机器实际支持的物理地址位数和安装的内存都可以更少。[RISC-V 官方规范](https://docs.riscv.org/reference/isa/priv/supervisor.html)

还要区分：**PTE 本身是 64 位，但它不是一个完整的物理地址。** 它的 64 位分配给了：

```text
高位扩展/保留 10 位 + PPN 44 位 + 低位标志等 10 位 = 64 位
```

页内偏移不存储在 PTE 中，而是访问发生时从虚拟地址中取得。

所以没有丢失 8 位：Sv39 本来就只用这个格式编码最多 56 位的物理地址。

## User

好的，那请你分别带我过一遍1GiB页的访问和正常4KiB页的访问流程

## Assistant

我用同一个虚拟地址分别走两遍，重点对比：在哪里停止查表，以及最终物理地址的哪些位来自页表。

## Activity

    Opened https://docs.riscv.org/reference/isa/priv/supervisor.html

## Activity

    Searched the web

## Assistant

好。我们让 CPU 执行一次“读取一个字节”，两遍都使用同一个虚拟地址：

```text
VA = 0x0000000080200123

VPN[2] = 2
VPN[1] = 1
VPN[0] = 0
低 12 位偏移 = 0x123
```

假设 Sv39 已开启，TLB 没有缓存这条映射，权限等检查均能通过。**页表已经由内核提前填好；下面查表的是 CPU 硬件。**

### 第一遍：通过 1 GiB 大页访问

这对应我们正在读的启动映射，`root[2]` 是叶子表项。

**① 从 `satp` 找到根页表。**

`satp` 保存根页表的物理页号：

```text
根页表物理地址 = satp.PPN << 12
```

**② 用 `VPN[2] = 2` 选择表项。**

每个 PTE 占 8 字节，因此 CPU 从以下物理位置读取 PTE：

```text
根页表物理地址 + 2 × 8
```

读取到：

```text
root[2] = 0x200000cf
```

其中，`V=1`，`R/W/X=1`。CPU 认出它是一个有效叶子，**查表在 L2 停止**。

**③ 从表项中取得物理基址。**

```text
PPN = 0x80000

物理基址 = PPN << 12
         = 0x80000000
```

因为这是 L2 叶子，映射大小为 1 GiB；这个物理基址满足 1 GiB 对齐。

**④ 从虚拟地址取得大页内偏移。**

1 GiB 是 `2³⁰` 字节，因此取虚拟地址的低 30 位：

```text
偏移 = VA & 0x3fffffff
     = 0x00200123
```

这里的 `VPN[1]` 和 `VPN[0]` **不再用于查表，一起成为偏移**：

```text
低 30 位 = [VPN[1]，9 位][VPN[0]，9 位][低 12 位]
```

**⑤ 访问最终物理地址。**

```text
PA = 物理基址   + 大页内偏移
   = 0x80000000 + 0x00200123
   = 0x80200123
```

CPU 从这个物理地址读取所需的字节。

```text
satp → 根页表
           └─ root[2]：叶子
                  └─ 物理基址 + 低 30 位偏移 → 数据
```

### 第二遍：通过 4 KiB 页访问

这次我们**假设内核建立了另一套页表**，用来演示三级查找。下面的页表地址和目标地址都是教学示例，不是项目实际分配结果：

| 对象 | 物理地址 |
|---|---|
| L2 根页表 | `0x81000000` |
| 一张 L1 页表 | `0x81001000` |
| 一张 L0 页表 | `0x81002000` |
| 最终数据页 | `0x82005000` |

注意，这四块内存各有用途：前三块存表项，最后一块存我们想读的数据。

**① 从 `satp` 找到根页表。**

```text
satp.PPN = 0x81000
根页表物理地址 = 0x81000000
```

**② 用 `VPN[2] = 2` 读取 L2 表项。**

```text
读取位置 = 0x81000000 + 2 × 8
         = 0x81000010
```

这次内核填写的表项内容是：

```text
V = 1，R/W/X = 0 → 非叶子
PPN = 0x81001
```

所以这个 PPN 指向的是**下一张页表**：

```text
L1 页表物理地址 = 0x81001 << 12
               = 0x81001000
```

**③ 用 `VPN[1] = 1` 读取 L1 表项。**

```text
读取位置 = 0x81001000 + 1 × 8
         = 0x81001008
```

该表项也是非叶子，里面保存：

```text
PPN = 0x81002
```

因此继续找到：

```text
L0 页表物理地址 = 0x81002 << 12
               = 0x81002000
```

**④ 用 `VPN[0] = 0` 读取 L0 表项。**

```text
读取位置 = 0x81002000 + 0 × 8
         = 0x81002000
```

这次表项有效且允许读取，是一个叶子，里面保存：

```text
PPN = 0x82005
```

因为在 L0 遇到叶子，所以映射的是 4 KiB 数据页：

```text
数据页物理基址 = 0x82005 << 12
               = 0x82005000
```

**⑤ 加上低 12 位偏移，读取数据。**

```text
PA = 物理基址   + 页内偏移
   = 0x82005000 + 0x123
   = 0x82005123
```

这次，虚拟地址 `0x80200123` 对应的是物理地址 `0x82005123`。虚拟地址和物理地址无需相同。

```text
satp → L2 页表
           └─ [2] → L1 页表
                       └─ [1] → L0 页表
                                   └─ [0] → 数据页
                                               └─ + 0x123 → 数据
```

两种路径的区别集中在这里：

| 对比项 | 1 GiB 页 | 4 KiB 页 |
|---|---|---|
| 在哪层遇到叶子 | L2 | L0 |
| 完整遍历读取几个 PTE | 1 个 | 3 个 |
| 哪些位用于查表 | `VPN[2]` | 三段 VPN |
| 哪些位成为偏移 | VA 低 30 位 | VA 低 12 位 |

这些是 Sv39 的标准翻译规则；实际命中 TLB 时，可以省去上述页表遍历。[RISC-V 官方规范](https://docs.riscv.org/reference/isa/v20260120/priv/supervisor.html)

留一个关键问题帮你检查理解：**第二遍中，L2 表项的 PPN 为什么不能直接拿来加 `0x123`，作为最终数据地址？**

## User

PPN = 0x80000只有20位啊，总的PPN不是有44位吗

## Assistant

这里要区分：**字段的宽度是 44 位，里面存的数值不一定需要用满 44 位。**

就像一个 32 位整数变量，可以保存数字 `5`，并不要求这个数一定有 32 位有效数字：

```c
uint32_t x = 5;
```

`PPN = 0x80000` 也是如此。我们平时写十六进制数会省略前导零。44 位对应 **11 个十六进制数字**，补齐后就是：

```text
简写：0x80000
完整：0x00000080000
```

这两个写法表示完全相同的数值。

把它拆成 PPN 的三段，可以看得更清楚：

```text
PPN[2]：26 位，值为 2
PPN[1]： 9 位，值为 0
PPN[0]： 9 位，值为 0
```

因为：

```text
PPN = (PPN[2] << 18) | (PPN[1] << 9) | PPN[0]
    = 2 << 18
    = 0x80000
```

再转换为物理基址：

```text
物理基址 = PPN << 12
         = 0x80000000
```

所以，**44 位是 PPN 能存储的容量；`0x80000` 是这次实际存进去的值，高位都是零。**

## User

好的，我理解了，继续之前的流程

## Assistant

我们接回 `_start`：接下来把同一片物理内存映射到高地址，然后让 CPU 启用这张启动页表。

## Activity

    $ sed -n '65,160p' src/arch/riscv/head.S
         *   root[258] -> PA 0x80000000
         *   root[259] -> PA 0xc0000000
         * root[256] 对应的规范化 Sv39 虚拟地址是
         * 0xffffffc000000000，即 KERNEL_DIRECT_BASE。
         *
         * root[256] 的字节偏移为 256 * 8 = 2048，但 store 的 12 位
         * 有符号立即数最大只能表示 2047，所以先让 t0 前进 2040 字节，
         * 再用 8、16、24、32 作为剩余偏移。 */
        addi t0, t0, 2040
        sd t1, 8(t0)                /* root[256] = root[0] */
        ld t2, -2032(t0)            /* 读取 root[1] */
        sd t2, 16(t0)               /* root[257] = root[1] */
        ld t2, -2024(t0)            /* 读取 root[2] */
        sd t2, 24(t0)               /* root[258] = root[2] */
        ld t2, -2016(t0)            /* 读取 root[3] */
        sd t2, 32(t0)               /* root[259] = root[3] */
    
        /* 恢复 t0，使它再次指向根页表的物理起始地址。 */
        addi t0, t0, -2040
    
        /* 构造 satp：
         *   bits 63:60 MODE = 8，表示 Sv39；
         *   bits 59:44 ASID = 0；
         *   bits 43:0  PPN = 根页表物理地址 >> 12。
         * 写 satp 后地址翻译立即生效；低地址恒等映射保证当前指令流
         * 仍能继续执行，随后再显式跳到高半区。 */
        srli t0, t0, 12
        li t1, 8
        slli t1, t1, 60
        or t0, t0, t1
        csrw satp, t0
        /* 丢弃旧的地址翻译，并保证后续取指/访存使用刚写入的页表。 */
        sfence.vma zero, zero
    
        /* .Lhigh_entry 位于高半区，与当前低地址 PC 相距太远，普通直接跳转
         * 无法编码。链接器把完整的 64 位高地址写入下面的 .dword，这里先
         * 从仍由恒等映射访问的 .text.boot 中读出它，再间接跳转。 */
        la t0, .Lhigh_entry_address
        ld t0, 0(t0)
        jr t0
    
        .align 3
    .Lhigh_entry_address:
        .dword .Lhigh_entry
        /* _start 的尺寸只计算低地址 .text.boot 部分；高地址入口属于 .text。 */
        .size _start, . - _start
    
        .section .text, "ax", @progbits
        .align 2
    .Lhigh_entry:
        /* 现在 PC 已经是高半区虚拟地址。初始化 gp 时必须关闭 linker
         * relaxation，否则“给 gp 赋初值”的指令本身可能被优化成依赖 gp。 */
        .option push
        .option norelax
        la gp, __global_pointer$
        .option pop
    
        /* 切换到链接器脚本预留的高半区启动栈。栈向低地址增长。 */
        la sp, __boot_stack_top
    
        /* .bss 是 ELF 的 NOBITS 区域，不在镜像中存放零字节，因此内核负责
         * 清零。此循环本身不使用栈，所以即使启动栈也位于 .bss 中仍安全。 */
        la t0, __bss_start
        la t1, __bss_end
    .Lclear_bss:
        bgeu t0, t1, .Lbss_done
        sd zero, 0(t0)
        addi t0, t0, 8
        j .Lclear_bss
    .Lbss_done:
        /* 按 C ABI 恢复 kernel_main 的两个参数。hart ID 可以直接恢复；
         * DTB 仍是物理地址，需要加 direct-map 基址，变成当前可解引用的
         * 高半区虚拟地址。临时页表已经映射了对应的前 4 GiB 物理空间。 */
        mv a0, s0
        li t0, 0xffffffc000000000
        add a1, s1, t0
        call kernel_main
    
        /* kernel_main 声明为 _Noreturn；若意外返回则进入 panic。 */
        call panic_returned_from_kernel_main
    .Lhang:
        wfi
        j .Lhang
    
        /* 静态启动根页表：NOBITS 表示 ELF 文件不保存 4096 字节零数据，
         * 但加载段会为它保留内存。页表必须按 4 KiB（2^12）对齐。 */
        .section .bss.boot, "aw", @nobits
        .align 12
        .globl boot_root_page
    boot_root_page:
        .space 4096
    
        .section .note.GNU-stack, "", @progbits
    ✓ • 0ms

## Assistant

我们回到 `_start`。目前已经完成：

```text
保存启动参数 → 清空根页表 → 填好 root[0]～root[3] 的恒等映射
```

**此时 CPU 还没使用页表，我们只是在内存中把它填好。**

接下来，项目把同样的映射再放到 `root[256]`～`root[259]`，使同一片物理内存也能通过高虚拟地址访问：

| 根表项 | 虚拟起始地址 | 物理起始地址 |
|---|---|---|
| `root[256]` | `0xffffffc000000000` | `0x00000000` |
| `root[257]` | `0xffffffc040000000` | `0x40000000` |
| `root[258]` | `0xffffffc080000000` | `0x80000000` |
| `root[259]` | `0xffffffc0c0000000` | `0xc0000000` |

每项映射 1 GiB。这里没有复制物理内存，只是复制了描述映射的表项。

为什么 `root[256]` 对应高地址？因为 `256` 的九位二进制是 `100000000`。它作为 `VPN[2]`，让虚拟地址的 bit 38 为 `1`，高 25 位也必须全部为 `1`，于是得到表中的高地址。

我们先看实际填写代码：

```asm
addi t0, t0, 2040
sd t1, 8(t0)             # 填写 root[256]

ld t2, -2032(t0)         # 读取 root[1]
sd t2, 16(t0)            # 写入 root[257]

ld t2, -2024(t0)         # 读取 root[2]
sd t2, 24(t0)            # 写入 root[258]

ld t2, -2016(t0)         # 读取 root[3]
sd t2, 32(t0)            # 写入 root[259]

addi t0, t0, -2040       # 回到根页表开头
```

这里看起来奇怪的 `2040`，是为了绕开指令的立即数限制。`sd` 的偏移最大只能直接写 `2047`，而 `root[256]` 的偏移已经是：

```text
256 × 8 = 2048
```

所以代码先移动指针，再加一个小偏移：

```text
新 t0 = 表头 + 2040
8(t0) = 表头 + 2048 = root[256] 的位置
```

读取 `root[1]` 也是同样的算术：

```text
新 t0 - 2032 = 表头 + 8 = root[1] 的位置
```

现在两组映射都准备好了，终于可以**启用地址翻译**：

```asm
srli t0, t0, 12
li t1, 8
slli t1, t1, 60
or t0, t0, t1
csrw satp, t0
sfence.vma zero, zero
```

这里构造的是 `satp`，注意它和 PTE 是不同的格式：

```text
63       60 59             44 43                         0
┌──────────┬────────────────┬────────────────────────────┐
│ MODE = 8 │    ASID = 0    │       根页表物理页号        │
└──────────┴────────────────┴────────────────────────────┘
```

逐行对应：

```asm
srli t0, t0, 12      # 根页表物理地址 → 根页表物理页号
li t1, 8
slli t1, t1, 60      # 把 8 放到 MODE 字段，表示 Sv39
or t0, t0, t1       # 合并模式与根页表页号，ASID 保持为 0
csrw satp, t0       # 写入 satp，让 CPU 使用这张页表
```

随后 `sfence.vma zero, zero` 用于同步页表更新与后续地址翻译，并使本 hart 的相关旧翻译缓存失效。

**写入 `satp` 不会自动把 PC 改成高地址。** CPU 此时仍在原来的低地址继续执行，但取指已经需要经过地址翻译。之前建立的恒等映射保证：同一个低地址仍然找到原来的物理指令。

接下来还需要一次显式跳转，才能进入高地址代码。

先确认这个交接点：**如果只建立高地址映射，没有保留当前代码所在的低地址恒等映射，开启 Sv39 后，CPU 继续从当前低地址取指时会遇到什么问题？**

## User

页表中没有映射要访问的低虚拟地址，但是已启用MMU所以会触发page fault.sfence.vma zero, zero 具体干了什么？

## Assistant

你的判断对。我查一下规范中 `sfence.vma` 的精确定义，再结合这两条指令的先后关系解释。

## Activity

    Opened https://docs.riscv.org/reference/isa/v20260120/priv/supervisor.html

## Activity

    Searched the web for site:docs.riscv.org "If rs1=x0 and rs2=x0" "SFENCE.VMA"

## Assistant

对，这里会发生**取指页故障（instruction page fault）**。

`sfence.vma zero, zero` 可以理解为：**让当前 CPU 核的地址翻译机制与刚才写好的页表同步。** 它包含两件事，不能只理解为“清空 TLB”。

先看为什么需要它。

CPU 为了加速访问，会缓存地址翻译结果，例如：

```text
内存中的页表：VA → PA
                  ↓ 查表后缓存
TLB：           VA → PA，以及权限
```

假设内核修改了页表：

```text
修改前：虚拟页 A → 物理页 B
修改后：虚拟页 A → 物理页 C
```

页表在内存中已经改了，但 TLB 可能仍然记着 `A → B`。如果继续使用这个缓存，就可能访问旧位置。

**第一件事：使旧的地址翻译缓存失效。**

执行 `sfence.vma zero, zero` 后，本核原有的地址翻译缓存不能再作为有效结果使用。后续访问需要根据同步后的页表获得翻译。这里不只涉及通常所说的 TLB，也涵盖其他地址翻译缓存。

**第二件事：保证之前的页表写入，对后续硬件查表生效。**

填写页表用的是普通存储指令：

```asm
sd t2, 16(t0)
```

而硬件查表是 CPU 自动进行的内存读取。`sfence.vma` 在两者之间建立必要的顺序：

```text
此前写入页表
      ↓
sfence.vma
      ↓
后续指令触发的硬件查表，要遵守此前的页表更新
```

因此，它既处理“缓存了旧结果”，也处理“页表写入与硬件查表之间的同步”。[RISC-V 官方规范](https://docs.riscv.org/reference/isa/priv/supervisor.html)

两个 `zero` 决定作用范围：

```asm
sfence.vma rs1, rs2
```

| 参数 | 通常用来指定 | 使用 `zero` 的含义 |
|---|---|---|
| `rs1` | 虚拟地址 | 所有虚拟地址 |
| `rs2` | 地址空间编号 ASID | 所有地址空间，包括全局映射 |

这里 `zero` 是特殊寄存器 `x0`，表示“不限制范围”，**不是仅处理虚拟地址 0 和 ASID 0**。作用仍然限于当前 hart，不会自动同步其他核心。[RISC-V 官方规范](https://docs.riscv.org/reference/isa/priv/supervisor.html)

回到项目：

```asm
csrw satp, t0
sfence.vma zero, zero
```

两条指令分别负责：

```text
csrw satp：
    选择 Sv39 模式和根页表。

sfence.vma：
    同步此前的页表写入，使本核旧的翻译缓存失效。
```

**写 `satp` 本身不替你完成上述同步。** 另外，`sfence.vma` 不会清空内存中的页表，也不是清空普通数据缓存；它针对的是地址翻译机制。

## User

好的继续

## Assistant

现在，CPU 已启用启动页表，仍然通过低地址恒等映射执行代码。接下来要做的是：**把 PC 跳转到高虚拟地址，进入高地址入口。**

项目里的代码是：

```asm
la t0, .Lhigh_entry_address
ld t0, 0(t0)
jr t0
```

后面紧接着存放了一个地址：

```asm
.align 3
.Lhigh_entry_address:
    .dword .Lhigh_entry
```

这里有两个名字，很容易混淆：

- `.Lhigh_entry`：接下来要执行的代码入口。
- `.Lhigh_entry_address`：一块存放该入口地址的内存。

它们的关系是：

```text
.Lhigh_entry_address 所在的内存
┌──────────────────────────────┐
│ 保存着 .Lhigh_entry 的高地址  │
└──────────────────────────────┘
                 │
                 ▼
        .Lhigh_entry 的代码
```

我们逐条走。

**第一条：取出“存地址的地方”的地址。**

```asm
la t0, .Lhigh_entry_address
```

执行后，`t0` 指向那个八字节存储位置。此时 `t0` 还不是最终跳转目标。

**第二条：读取里面保存的高地址。**

```asm
ld t0, 0(t0)
```

`ld` 从内存读取八字节，放入寄存器。这里的效果相当于：

```c
t0 = *(uint64_t *)t0;
```

执行后，`t0` 才变成 `.Lhigh_entry` 的高虚拟地址。

这个地址来自：

```asm
.dword .Lhigh_entry
```

`.dword` 是汇编指示，不是 CPU 执行的指令。它让构建工具在这里存放一个八字节数值，最终由链接器确定 `.Lhigh_entry` 的地址并填入。

`.align 3` 则让这个存储位置按 `2³ = 8` 字节对齐。

**第三条：跳过去。**

```asm
jr t0
```

它把控制流转到 `t0` 指定的地址，不保存返回地址。这里是启动流程的一次转移，没有打算再返回低地址入口。

为什么先从内存读取地址，再跳转？因为高地址入口距离当前低地址 PC 太远，普通直接跳转指令无法编码这个距离。这样先取得完整的 64 位地址，就能通过寄存器跳过去。

跳转之后：

```text
PC 变成高虚拟地址
       ↓
CPU 通过启动页表翻译它
       ↓
找到 RAM 中对应的物理指令
       ↓
开始执行 .Lhigh_entry
```

**代码没有被搬家，改变的是 CPU 用来访问代码的虚拟地址。** 前面建立的高地址映射，现在正式派上了用场。

我们先把这三条指令理解扎实：如果删掉中间的 `ld t0, 0(t0)`，直接执行 `jr t0`，CPU 会跳到真正的高地址入口，还是跳到存放入口地址的那块内存？

## User

后者。为什么离得太远普通跳转无法执行？

## Assistant

对，少了 `ld` 就会跳到存放地址的数据处。我核对一下跳转指令的编码范围，说明“太远”具体受什么限制。

## Activity

    Searched the web for site:docs.riscv.org JAL signed offset range ±1 MiB JALR 12 bit

## Assistant

因为**直接跳转指令只能用有限的位数记录“从当前位置跳多远”**。

这里的“远”是地址数值之差，和代码在物理内存中是否相邻无关。

例如：

```asm
j target
```

它是伪指令，可以编码成：

```asm
jal zero, target
```

这条 `jal` 指令本身只有 **32 位**，其中还要记录操作类型、目标寄存器等信息，不能塞入完整的 64 位目标地址。它采用的办法是记录一个相对于当前 PC 的偏移：

```text
目标地址 = 当前 PC + 指令中编码的偏移
```

这个偏移的范围是：

```text
−1 MiB ～ +1 MiB − 2 字节
```

也就是说，一条 `jal` 只能直接跳到当前位置前后大约 1 MiB 的范围内。[RISC-V 指令规范](https://docs.riscv.org/reference/isa/v20240411/unpriv/rv32.html)

举一个示意例子：

```text
当前 PC：0x0000000080200000
目标地址：0x0000000080200100

偏移：+0x100，也就是 +256 字节
```

这个偏移放得下，可以直接跳转。

但我们的启动过程要从低地址跳到类似下面的高地址：

```text
当前 PC：0x000000008020....
目标地址：0xffffffc08020....
```

两者的差远超 `jal` 的范围，无法把这个偏移编码进一条指令。直接这样写，通常会在汇编或链接阶段报出距离超限，而不是运行时让 CPU 尝试一次“跳太远”。

那为什么 `jr t0` 可以？

```asm
jr t0
```

它使用寄存器间接跳转，相当于：

```asm
jalr zero, 0(t0)
```

目标地址来自：

```text
目标地址 = (t0 + 0)，并将最低位清零
```

`t0` 是 **64 位寄存器**，能够保存完整的高地址。指令只需要编码“使用 t0”，不需要把那个高地址塞进指令本身，所以不受“距离当前 PC 多远”的限制。当然，目标地址仍然需要有合法的映射和执行权限。[RISC-V 指令规范](https://docs.riscv.org/reference/isa/v20240411/_attachments/riscv-unprivileged.pdf)

因此，项目用了这个组合：

```asm
la t0, .Lhigh_entry_address  # 找到附近存放地址的位置
ld t0, 0(t0)                # 从中读出完整的 64 位高地址
jr t0                       # 使用寄存器中的地址跳转
```

**直接跳转把“距离”放在指令里；寄存器间接跳转把“目标地址”放在寄存器里。** 这就是两者能到达的范围不同的原因。

## User

好的继续

## Assistant

接下来进入高地址入口，准备 C 代码需要的寄存器和栈。我会对照链接脚本，把 `gp`、`sp` 指向的具体位置一起讲清楚。

## Activity

    $ sed -n '105,165p' src/arch/riscv/head.S && sed -n '1,220p' src/linker/linker.ld
    
        .align 3
    .Lhigh_entry_address:
        .dword .Lhigh_entry
        /* _start 的尺寸只计算低地址 .text.boot 部分；高地址入口属于 .text。 */
        .size _start, . - _start
    
        .section .text, "ax", @progbits
        .align 2
    .Lhigh_entry:
        /* 现在 PC 已经是高半区虚拟地址。初始化 gp 时必须关闭 linker
         * relaxation，否则“给 gp 赋初值”的指令本身可能被优化成依赖 gp。 */
        .option push
        .option norelax
        la gp, __global_pointer$
        .option pop
    
        /* 切换到链接器脚本预留的高半区启动栈。栈向低地址增长。 */
        la sp, __boot_stack_top
    
        /* .bss 是 ELF 的 NOBITS 区域，不在镜像中存放零字节，因此内核负责
         * 清零。此循环本身不使用栈，所以即使启动栈也位于 .bss 中仍安全。 */
        la t0, __bss_start
        la t1, __bss_end
    .Lclear_bss:
        bgeu t0, t1, .Lbss_done
        sd zero, 0(t0)
        addi t0, t0, 8
        j .Lclear_bss
    .Lbss_done:
        /* 按 C ABI 恢复 kernel_main 的两个参数。hart ID 可以直接恢复；
         * DTB 仍是物理地址，需要加 direct-map 基址，变成当前可解引用的
         * 高半区虚拟地址。临时页表已经映射了对应的前 4 GiB 物理空间。 */
        mv a0, s0
        li t0, 0xffffffc000000000
        add a1, s1, t0
        call kernel_main
    
        /* kernel_main 声明为 _Noreturn；若意外返回则进入 panic。 */
        call panic_returned_from_kernel_main
    .Lhang:
        wfi
        j .Lhang
    
        /* 静态启动根页表：NOBITS 表示 ELF 文件不保存 4096 字节零数据，
         * 但加载段会为它保留内存。页表必须按 4 KiB（2^12）对齐。 */
        .section .bss.boot, "aw", @nobits
        .align 12
        .globl boot_root_page
    boot_root_page:
        .space 4096
    
        .section .note.GNU-stack, "", @progbits
    OUTPUT_ARCH(riscv)
    ENTRY(_start)
    
    KERNEL_PHYS_BASE = 0x80200000;
    KERNEL_DIRECT_BASE = 0xffffffc000000000;
    KERNEL_VIRT_BASE = KERNEL_DIRECT_BASE + KERNEL_PHYS_BASE;
    
    SECTIONS
    {
        . = KERNEL_PHYS_BASE;
        __kernel_phys_start = .;
        .text.boot : ALIGN(4K) { KEEP(*(.text.boot)) }
        .bss.boot (NOLOAD) : ALIGN(4K) { *(.bss.boot) }
    
        . = KERNEL_VIRT_BASE + SIZEOF(.text.boot) + SIZEOF(.bss.boot);
        . = ALIGN(4K);
        __kernel_start = .;
    
        .text : AT(ADDR(.text) - KERNEL_DIRECT_BASE)
        {
            __text_start = .;
            *(.text .text.*)
            __text_end = .;
        }
        .rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_DIRECT_BASE)
        {
            __rodata_start = .;
            *(.rodata .rodata.*)
            __rodata_end = .;
        }
        .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_DIRECT_BASE)
        {
            __data_start = .;
            *(.sdata .sdata.*)
            *(.data .data.*)
            __data_end = .;
        }
    
        __global_pointer$ = MIN(__data_start + 0x800,
                                MAX(__data_start + 0x800, __data_end - 0x800));
    
        .bss ALIGN(4K) (NOLOAD) : AT(ADDR(.bss) - KERNEL_DIRECT_BASE)
        {
            __bss_start = .;
            *(.sbss .sbss.*)
            *(.bss .bss.*)
            *(COMMON)
            . = ALIGN(4K);
            __boot_stack_guard = .;
            . += 4K;
            __boot_stack_bottom = .;
            . += 12K;
            __boot_stack_top = .;
            __bss_end = .;
        }
    
        . = ALIGN(4K);
        __kernel_end = .;
        __kernel_phys_end = __kernel_end - KERNEL_DIRECT_BASE;
    
        /DISCARD/ :
        {
            *(.comment)
            *(.eh_frame .eh_frame.*)
            *(.riscv.attributes)
        }
    }
    
    ASSERT((__boot_stack_top & 15) == 0, "boot stack must be 16-byte aligned")
    ASSERT(SIZEOF(.text.boot) <= 4K, "boot trampoline is too large")
    ASSERT(SIZEOF(.bss.boot) == 4K, "bootstrap root must occupy one page")
    ✓ • 0ms

## Assistant

现在 CPU 已经跳到 `.Lhigh_entry`，正在高虚拟地址执行。接下来先准备两个寄存器：**`gp` 用于访问部分全局数据，`sp` 指向启动栈。**

代码是：

```asm
.Lhigh_entry:
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop

    la sp, __boot_stack_top
```

先看 `gp`：

```asm
la gp, __global_pointer$
```

`gp` 是 global pointer，全局指针。编译、链接工具可以把一些全局数据访问优化成“`gp` 加一个小偏移”，这样不必每次都构造完整地址。

因此，启动代码先给 `gp` 设置一个约定好的基准地址。`__global_pointer$` 是链接脚本定义的符号；你可以暂时把它理解成**链接器给某个地址起的名字**，它不是一个需要读取的 C 变量。

它前后的三条 `.option` 也有原因：

```asm
.option push       # 保存汇编选项
.option norelax    # 禁止对这段代码进行链接松弛优化
la gp, __global_pointer$
.option pop        # 恢复选项
```

链接松弛可以把某些地址计算改写成依赖 `gp` 的短指令。但这里正在给 `gp` 赋初值，不能让这次初始化反过来依赖尚未初始化的 `gp`，所以临时禁用这种优化。这些 `.option` 是给工具的指示，不是 CPU 执行的指令。

然后终于来到我们最初讨论的栈：

```asm
la sp, __boot_stack_top
```

**这条指令没有分配内存，只是让 `sp` 指向提前预留好的栈顶。**

那块内存由链接脚本安排：

```ld
__boot_stack_guard = .;
. += 4K;
__boot_stack_bottom = .;
. += 12K;
__boot_stack_top = .;
```

这里的 `.` 是链接脚本中的“当前位置”。`. += 12K` 表示把位置向后推进 12 KiB，给栈留出空间：

```text
高地址
__boot_stack_top     ← 初始 sp，位于栈空间末尾之后
┌──────────────────┐
│                  │
│   12 KiB 启动栈  │
│       ↓ 向下增长 │
│                  │
└──────────────────┘
__boot_stack_bottom
┌──────────────────┐
│  4 KiB 保护区    │
└──────────────────┘
__boot_stack_guard
低地址
```

函数需要栈空间时，会先减小 `sp`，再向那段空间写数据。例如，假设某个函数需要 32 字节：

```asm
addi sp, sp, -32    # 留出 32 字节栈空间
sd ra, 24(sp)      # 在其中保存返回地址
```

函数返回前再恢复：

```asm
ld ra, 24(sp)
addi sp, sp, 32
ret
```

这样就接上了之前讲的：嵌套调用会改写 `ra`，函数可以把原来的返回地址保存到自己的栈空间里。

下面的 4 KiB 保护区用于检测栈溢出。不过，**现在的临时大页映射仍覆盖它**；后面建立正式页表时，才会通过不映射这一区域，让越界访问触发页故障。

到这里，我们已经有了可用栈。接下来还要清零 BSS，才能进入 C 入口。

先确认一个方向问题：如果初始 `sp` 是 `__boot_stack_top`，函数执行了 `addi sp, sp, -32`，新的 `sp` 是进入预留的栈空间，还是离开了栈空间？

## User

进入。

## Assistant

对，`sp` 减小以后，进入了预留的 12 KiB 栈空间。

接下来是进入 C 代码前的另一个准备：**清零 BSS。**

C 语言要求，没有显式初始化的静态存储期变量初始为零。例如项目里的：

```c
static uint64_t bss_test;
```

`bss_test` 应当从 `0` 开始。这样的变量通常放在 `.bss` 段中。镜像不必为它们逐个保存零字节，只记录需要的内存空间；启动代码负责确保这些内存为零。

项目这样做：

```asm
la t0, __bss_start
la t1, __bss_end

.Lclear_bss:
    bgeu t0, t1, .Lbss_done
    sd zero, 0(t0)
    addi t0, t0, 8
    j .Lclear_bss

.Lbss_done:
```

你已经认识大部分指令了：

```text
t0 = BSS 起始地址
t1 = BSS 结束地址，不包含这个位置
```

循环每次先检查：

```asm
bgeu t0, t1, .Lbss_done
```

`bgeu` 表示“按无符号数比较，大于或等于就跳转”。也就是：

```c
if (t0 >= t1)
    goto done;
```

还没到结尾，就写入八字节零，再前进八字节：

```asm
sd zero, 0(t0)
addi t0, t0, 8
```

这里有个值得注意的细节：**刚才预留的启动栈也在这段 BSS 里，因此它也会被清零。**

为什么不会破坏当前执行？因为虽然已经设置了 `sp`，但这个循环还没有使用栈保存任何内容，也没有调用函数。设置 `sp` 只是给寄存器赋值，不等于栈里已经有有效数据。

清零完成后，就要恢复最开始收到的启动参数：

```asm
mv a0, s0
li t0, 0xffffffc000000000
add a1, s1, t0
call kernel_main
```

还记得启动第一步吗？

```asm
mv s0, a0    # 保存 hart ID
mv s1, a1    # 保存 DTB 物理地址
```

现在逐个交还给 C 函数：

- `mv a0, s0`：把 hart ID 放回第一个参数寄存器。
- `add a1, s1, t0`：把 DTB 物理地址加上高地址直接映射的基址，得到供内核使用的虚拟地址。
- `call kernel_main`：保存返回地址并进入 C 函数。

对应的 C 入口是：

```c
_Noreturn void kernel_main(uint64_t hart_id, const void *dtb)
```

`hart_id` 是编号，直接传递即可；`dtb` 是一个需要通过它读取内存的指针，因此这里把它转换成高虚拟地址。DTB 的内容没有被复制。

我们终于把启动链走通了：

```text
保存参数
  → 建立启动页表
  → 开启 Sv39
  → 跳到高地址入口
  → 初始化 gp、sp
  → 清零 BSS
  → 准备参数
  → 调用 kernel_main
```

进入 `kernel_main` 后，项目还会检查 `bss_test` 是否确实为零，这正是在验证刚才的启动准备。

最后确认一个容易混淆的点：**给 DTB 的物理地址加上高地址映射基址，改变的是 DTB 数据存放的位置，还是内核访问它时使用的地址？**

## User

后者。

## Assistant

对。我们现在进入 `kernel_main`，沿着第一条打印语句看看字符怎样到达终端。

## Activity

    $ sed -n '95,155p' src/main.c && sed -n '1,220p' src/platform/qemu-virt.c && sed -n '1,140p' src/platform/platform.h && sed -n '1,100p' src/mm/layout.h
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
    #ifndef MINICORE_PLATFORM_H
    #define MINICORE_PLATFORM_H
    
    #include <stdbool.h>
    #include <stdint.h>
    
    void console_putc(char ch);
    void console_puts(const char *s);
    void console_puthex(uint64_t value);
    _Noreturn void platform_exit(bool success);
    
    #endif
    #ifndef MINICORE_MM_LAYOUT_H
    #define MINICORE_MM_LAYOUT_H
    
    #include <stdint.h>
    
    #define PAGE_SIZE UINT64_C(4096)
    #define PAGE_MASK (PAGE_SIZE - 1)
    #define KERNEL_DIRECT_BASE UINT64_C(0xffffffc000000000)
    
    static inline void *phys_to_virt(uint64_t physical)
    {
        return (void *)(uintptr_t)(KERNEL_DIRECT_BASE + physical);
    }
    
    static inline uint64_t virt_to_phys(const void *virtual_address)
    {
        return (uint64_t)(uintptr_t)virtual_address - KERNEL_DIRECT_BASE;
    }
    
    #endif
    ✓ • 0ms

## Assistant

对，DTB 仍在原来的物理位置，改变的是访问它的地址。启动流程这一段就接通了。

现在进入 `kernel_main`，第一件实际执行的事是打印：

```c
console_puts("\nMiniCore M4\nhart = ");
```

这里没有标准库的 `printf`。项目自己实现了输出，我们沿着这条调用走到底。

先看 `console_puts`：

```c
void console_puts(const char *s)
{
    while (*s)
        console_putc(*s++);
}
```

它逐个读取字符串里的字符，交给 `console_putc`，遇到结尾的 `'\0'` 就停止。

`console_putc(*s++)` 可以拆成：

```c
char ch = *s;       // 读取当前字符
s = s + 1;         // 指针前进一个字符
console_putc(ch);   // 输出刚才读取的字符
```

真正接触硬件的是下面这个函数：

```c
void console_putc(char ch)
{
    if (ch == '\n')
        console_putc('\r');

    while ((UART0[5] & (1 << 5)) == 0) {
    }

    UART0[0] = (uint8_t)ch;
}
```

这里的 `UART0` 是这样定义的：

```c
#define UART0 \
    ((volatile uint8_t *)(KERNEL_DIRECT_BASE + 0x10000000))
```

**UART 是串口设备。这里通过读写特定地址来操作它，这种方式叫内存映射 I/O，简称 MMIO。**

CPU 的读写指令可以访问 RAM，也可以访问设备寄存器，具体由物理地址落在哪里决定。

我们的访问路径是：

```text
UART0 的高虚拟地址：0xffffffc010000000
                 ↓ 启动页表翻译
UART 的物理地址：  0x10000000
                 ↓
             QEMU 模拟的串口设备
```

所以，`UART0[0] = ch` 看起来像给数组元素赋值，实际效果却是**把字符交给串口发送**。QEMU 再把串口输出显示到终端。

现在逐步看这个函数。

**第一步：处理换行。**

```c
if (ch == '\n')
    console_putc('\r');
```

遇到换行 `'\n'`，先输出回车 `'\r'`，再继续输出原来的换行：

```text
'\r'：回到行首
'\n'：换到下一行
```

这里递归一层就结束了，因为传入 `'\r'` 不会再次满足条件。

**第二步：等待设备可以接收字符。**

```c
while ((UART0[5] & (1 << 5)) == 0) {
}
```

`UART0` 是 `uint8_t *`，所以 `[5]` 指的是基址后 **5 字节**处的状态寄存器。

`1 << 5` 是掩码 `0x20`，用来检查状态值的 bit 5：

```text
bit 5 = 0 → 发送保持寄存器还没空，继续等待
bit 5 = 1 → 可以写入下一个字符
```

这是**轮询等待**：CPU 反复读取状态，在等待期间一直执行这个循环。

**第三步：写入字符。**

```c
UART0[0] = (uint8_t)ch;
```

把字符写入发送寄存器，完成这次字符提交。

最后看定义里的 `volatile`。它告诉编译器：这些内存访问本身有意义，不能把反复读取状态寄存器优化成“只读一次、以后复用旧值”。设备状态可能在程序没有写入的情况下自行变化。

它不等于锁，也不等于完整的硬件内存屏障；这里先抓住“必须保留这些设备读写操作”的作用。

这样，第一条打印语句的路径就完整了：

```text
kernel_main
  → console_puts：拆成字符
  → console_putc：等待串口就绪，写入字符
  → QEMU 串口输出
  → 终端显示
```

确认一个细节：**`UART0[5]` 与 `UART0[0]` 都是设备地址，为什么代码要先读前者，再写后者？**

## User

确认串口处于空闲状态可以写入，如果不确认就写会不会丢数据？

## Assistant

对，这一步是在确认发送寄存器能接收下一个字符。我核对一下“可以写入”和“整个串口已经发送完毕”的区别，这两个状态容易混淆。

## Activity

    Searched the web for site.ti.com TL16C550C THRE TEMT holding register empty bit 5 bit 6

## Assistant

对，**不检查设备是否能接收就连续写，可能丢数据**。具体是覆盖、丢弃还是其他行为，取决于设备实现，不能假定每次写入都会被排队保存。

不过，“串口处于空闲状态”还可以说得更精确：这里检查的是**发送保持寄存器为空，可以接收下一个字符**，不要求上一个字符已经完全发送出去。

可以把发送过程理解成：

```text
CPU 写字符
    ↓
发送保持寄存器：暂存待发送字符
    ↓
发送移位寄存器：逐位发送字符
    ↓
串口线路
```

例如发送 `'A'` 和 `'B'`：

1. CPU 把 `'A'` 写入保持寄存器。
2. 硬件把 `'A'` 转入移位寄存器，开始逐位发送。
3. 保持寄存器已经空了，此时 CPU 就可以写入 `'B'`。
4. `'A'` 发完后，硬件接着发送 `'B'`。

因此，这两个状态不同：

| 状态位 | 含义 |
|---|---|
| bit 5：THRE | 发送保持寄存器空，可以提交下一个字符 |
| bit 6：TEMT | 保持寄存器和移位寄存器都空，发送已完成 |

启用 FIFO 时，保持寄存器这一侧换成发送 FIFO，状态含义相应针对 FIFO。[16550 类 UART 数据手册](https://www.ti.com/lit/ds/symlink/tl16c550c.pdf)

回到代码：

```c
while ((UART0[5] & (1 << 5)) == 0) {
}
UART0[0] = (uint8_t)ch;
```

它等的是“可以提交”，写完就返回。**所以 `console_putc()` 返回时，字符已经交给设备，但不保证已经全部发送完毕。**

## User

继续

## Assistant

接着看 `kernel_main` 的下一条调用：

```c
console_puthex(hart_id);
```

串口只能接收一个个字节，不能直接理解“打印一个整数”。所以要先把整数转成字符，再逐个输出。

项目的实现是：

```c
void console_puthex(uint64_t value)
{
    static const char digits[] = "0123456789abcdef";

    console_puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4)
        console_putc(digits[(value >> shift) & 0xf]);
}
```

我们用 `value = 0x2af` 走一遍。

**先输出前缀。**

```c
console_puts("0x");
```

这两个字符只是告诉读者，后面的数采用十六进制表示。

**接下来，每次取出四个二进制位。**

一个十六进制数字恰好对应四个二进制位：

```text
二进制：0010 1010 1111
十六进制： 2    a    f
```

`uint64_t` 有 64 位，所以完整表示需要 `64 / 4 = 16` 个十六进制数字。循环从最高的四位开始：

```text
shift：60、56、52……8、4、0
```

为什么从 `60` 开始？因为最高一组是 bits 63～60，右移 60 位后，它们就到了最低四位。

核心表达式是：

```c
(value >> shift) & 0xf
```

分成两步：

```text
右移：把当前要打印的那组位移到最低位置
与 0xf：只保留最低四位，得到 0～15 的整数
```

对于 `0x2af`，前面的高位都是零，最后三轮是：

| `shift` | `(value >> shift) & 0xf` | 对应字符 |
|---|---|---|
| 8 | `2` | `'2'` |
| 4 | `10` | `'a'` |
| 0 | `15` | `'f'` |

**然后用查表把数值变成字符。**

```c
digits[10]  // 得到字符 'a'
digits[15]  // 得到字符 'f'
```

数值 `10` 和字符 `'a'` 是不同的东西。`digits` 这张小表负责转换，再由 `console_putc` 把字符交给串口。

这个函数没有跳过前导零，所以最后打印的是：

```text
0x00000000000002af
```

对于单核启动时常见的 hart ID `0`，打印结果就是：

```text
0x0000000000000000
```

随后，入口还会打印 DTB 指针：

```c
console_puts("\ndtb  = ");
console_puthex((uintptr_t)dtb);
```

`(uintptr_t)dtb` 把指针转换成能够承载地址的无符号整数，供刚才的函数打印。**这里打印的是地址数值，没有读取 DTB 指向的内容。**

再往后是：

```c
address("kernel = ", __kernel_start);
```

辅助函数 `address` 只是把我们已经理解的输出函数组合起来：

```c
static void address(const char *name, const void *value)
{
    console_puts(name);
    console_puthex((uintptr_t)value);
    console_putc('\n');
}
```

至此，启动时“打印 hart ID、DTB 地址、内核各段地址”的代码就全部串起来了。

下一步是启动自检，它会使用 `memset`、`memcpy`、`memmove`。在进入那里之前，确认一下：**如果把循环的起点从 `shift = 60` 改成 `shift = 0`，并每次加 4，会把十六进制数字按什么顺序打印出来？**

## User

小端序

## Assistant

你抓住了“低位先输出”，但这里更准确的说法是：**十六进制数字会倒序打印，不是小端序。**

小端序描述的是一个多字节数值在**内存里按字节存放的顺序**。刚才的循环每次取四位，处理的是一个十六进制数字，而不是一个字节。

以 `0x1234` 为例：

| 情况 | 结果 |
|---|---|
| 正常打印，从高四位到低四位 | `1234` |
| 修改后的循环，从低四位到高四位 | `4321` |
| 作为 16 位整数按小端存储，按内存地址递增查看 | `34 12` |

注意：小端存储里的一个字节 `0x34`，不会变成 `0x43`。

另外，`value >> shift` 操作的是数值本身，因此原来的 `console_puthex` 并不依赖内存端序。

我们继续到启动自检：

```c
char test[4];

memset(test, 0, sizeof(test));
memcpy(test, "M0", 3);
memmove(test + 1, test, 3);

if (!dtb || bss_test != 0 || strlen(test + 1) != 2)
    panic("self-test failed");
```

先手推这四个字节如何变化，再深入函数实现。

第一步：

```c
memset(test, 0, sizeof(test));
```

把四个字节都填成零：

```text
下标：     0      1      2      3
内容：   '\0'   '\0'   '\0'   '\0'
```

第二步：

```c
memcpy(test, "M0", 3);
```

字符串 `"M0"` 包含两个可见字符和一个结尾零，复制三个字节后：

```text
下标：     0      1      2      3
内容：    'M'    '0'   '\0'   '\0'
```

这里字符 `'0'` 和零字节 `'\0'` 不同：前者是可见数字，后者用于结束字符串。

第三步：

```c
memmove(test + 1, test, 3);
```

把原来的前三个字节向后移动一格：

```text
移动前：  'M'    '0'   '\0'   '\0'
移动后：  'M'    'M'    '0'   '\0'
```

源区间是下标 `0～2`，目标区间是下标 `1～3`，**两者重叠了**。`memmove` 必须保证这种情况下也能正确复制；`memcpy` 不提供这个保证。

于是，`test + 1` 指向字符串 `"M0"`，长度是 `2`。最后的检查同时确认 DTB 指针非空、BSS 测试变量为零，以及这段字符串操作得到了预期结果。

想一下最关键的重叠问题：如果 `memmove` 在这里从前往后复制，先执行 `test[1] = test[0]`，再执行 `test[2] = test[1]`，第二次复制拿到的会是原来的 `'0'`，还是刚刚写进去的 `'M'`？

## User

后者

## Assistant

对，前面写入的 `'M'` 覆盖了还没来得及读取的 `'0'`。我们看项目里的 `memmove` 如何选择复制方向。

## Activity

    $ sed -n '1,240p' src/runtime.c && sed -n '1,160p' src/runtime.h
    #include "runtime.h"
    
    void *memcpy(void *restrict dst, const void *restrict src, size_t n)
    {
        unsigned char *d = dst;
        const unsigned char *s = src;
    
        while (n--)
            *d++ = *s++;
        return dst;
    }
    
    int memcmp(const void *left, const void *right, size_t n)
    {
        const unsigned char *a = left;
        const unsigned char *b = right;
    
        while (n--) {
            if (*a != *b)
                return *a < *b ? -1 : 1;
            ++a;
            ++b;
        }
        return 0;
    }
    
    void *memmove(void *dst, const void *src, size_t n)
    {
        unsigned char *d = dst;
        const unsigned char *s = src;
    
        if (d < s) {
            while (n--)
                *d++ = *s++;
        } else if (d > s) {
            d += n;
            s += n;
            while (n--)
                *--d = *--s;
        }
        return dst;
    }
    
    void *memset(void *dst, int value, size_t n)
    {
        unsigned char *d = dst;
    
        while (n--)
            *d++ = (unsigned char)value;
        return dst;
    }
    
    size_t strlen(const char *s)
    {
        size_t n = 0;
    
        while (s[n])
            ++n;
        return n;
    }
    #ifndef MINICORE_RUNTIME_H
    #define MINICORE_RUNTIME_H
    
    #include <stddef.h>
    
    void *memcpy(void *restrict dst, const void *restrict src, size_t n);
    int memcmp(const void *left, const void *right, size_t n);
    void *memmove(void *dst, const void *src, size_t n);
    void *memset(void *dst, int value, size_t n);
    size_t strlen(const char *s);
    
    #endif
    ✓ • 0ms

## Assistant

对，第二次会读到刚写进去的 `'M'`。要避免覆盖尚未读取的数据，这次必须**从后往前复制**。

项目中的 `memmove` 是这样实现的：

```c
void *memmove(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;

    if (d < s) {
        while (n--)
            *d++ = *s++;
    } else if (d > s) {
        d += n;
        s += n;
        while (n--)
            *--d = *--s;
    }
    return dst;
}
```

我们继续代入：

```c
memmove(test + 1, test, 3);
```

此时：

```text
d 指向 test[1]
s 指向 test[0]
n = 3
```

所以走 `d > s` 分支。

先把两个指针移动到各自区间的末尾之后：

```c
d += n;   // 指向 test[4]
s += n;   // 指向 test[3]
```

`test[4]` 在数组末尾之后。可以形成这个指针，但不能解引用它。接下来用的是**先减一，再读取或写入**：

```c
*--d = *--s;
```

三次循环依次完成：

```text
第 1 次：test[3] = test[2]    复制 '\0'
第 2 次：test[2] = test[1]    复制 '0'
第 3 次：test[1] = test[0]    复制 'M'
```

每次覆盖一个位置前，它原来的内容都已经被读取，因此结果正确：

```text
'M'  'M'  '0'  '\0'
```

反过来，如果目标在源前面，也就是 `d < s`，就从前往后复制：

```c
*d++ = *s++;
```

如果 `d == s`，源和目标完全相同，什么都不用做。这也解释了代码为什么没有最后的 `else`。

再补齐这个函数的几个细节：

- 转成 `unsigned char *`，是为了逐字节访问，每次指针加减一都移动一个字节。
- `const unsigned char *s` 表示通过 `s` 读取，不通过它修改源数据。
- `n` 是字节数，复制不会因为遇到 `'\0'` 而提前停止。
- 返回原始的 `dst`，而不是循环中移动过的 `d`。
- 它没有分配临时缓冲区，靠复制方向处理重叠。

`while (n--)` 则是先用旧的 `n` 判断，再递减。`n=3` 时循环体执行三次；最后旧值为零，不再进入循环。由于 `size_t` 是无符号类型，最后这次递减会让局部变量 `n` 回绕到最大值，不过之后没有再使用它，不影响结果。

对于眼前这个同一数组内的移动，我们已经能完整解释它为什么正确。

现在反向试一次：数组是 `{'A', 'B', 'C', '\0'}`，执行 `memmove(test, test + 1, 3)`，应该选从前往后还是从后往前？

## User

从后往前

## Assistant

这次应该**从前往后**。我们用实际数据走一遍就清楚了。

```c
memmove(test, test + 1, 3);
```

意思是把下标 `1、2、3` 的内容复制到 `0、1、2`：

```text
下标：  0    1    2     3
原始： 'A'  'B'  'C'  '\0'
```

从前往后复制：

```text
test[0] = test[1] → 'B'  'B'  'C'  '\0'
test[1] = test[2] → 'B'  'C'  'C'  '\0'
test[2] = test[3] → 'B'  'C' '\0'  '\0'
```

每次都能读到原始数据，最后得到字符串 `"BC"`。

如果从后往前：

```text
test[2] = test[3] → 'A'  'B' '\0'  '\0'
test[1] = test[2] → 'A' '\0' '\0'  '\0'
```

第一步就覆盖了还没读取的 `'C'`，第二步读到的已经是零了。

你可以先记住这个判断原则：

> **复制时，写入的位置不能破坏后面还需要读取的源数据。**

所以，对重叠区域：

```text
向低地址移动：从前往后复制
向高地址移动：从后往前复制
```

这也正好对应代码里的 `d < s` 和 `d > s`。两种情况都遵循同一个原则，只是安全的方向相反。

## User

继续

## Assistant

我们把这段启动自检里剩下的函数串完：

```c
memset(test, 0, sizeof(test));
memcpy(test, "M0", 3);
memmove(test + 1, test, 3);

if (!dtb || bss_test != 0 || strlen(test + 1) != 2)
    panic("self-test failed");
```

`memmove` 已经理解了，现在看它前后的三个函数。

先是 `memset`：

```c
void *memset(void *dst, int value, size_t n)
{
    unsigned char *d = dst;

    while (n--)
        *d++ = (unsigned char)value;
    return dst;
}
```

它把目标区域的 **每一个字节** 都设置成同一个值。这里 `sizeof(test)` 是 `4`，所以：

```c
memset(test, 0, sizeof(test));
```

就是写入四个零字节。

参数 `value` 虽然是 `int`，实际写入前会转换成 `unsigned char`。因此，`memset` 按字节填充，不是给每个整数元素赋值。例如，在本项目中：

```c
uint32_t x;
memset(&x, 1, sizeof(x));
```

会让 `x` 变成 `0x01010101`，而不是 `1`。

接着是 `memcpy`：

```c
void *memcpy(void *restrict dst, const void *restrict src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;

    while (n--)
        *d++ = *s++;
    return dst;
}
```

它固定从前往后逐字节复制，没有判断方向。这里：

```c
memcpy(test, "M0", 3);
```

源字符串与目标数组不重叠，可以安全使用。

参数里的 `restrict` 是向编译器作出的访问约定，不是运行时检查。对这里的调用，必须遵守 `memcpy` 的要求：**复制的源、目标区域不能重叠**。有重叠就用 `memmove`。

再看 `strlen`：

```c
size_t strlen(const char *s)
{
    size_t n = 0;

    while (s[n])
        ++n;
    return n;
}
```

它从 `s[0]` 开始寻找第一个零字节，返回它前面有多少个字符，**不把结尾的 `'\0'` 算进去**。

经过前面的移动：

```text
test：     'M'  'M'  '0'  '\0'
                 ↑
              test + 1
```

所以：

```text
s[0] = 'M'  → n 变成 1
s[1] = '0'  → n 变成 2
s[2] = '\0' → 停止，返回 2
```

这里还有一个重要的接口区别：

| 函数 | 怎么知道何时停止 |
|---|---|
| `memset`、`memcpy`、`memmove` | 由调用者传入字节数 `n` |
| `strlen` | 一直寻找 `'\0'` |

因此，`strlen` 不知道数组有多大；调用者必须提供可访问、且有零字节结尾的字符串。

最后看判断：

```c
if (!dtb || bss_test != 0 || strlen(test + 1) != 2)
    panic("self-test failed");
```

三个条件中任意一个成立，就报告失败：

- `dtb` 是空指针。
- BSS 中的测试变量不是零。
- 字符串操作后的长度不是预期的 `2`。

`||` 从左向右短路求值，前面已经为真，就不再计算后面的条件。这段检查是一次启动冒烟测试，并没有覆盖这些函数的所有输入情况。

到这里，我们已经讲完 `memset`、`memcpy`、`memmove` 和 `strlen` 的实际实现。下一站是失败路径 `panic`，然后就进入 DTB 硬件发现。

先确认一个很实用的区别：**对内存 `{'A', '\0', 'B', '\0'}`，`memcpy(dst, src, 4)` 会复制几个字节，而 `strlen(src)` 会返回多少？**

## User

4,2

## Assistant

第一个对，复制 **4 字节**；第二个应该是 **1**。

```text
下标：  0      1      2      3
内容： 'A'   '\0'   'B'   '\0'
```

`strlen` 的执行过程是：

```text
s[0] 是 'A'  → n 从 0 变成 1
s[1] 是 '\0' → 停止，返回 1
```

它不统计整块内存中有几个非零字符，只统计**第一个 `'\0'` 之前**的字符数。后面的 `'B'` 根本不会读到。

而 `memcpy` 不理解字符串结束标记，只按指定长度搬运，因此四个字节，包括两个 `'\0'`，都会复制。

## User

继续

## Assistant

接下来把启动失败时的处理讲完，再进入 DTB：内核如何从一块二进制数据中读出机器配置。

## Activity

    $ sed -n '20,60p' src/main.c && sed -n '1,130p' src/platform/dtb.h && sed -n '1,65p' src/platform/qemu-virt.c
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
    #ifndef MINICORE_DTB_H
    #define MINICORE_DTB_H
    
    #include <stdbool.h>
    #include <stddef.h>
    #include <stdint.h>
    
    #define BOOT_MAX_MEMORY_RANGES 8
    #define BOOT_MAX_RESERVED_RANGES 16
    #define BOOT_MAX_VIRTIO_DEVICES 16
    
    struct physical_range {
        uint64_t base;
        uint64_t size;
    };
    
    struct boot_info {
        struct physical_range memory[BOOT_MAX_MEMORY_RANGES];
        size_t memory_count;
    
        struct physical_range reserved[BOOT_MAX_RESERVED_RANGES];
        size_t reserved_count;
    
        struct physical_range uart;
        uint32_t uart_irq;
        struct physical_range plic;
        uint32_t plic_source_count;
        struct physical_range virtio[BOOT_MAX_VIRTIO_DEVICES];
        uint32_t virtio_irqs[BOOT_MAX_VIRTIO_DEVICES];
        size_t virtio_count;
    
        uint64_t timebase_frequency;
        uintptr_t dtb_base;
        size_t dtb_size;
        bool uart_present;
        bool plic_present;
    };
    
    enum dtb_error {
        DTB_OK,
        DTB_ERR_ARGUMENT,
        DTB_ERR_MAGIC,
        DTB_ERR_VERSION,
        DTB_ERR_BOUNDS,
        DTB_ERR_STRUCTURE,
        DTB_ERR_PROPERTY,
        DTB_ERR_UNSUPPORTED,
        DTB_ERR_CAPACITY,
    };
    
    enum dtb_error dtb_parse(const void *blob, struct boot_info *info);
    const char *dtb_error_string(enum dtb_error error);
    
    #endif
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
    ✓ • 0ms

## Assistant

接下来把失败路径走完。自检不通过时会调用：

```c
panic("self-test failed");
```

项目里的 `panic` 很短：

```c
static _Noreturn void panic(const char *message)
{
    console_puts("PANIC: ");
    console_puts(message);
    console_putc('\n');
    platform_exit(false);
}
```

前三行会输出：

```text
PANIC: self-test failed
```

然后 `platform_exit(false)` 通知 QEMU：这次运行失败，结束模拟。

这里有两个声明细节：

- `static`：这个函数名只在当前 C 文件中可见。
- `_Noreturn`：告诉编译器，这个函数不会正常返回调用者。它本身不负责停机，实际行为由函数体实现。

继续进入 `platform_exit`：

```c
#define QEMU_TEST \
    ((volatile uint32_t *)(KERNEL_DIRECT_BASE + 0x00100000))

_Noreturn void platform_exit(bool success)
{
    *QEMU_TEST = success ? 0x5555 : 0x3333;

    for (;;)
        __asm__ volatile("wfi");
}
```

这又是我们刚学过的 MMIO：向特定设备地址写入数值，触发设备行为。

不过这次操作的是 QEMU 的测试退出设备：

```text
success 为 true  → 写入 0x5555，报告成功
success 为 false → 写入 0x3333，报告失败
```

注意它是 `volatile uint32_t *`，因此这里写入的是 **32 位，即四字节**。之前 UART 的指针是 `uint8_t *`，每次写入一个字节。设备访问的宽度也是接口的一部分，不能随意互换。

正常情况下，写入后 QEMU 就会退出。后面的无限循环是兜底：

```c
for (;;)
    __asm__ volatile("wfi");
```

`__asm__ volatile` 在 C 中嵌入汇编；`wfi` 是等待中断的指令。这里没有返回路径，即使执行继续，也会留在循环中。

这也解释了启动汇编中尚未讲过的一句：

```asm
call kernel_main
call panic_returned_from_kernel_main
```

`kernel_main` 按设计不应返回。假如它意外返回，就执行：

```c
_Noreturn void panic_returned_from_kernel_main(void)
{
    panic("kernel_main returned");
}
```

这是对错误返回的兜底处理。

现在回到成功路径。启动自检通过后，下一步是：

```c
error = dtb_parse(dtb, &boot_info);
```

它的输入输出关系是：

```text
dtb：指向原始硬件描述数据
             ↓ dtb_parse
boot_info：整理好的 C 结构体，供内核使用
```

例如 `boot_info` 中用这个结构记录一段物理区域：

```c
struct physical_range {
    uint64_t base;
    uint64_t size;
};
```

`base` 是起始物理地址，`size` 是字节数。内存范围、保留范围、设备寄存器范围，都可以用它描述。

我们接下来会先弄懂 DTB 的数据组织，再读解析函数。先留一个和前面串口代码有关的问题：**既然 DTB 可以告诉我们 UART 在哪里，为什么这个项目还能在解析 DTB 之前就打印文字？**

## User

qemu给了S mode 直接操作UART0的权限

## Assistant

你说到了“能不能访问”的前提，但这里还缺一个关键：**解析 DTB 之前，内核怎么知道 UART 在哪里？**

这个项目把 QEMU `virt` 的 UART 物理地址直接写在代码里了：

```c
#define UART0 \
    ((volatile uint8_t *)(KERNEL_DIRECT_BASE + 0x10000000))
```

因此不需要先解析 DTB，就已经知道要访问物理地址 `0x10000000`。

把几个条件分开看：

| 条件 | 这个项目如何满足 |
|---|---|
| 知道 UART 地址 | 代码写死 `0x10000000` |
| 能通过虚拟地址访问它 | 启动页表已经映射对应区域 |
| S-mode 访问被允许 | 启动环境的物理访问保护允许这次访问 |

你提到的是第三点。不过，准确地说，不能笼统理解成“QEMU 直接授予 S-mode 权限”：QEMU 模拟硬件，OpenSBI 在 M-mode 下运行；S-mode 的物理访问还受适用的 PMP 等硬件保护规则约束。

**所以提前打印的关键，是内核已经使用了已知的平台地址，并建立了可用映射。**

这是一种启动阶段的取舍：先依赖固定平台信息提供输出，后面才能在解析 DTB 失败时打印错误。DTB 解析得到的 UART 信息，在当前实现里也不会修改这个写死的 `UART0` 宏。

接下来理解 DTB 的作用就容易了：UART 地址可以暂时写死，但 RAM 大小可能随 QEMU 启动参数变化。内核需要读取机器实际提供的配置，才能知道哪些内存可以使用。

DTB 是 **Device Tree Blob，设备树的二进制形式**。你可以先把它理解成这样的层级信息：

```text
根节点
├─ 内存节点：RAM 起始地址、大小
├─ CPU 相关节点：时钟信息等
└─ 设备相关节点
   ├─ UART：寄存器地址、中断号
   ├─ PLIC：中断控制器信息
   └─ VirtIO：设备地址、中断号
```

这只是帮助理解的示意，不是原始 DTB 的字节布局。

`dtb_parse` 的任务就是读取这棵树，把内核关心的属性提取到 `boot_info`。下一步我们会从它的入口检查开始：**收到一个指针，并不等于可以直接相信指针后面的数据就是合法 DTB。**

## User

继续

## Assistant

我们先看 `dtb_parse` 如何检查头部和各段边界，再进入树的解析。这样每个检查都能对应到它保护的那一次内存读取。

## Activity

    $ sed -n '1,220p' src/platform/dtb.c && rg -n 'dtb_parse|read_be|bounds|header' src/platform/dtb.c
    #include "dtb.h"
    
    #include "../runtime.h"
    
    #define FDT_MAGIC 0xd00dfeedU
    #define FDT_BEGIN_NODE 1U
    #define FDT_END_NODE 2U
    #define FDT_PROP 3U
    #define FDT_NOP 4U
    #define FDT_END 9U
    
    #define FDT_HEADER_SIZE 40U
    #define FDT_SUPPORTED_VERSION 17U
    #define FDT_MAX_SIZE (16U * 1024U * 1024U)
    #define FDT_MAX_DEPTH 32
    
    struct fdt_view {
        const uint8_t *blob;
        size_t total_size;
        const uint8_t *structure;
        size_t structure_size;
        const uint8_t *strings;
        size_t strings_size;
        size_t reservation_offset;
    };
    
    struct node {
        const char *name;
        size_t name_length;
        uint32_t address_cells;
        uint32_t size_cells;
        const uint8_t *ranges;
        size_t ranges_length;
        const uint8_t *reg;
        size_t reg_length;
        const uint8_t *compatible;
        size_t compatible_length;
        const uint8_t *device_type;
        size_t device_type_length;
        const uint8_t *status;
        size_t status_length;
        const uint8_t *interrupts;
        size_t interrupts_length;
        const uint8_t *plic_source_count;
        size_t plic_source_count_length;
        bool ranges_present;
        bool reserved_memory;
        bool cpus;
    };
    
    static uint32_t read_be32(const void *pointer)
    {
        const uint8_t *p = pointer;
    
        return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
               ((uint32_t)p[2] << 8) | p[3];
    }
    
    static uint64_t read_be64(const void *pointer)
    {
        const uint8_t *p = pointer;
    
        return ((uint64_t)read_be32(p) << 32) | read_be32(p + 4);
    }
    
    static bool range_within(size_t total, size_t offset, size_t length)
    {
        return offset <= total && length <= total - offset;
    }
    
    static bool add_overflows_u64(uint64_t left, uint64_t right)
    {
        return left > UINT64_MAX - right;
    }
    
    static bool regions_overlap(size_t first_offset, size_t first_size,
                                size_t second_offset, size_t second_size)
    {
        return first_offset < second_offset + second_size &&
               second_offset < first_offset + first_size;
    }
    
    static bool bytes_equal_string(const uint8_t *bytes, size_t length,
                                   const char *string)
    {
        size_t string_length = strlen(string);
    
        return length == string_length + 1 && bytes[string_length] == '\0' &&
               memcmp(bytes, string, string_length) == 0;
    }
    
    static bool name_equal(const struct node *node, const char *name)
    {
        size_t length = strlen(name);
    
        return node->name_length == length &&
               memcmp(node->name, name, length) == 0;
    }
    
    static bool compatible_has(const struct node *node, const char *wanted)
    {
        size_t wanted_length = strlen(wanted);
        size_t offset = 0;
    
        while (offset < node->compatible_length) {
            size_t length = 0;
    
            while (offset + length < node->compatible_length &&
                   node->compatible[offset + length] != '\0')
                ++length;
            if (offset + length == node->compatible_length)
                return false;
            if (length == wanted_length &&
                memcmp(node->compatible + offset, wanted, wanted_length) == 0)
                return true;
            offset += length + 1;
        }
        return false;
    }
    
    static bool node_enabled(const struct node *node)
    {
        if (!node->status)
            return true;
        return bytes_equal_string(node->status, node->status_length, "okay") ||
               bytes_equal_string(node->status, node->status_length, "ok");
    }
    
    static enum dtb_error decode_cells(const uint8_t *data, uint32_t cells,
                                       uint64_t *value)
    {
        if (cells > 2)
            return DTB_ERR_UNSUPPORTED;
    
        *value = 0;
        for (uint32_t i = 0; i < cells; ++i)
            *value = (*value << 32) | read_be32(data + i * 4U);
        return DTB_OK;
    }
    
    static enum dtb_error translate_address(const struct node *nodes, int depth,
                                            uint64_t size, uint64_t *address)
    {
        for (int bus_depth = depth - 1; bus_depth > 0; --bus_depth) {
            const struct node *bus = &nodes[bus_depth];
            const struct node *parent = &nodes[bus_depth - 1];
            size_t entry_cells;
            size_t entry_size;
            bool matched = false;
    
            if (!bus->ranges_present)
                return DTB_ERR_UNSUPPORTED;
            if (bus->ranges_length == 0)
                continue;
            if (bus->address_cells > 2 || parent->address_cells > 2 ||
                bus->size_cells > 2)
                return DTB_ERR_UNSUPPORTED;
    
            entry_cells = (size_t)bus->address_cells + parent->address_cells +
                          bus->size_cells;
            entry_size = entry_cells * 4U;
            if (entry_size == 0 || bus->ranges_length % entry_size != 0)
                return DTB_ERR_PROPERTY;
    
            for (size_t offset = 0; offset < bus->ranges_length;
                 offset += entry_size) {
                const uint8_t *entry = bus->ranges + offset;
                uint64_t child_address;
                uint64_t parent_address;
                uint64_t range_size;
                enum dtb_error error;
    
                error = decode_cells(entry, bus->address_cells, &child_address);
                if (error != DTB_OK)
                    return error;
                entry += bus->address_cells * 4U;
                error = decode_cells(entry, parent->address_cells,
                                     &parent_address);
                if (error != DTB_OK)
                    return error;
                entry += parent->address_cells * 4U;
                error = decode_cells(entry, bus->size_cells, &range_size);
                if (error != DTB_OK)
                    return error;
    
                if (*address >= child_address && size <= range_size &&
                    *address - child_address <= range_size - size) {
                    uint64_t displacement = *address - child_address;
    
                    if (add_overflows_u64(parent_address, displacement))
                        return DTB_ERR_PROPERTY;
                    *address = parent_address + displacement;
                    matched = true;
                    break;
                }
            }
            if (!matched)
                return DTB_ERR_PROPERTY;
        }
        return DTB_OK;
    }
    
    static enum dtb_error add_range(struct physical_range *ranges, size_t *count,
                                    size_t capacity, uint64_t base, uint64_t size)
    {
        if (size == 0)
            return DTB_OK;
        if (add_overflows_u64(base, size))
            return DTB_ERR_PROPERTY;
        if (*count == capacity)
            return DTB_ERR_CAPACITY;
        ranges[*count].base = base;
        ranges[*count].size = size;
        ++*count;
        return DTB_OK;
    }
    
    static enum dtb_error parse_reg(const struct node *nodes, int depth,
                                    struct physical_range *ranges, size_t *count,
                                    size_t capacity)
    51:static uint32_t read_be32(const void *pointer)
    59:static uint64_t read_be64(const void *pointer)
    63:    return ((uint64_t)read_be32(p) << 32) | read_be32(p + 4);
    137:        *value = (*value << 32) | read_be32(data + i * 4U);
    311:            info->uart_irq = read_be32(node->interrupts);
    326:            info->plic_source_count = read_be32(node->plic_source_count);
    342:                read_be32(node->interrupts);
    350:static enum dtb_error validate_header(const void *blob, struct fdt_view *view)
    364:    if (read_be32(bytes) != FDT_MAGIC)
    367:    total_size = read_be32(bytes + 4);
    368:    structure_offset = read_be32(bytes + 8);
    369:    strings_offset = read_be32(bytes + 12);
    370:    reservation_offset = read_be32(bytes + 16);
    371:    version = read_be32(bytes + 20);
    372:    last_compatible_version = read_be32(bytes + 24);
    373:    strings_size = read_be32(bytes + 32);
    374:    structure_size = read_be32(bytes + 36);
    420:        address = read_be64(view->blob + offset);
    421:        size = read_be64(view->blob + offset + 8);
    462:        node->address_cells = read_be32(value);
    466:        node->size_cells = read_be32(value);
    492:        info->timebase_frequency = read_be32(value);
    506:        uint32_t token = read_be32(view->structure + offset);
    554:            length = read_be32(view->structure + offset);
    555:            name_offset = read_be32(view->structure + offset + 4);
    579:enum dtb_error dtb_parse(const void *blob, struct boot_info *info)
    587:    error = validate_header(blob, &view);
    611:        return "out of bounds";
    ✓ • 0ms

## Activity

    $ sed -n '350,405p' src/platform/dtb.c && sed -n '579,630p' src/platform/dtb.c
    static enum dtb_error validate_header(const void *blob, struct fdt_view *view)
    {
        const uint8_t *bytes = blob;
        uint32_t total_size;
        uint32_t structure_offset;
        uint32_t strings_offset;
        uint32_t reservation_offset;
        uint32_t version;
        uint32_t last_compatible_version;
        uint32_t strings_size;
        uint32_t structure_size;
    
        if (!blob)
            return DTB_ERR_ARGUMENT;
        if (read_be32(bytes) != FDT_MAGIC)
            return DTB_ERR_MAGIC;
    
        total_size = read_be32(bytes + 4);
        structure_offset = read_be32(bytes + 8);
        strings_offset = read_be32(bytes + 12);
        reservation_offset = read_be32(bytes + 16);
        version = read_be32(bytes + 20);
        last_compatible_version = read_be32(bytes + 24);
        strings_size = read_be32(bytes + 32);
        structure_size = read_be32(bytes + 36);
    
        if (version < FDT_SUPPORTED_VERSION ||
            last_compatible_version > FDT_SUPPORTED_VERSION)
            return DTB_ERR_VERSION;
        if (total_size < FDT_HEADER_SIZE || total_size > FDT_MAX_SIZE)
            return DTB_ERR_BOUNDS;
        if (structure_offset < FDT_HEADER_SIZE ||
            strings_offset < FDT_HEADER_SIZE ||
            reservation_offset < FDT_HEADER_SIZE ||
            (structure_offset & 3U) != 0 || (reservation_offset & 7U) != 0 ||
            !range_within(total_size, structure_offset, structure_size) ||
            !range_within(total_size, strings_offset, strings_size) ||
            !range_within(total_size, reservation_offset, 16) ||
            regions_overlap(structure_offset, structure_size, strings_offset,
                            strings_size))
            return DTB_ERR_BOUNDS;
    
        view->blob = bytes;
        view->total_size = total_size;
        view->structure = bytes + structure_offset;
        view->structure_size = structure_size;
        view->strings = bytes + strings_offset;
        view->strings_size = strings_size;
        view->reservation_offset = reservation_offset;
        return DTB_OK;
    }
    
    static enum dtb_error parse_reservations(const struct fdt_view *view,
                                             struct boot_info *info)
    {
        size_t offset = view->reservation_offset;
    enum dtb_error dtb_parse(const void *blob, struct boot_info *info)
    {
        struct fdt_view view;
        enum dtb_error error;
    
        if (!info)
            return DTB_ERR_ARGUMENT;
        memset(info, 0, sizeof(*info));
        error = validate_header(blob, &view);
        if (error != DTB_OK)
            return error;
    
        info->dtb_base = (uintptr_t)blob;
        info->dtb_size = view.total_size;
        error = parse_reservations(&view, info);
        if (error != DTB_OK)
            return error;
        return parse_structure(&view, info);
    }
    
    const char *dtb_error_string(enum dtb_error error)
    {
        switch (error) {
        case DTB_OK:
            return "ok";
        case DTB_ERR_ARGUMENT:
            return "invalid argument";
        case DTB_ERR_MAGIC:
            return "bad magic";
        case DTB_ERR_VERSION:
            return "unsupported version";
        case DTB_ERR_BOUNDS:
            return "out of bounds";
        case DTB_ERR_STRUCTURE:
            return "malformed structure";
        case DTB_ERR_PROPERTY:
            return "malformed property";
        case DTB_ERR_UNSUPPORTED:
            return "unsupported encoding";
        case DTB_ERR_CAPACITY:
            return "boot info capacity exceeded";
        }
        return "unknown error";
    }
    ✓ • 0ms

## Assistant

我们进入这条调用：

```c
error = dtb_parse(dtb, &boot_info);
```

先看实际入口。你已经认识里面的 `memset` 了：

```c
enum dtb_error dtb_parse(const void *blob, struct boot_info *info)
{
    struct fdt_view view;
    enum dtb_error error;

    if (!info)
        return DTB_ERR_ARGUMENT;

    memset(info, 0, sizeof(*info));

    error = validate_header(blob, &view);
    if (error != DTB_OK)
        return error;

    info->dtb_base = (uintptr_t)blob;
    info->dtb_size = view.total_size;

    error = parse_reservations(&view, info);
    if (error != DTB_OK)
        return error;

    return parse_structure(&view, info);
}
```

它把工作分为三步：

```text
validate_header     检查头部，定位各个数据区域
        ↓
parse_reservations  读取不能随意分配的物理内存范围
        ↓
parse_structure     解析节点和属性，提取硬件信息
```

`info` 是最终输出；`view` 是解析过程使用的“导航信息”，保存各区域的地址和大小，避免后续函数反复解读头部。

开头先检查输出指针，再清空输出结构：

```c
if (!info)
    return DTB_ERR_ARGUMENT;

memset(info, 0, sizeof(*info));
```

注意 `sizeof(*info)` 是整个 `boot_info` 结构的大小。如果写成 `sizeof(info)`，得到的只是指针大小，就清不完整了。

接下来进入 `validate_header`。最初两项检查是：

```c
if (!blob)
    return DTB_ERR_ARGUMENT;

if (read_be32(bytes) != FDT_MAGIC)
    return DTB_ERR_MAGIC;
```

`FDT_MAGIC` 在代码中定义为：

```c
#define FDT_MAGIC 0xd00dfeedU
```

这个固定值叫**魔数**，用于初步识别数据格式。就像文件开头的格式标记：不匹配，就不能按 DTB 继续解析；匹配也不代表后面所有内容都合法。

这里出现一个重要函数：

```c
static uint32_t read_be32(const void *pointer)
{
    const uint8_t *p = pointer;

    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |
           p[3];
}
```

`be` 表示 big-endian，大端序。它把四个按大端顺序存储的字节，组合成一个 32 位数值。

这正好接上我们前面讲过的端序。DTB 开头的四个字节按地址递增排列为：

```text
偏移：    0     1     2     3
字节：   d0    0d    fe    ed
```

函数逐个读取，再放到正确位置：

```text
p[0] << 24 → 0xd0000000
p[1] << 16 → 0x000d0000
p[2] <<  8 → 0x0000fe00
p[3]       → 0x000000ed
按位或合并 → 0xd00dfeed
```

为什么先转换成 `uint32_t`？

```c
(uint32_t)p[0] << 24
```

这样移位在无符号 32 位类型上进行，避免字节先提升为有符号 `int` 后，左移产生不可表示的结果。

为什么逐字节读取？这样不依赖 CPU 的内存端序，也不要求传入地址按四字节对齐。不过，调用者仍须保证这四个字节可读；这个函数本身不检查边界。

这里也要明确项目的前提：**非空检查不证明地址有效。** 这套入口依赖启动环境交付可访问的 DTB；头部验证主要检查数据格式与内部范围，并不能把任意坏指针变安全。

下一步会用同一个 `read_be32` 读取总长度、各区域偏移，再检查边界。

先用一个小例子确认端序：如果四个字节依次是 `00 00 01 00`，经过 `read_be32` 后，得到的是十进制 `1` 还是 `256`？

## User

不是4？

## Assistant

这里得到的是 **256**。**4 是读取的字节数，不是这四个字节表示的数值。**

`00 00 01 00` 表示四个字节，每个字节用两个十六进制数字书写：

```text
p[0] = 0x00
p[1] = 0x00
p[2] = 0x01
p[3] = 0x00
```

代入代码：

```text
(0 << 24) | (0 << 16) | (1 << 8) | 0
= 1 << 8
= 256
```

你也可以把大端序看成“以 256 为进制的四位数”，因为一个字节有 256 种取值：

```text
数值 = p[0] × 256³
     + p[1] × 256²
     + p[2] × 256
     + p[3]
```

所以：

```text
00 00 00 01 → 1
00 00 00 04 → 4
00 00 01 00 → 256
```

就像十进制的 `0100` 表示一百，而不是因为写了四个数字就表示四。

## User

继续

## Assistant

现在我们知道 `read_be32` 如何读出数值，继续看 `validate_header` 如何使用它。

DTB 的头部相当于一份目录，记录“总共有多大、各块数据在哪里”。项目读取这些字段：

```c
total_size         = read_be32(bytes + 4);
structure_offset   = read_be32(bytes + 8);
strings_offset     = read_be32(bytes + 12);
reservation_offset = read_be32(bytes + 16);
version            = read_be32(bytes + 20);
last_compatible_version = read_be32(bytes + 24);
strings_size       = read_be32(bytes + 32);
structure_size     = read_be32(bytes + 36);
```

`bytes` 是字节指针，所以 `bytes + 8` 就是从 DTB 开头向后移动八字节。

这里最重要的是区分**偏移**与**地址**：

```text
structure_offset：结构块距离 DTB 开头有多少字节
bytes + structure_offset：结构块实际所在的地址
```

三个数据区域的用途如下：

| 区域 | 存放什么 |
|---|---|
| 结构块 `structure` | 节点、属性，以及树的层级关系 |
| 字符串块 `strings` | 属性名称字符串，供结构块引用 |
| 保留内存表 `reservation` | 启动环境声明的保留物理内存范围 |

先不展开区域内部格式，我们目前只检查“目录是否合理”。

代码先检查版本：

```c
if (version < FDT_SUPPORTED_VERSION ||
    last_compatible_version > FDT_SUPPORTED_VERSION)
    return DTB_ERR_VERSION;
```

本项目的 `FDT_SUPPORTED_VERSION` 是 `17`。它接受版本不低于 17、并且仍声明兼容版本 17 的数据。

然后检查总长度：

```c
if (total_size < FDT_HEADER_SIZE || total_size > FDT_MAX_SIZE)
    return DTB_ERR_BOUNDS;
```

本项目要求总大小至少容得下 40 字节头部，并且不超过自己规定的 16 MiB 上限。

接下来要确认各区域没有越过 DTB 的结尾。这里使用了一个很值得学的辅助函数：

```c
static bool range_within(size_t total, size_t offset, size_t length)
{
    return offset <= total && length <= total - offset;
}
```

它检查：

> 从 `offset` 开始、长度为 `length` 的区域，是否完全位于总长度 `total` 以内？

例如：

```text
total  = 100 字节
offset = 80
length = 20
```

先确认 `80 <= 100`，再确认 `20 <= 100 - 80`，结果为真。最后使用的是偏移 `99` 的字节，刚好没有越界。

如果 `length = 21`，就会触及偏移 `100`，返回假。

你可能会想到更直接的写法：

```c
offset + length <= total
```

项目没有这样写，是为了避免**无符号加法溢出**。两个很大的数相加可能回绕成小数，导致一个越界区域被误判为合法。

当前写法先检查：

```c
offset <= total
```

由于 `&&` 短路求值，只有它成立时才计算 `total - offset`，因此减法也不会下溢。

其他检查还包括：

- 各区域起点不能落在头部内部。
- 结构块偏移按四字节对齐。
- 保留内存表偏移按八字节对齐，并至少容得下一条 16 字节记录。
- 结构块与字符串块不能重叠。

检查通过后，才把地址写入 `view`：

```c
view->structure = bytes + structure_offset;
view->structure_size = structure_size;

view->strings = bytes + strings_offset;
view->strings_size = strings_size;
```

这只是记录指针与长度，没有复制数据。后面的解析函数便可以使用这些已经检查过的范围。

我们先确认边界的理解：**总长度为 100，偏移为 100，长度为 0，`range_within` 会返回真还是假？**

## User

真。不使用read be32直接读不也是大端序吗？

## Assistant

对，那个零长度范围返回真。

至于直接读取：**内存中的字节按大端排列，不代表 CPU 会自动按大端解释它。** CPU 不知道这些字节来自 DTB。

例如内存里是：

```text
地址递增 →
00 00 01 00
```

如果“直接读”指这样：

```c
uint32_t value = *(const uint32_t *)p;
```

CPU 会按自身的数据端序解释这四个字节。MiniCore 当前运行环境使用小端序，低地址字节被当成数值的低位，因此得到：

```text
内存字节：00 00 01 00
解释结果：0x00010000 = 65536
```

而 DTB 规定按大端解释，正确结果应该是：

```text
内存字节：00 00 01 00
解释结果：0x00000100 = 256
```

`read_be32` 的作用，就是**明确指定如何组合字节**：

```c
((uint32_t)p[0] << 24) |
((uint32_t)p[1] << 16) |
((uint32_t)p[2] << 8)  |
p[3]
```

它逐字节读取，每个字节内部没有大小端的区别；再通过移位，明确把第一个字节放到最高八位。因此，不论 CPU 使用哪种端序，都得到 `256`。

可以这样区分：

| 读取方式 | 谁决定字节如何组合 |
|---|---|
| 直接通过 `uint32_t *` 读取 | CPU 的数据端序 |
| `read_be32` 逐字节组合 | 代码明确指定大端顺序 |

如果 CPU 本身工作在大端模式，直接读取在满足对齐等条件时也能得到正确结果。但我们的解析器不依赖这个前提。

## User

继续

## Assistant

接下来读保留内存表：它告诉内核哪些物理内存不能交给分配器使用。这一段会把刚学过的边界检查和大端读取组合起来。

## Activity

    $ sed -n '50,92p' src/platform/dtb.c && sed -n '199,220p' src/platform/dtb.c && sed -n '402,454p' src/platform/dtb.c
    
    static uint32_t read_be32(const void *pointer)
    {
        const uint8_t *p = pointer;
    
        return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
               ((uint32_t)p[2] << 8) | p[3];
    }
    
    static uint64_t read_be64(const void *pointer)
    {
        const uint8_t *p = pointer;
    
        return ((uint64_t)read_be32(p) << 32) | read_be32(p + 4);
    }
    
    static bool range_within(size_t total, size_t offset, size_t length)
    {
        return offset <= total && length <= total - offset;
    }
    
    static bool add_overflows_u64(uint64_t left, uint64_t right)
    {
        return left > UINT64_MAX - right;
    }
    
    static bool regions_overlap(size_t first_offset, size_t first_size,
                                size_t second_offset, size_t second_size)
    {
        return first_offset < second_offset + second_size &&
               second_offset < first_offset + first_size;
    }
    
    static bool bytes_equal_string(const uint8_t *bytes, size_t length,
                                   const char *string)
    {
        size_t string_length = strlen(string);
    
        return length == string_length + 1 && bytes[string_length] == '\0' &&
               memcmp(bytes, string, string_length) == 0;
    }
    
    static bool name_equal(const struct node *node, const char *name)
        }
        return DTB_OK;
    }
    
    static enum dtb_error add_range(struct physical_range *ranges, size_t *count,
                                    size_t capacity, uint64_t base, uint64_t size)
    {
        if (size == 0)
            return DTB_OK;
        if (add_overflows_u64(base, size))
            return DTB_ERR_PROPERTY;
        if (*count == capacity)
            return DTB_ERR_CAPACITY;
        ranges[*count].base = base;
        ranges[*count].size = size;
        ++*count;
        return DTB_OK;
    }
    
    static enum dtb_error parse_reg(const struct node *nodes, int depth,
                                    struct physical_range *ranges, size_t *count,
                                    size_t capacity)
    static enum dtb_error parse_reservations(const struct fdt_view *view,
                                             struct boot_info *info)
    {
        size_t offset = view->reservation_offset;
    
        for (;;) {
            uint64_t address;
            uint64_t size;
            enum dtb_error error;
    
            if (!range_within(view->total_size, offset, 16) ||
                regions_overlap(offset, 16,
                                (size_t)(view->structure - view->blob),
                                view->structure_size) ||
                regions_overlap(offset, 16,
                                (size_t)(view->strings - view->blob),
                                view->strings_size))
                return DTB_ERR_BOUNDS;
            address = read_be64(view->blob + offset);
            size = read_be64(view->blob + offset + 8);
            offset += 16;
            if (address == 0 && size == 0)
                return DTB_OK;
            error = add_range(info->reserved, &info->reserved_count,
                              BOOT_MAX_RESERVED_RANGES, address, size);
            if (error != DTB_OK)
                return error;
        }
    }
    
    static enum dtb_error property_name(const struct fdt_view *view,
                                        uint32_t offset, const char **name)
    {
        if (offset >= view->strings_size)
            return DTB_ERR_BOUNDS;
        for (size_t i = offset; i < view->strings_size; ++i) {
            if (view->strings[i] == '\0') {
                *name = (const char *)view->strings + offset;
                return DTB_OK;
            }
        }
        return DTB_ERR_BOUNDS;
    }
    
    static bool string_equal(const char *left, const char *right)
    {
        while (*left && *left == *right) {
            ++left;
            ++right;
        }
        return *left == *right;
    }
    
    ✓ • 0ms

## Assistant

头部验证通过后，`dtb_parse` 会记录 DTB 的位置和大小，再读取保留内存表：

```c
info->dtb_base = (uintptr_t)blob;
info->dtb_size = view.total_size;

error = parse_reservations(&view, info);
```

注意，此时 `dtb_base` 保存的是传入指针的数值，也就是当前访问 DTB 的虚拟地址；回到 `kernel_main` 后，代码才会将它转换为物理地址。

保留内存表由一条条记录组成，每条 **16 字节**：

```text
前 8 字节：物理起始地址 address
后 8 字节：区域大小 size
```

例如一条记录表示：

```text
address = 0x80000000
size    = 0x00200000
```

含义就是：从物理地址 `0x80000000` 开始的 2 MiB 内存需要保留。这个例子只是说明格式，不代表当前 DTB 一定包含它。

记录中的两个数都按大端存储，所以项目提供了 `read_be64`：

```c
static uint64_t read_be64(const void *pointer)
{
    const uint8_t *p = pointer;

    return ((uint64_t)read_be32(p) << 32) |
           read_be32(p + 4);
}
```

它把八字节分成两半：

```text
前四字节 → 高 32 位
后四字节 → 低 32 位
```

先转换为 `uint64_t` 再左移 32 位，保证移位发生在足够宽的类型上。

现在看解析循环的核心部分。边界检查暂时用注释代替，方便看清主线：

```c
size_t offset = view->reservation_offset;

for (;;) {
    /* 检查接下来这 16 字节是否合法 */

    address = read_be64(view->blob + offset);
    size = read_be64(view->blob + offset + 8);
    offset += 16;

    if (address == 0 && size == 0)
        return DTB_OK;

    error = add_range(info->reserved, &info->reserved_count,
                      BOOT_MAX_RESERVED_RANGES, address, size);
    if (error != DTB_OK)
        return error;
}
```

每一轮做四件事：

1. 确认整条记录都在 DTB 内，而且不与结构块、字符串块重叠。
2. 读取地址和大小。
3. 如果两者都为零，说明遇到结束标记。
4. 否则，把区域加入 `info->reserved`。

**这里不是靠提前给出的记录数量结束，而是靠 `(0, 0)` 这条特殊记录结束。** 如果迟迟读不到结束标记，边界检查或容量检查会让解析失败。

最后看负责保存记录的 `add_range`：

```c
static enum dtb_error add_range(struct physical_range *ranges,
                                size_t *count, size_t capacity,
                                uint64_t base, uint64_t size)
{
    if (size == 0)
        return DTB_OK;
    if (add_overflows_u64(base, size))
        return DTB_ERR_PROPERTY;
    if (*count == capacity)
        return DTB_ERR_CAPACITY;

    ranges[*count].base = base;
    ranges[*count].size = size;
    ++*count;
    return DTB_OK;
}
```

它先排除三个情况：零长度区域直接忽略；`base + size` 会溢出则报错；数组已满则报错。然后写入下一个空槽，并把记录数加一。项目为保留区域准备了 16 个槽位。

`count` 为什么是指针？因为它需要修改调用者的 `info->reserved_count`：

```c
++*count;
```

相当于：

```c
++(*count);   // 增加计数值，不是移动指针
```

这里仅仅是在**记账**，没有分配或移动任何物理内存。后面的物理页分配器会读取这份记录，避开保留区域。

确认一个细节：如果记录是 `address = 0`、`size = 4096`，它表示结束，还是表示从物理地址零开始保留一页？

## User

后者

## Assistant

对，只有地址和大小同时为零才是结束标记。接下来进入结构块，看看一串字节如何表示一棵树。

## Activity

    $ sed -n '448,580p' src/platform/dtb.c
        while (*left && *left == *right) {
            ++left;
            ++right;
        }
        return *left == *right;
    }
    
    static enum dtb_error record_property(struct node *node, const char *name,
                                          const uint8_t *value, size_t length,
                                          struct boot_info *info)
    {
        if (string_equal(name, "#address-cells")) {
            if (length != 4)
                return DTB_ERR_PROPERTY;
            node->address_cells = read_be32(value);
        } else if (string_equal(name, "#size-cells")) {
            if (length != 4)
                return DTB_ERR_PROPERTY;
            node->size_cells = read_be32(value);
        } else if (string_equal(name, "ranges")) {
            node->ranges = value;
            node->ranges_length = length;
            node->ranges_present = true;
        } else if (string_equal(name, "reg")) {
            node->reg = value;
            node->reg_length = length;
        } else if (string_equal(name, "compatible")) {
            node->compatible = value;
            node->compatible_length = length;
        } else if (string_equal(name, "device_type")) {
            node->device_type = value;
            node->device_type_length = length;
        } else if (string_equal(name, "status")) {
            node->status = value;
            node->status_length = length;
        } else if (string_equal(name, "interrupts")) {
            node->interrupts = value;
            node->interrupts_length = length;
        } else if (string_equal(name, "riscv,ndev")) {
            node->plic_source_count = value;
            node->plic_source_count_length = length;
        } else if (node->cpus && string_equal(name, "timebase-frequency")) {
            if (length != 4)
                return DTB_ERR_PROPERTY;
            info->timebase_frequency = read_be32(value);
        }
        return DTB_OK;
    }
    
    static enum dtb_error parse_structure(const struct fdt_view *view,
                                          struct boot_info *info)
    {
        struct node nodes[FDT_MAX_DEPTH];
        size_t offset = 0;
        int depth = -1;
        bool saw_root = false;
    
        while (range_within(view->structure_size, offset, 4)) {
            uint32_t token = read_be32(view->structure + offset);
    
            offset += 4;
            if (token == FDT_BEGIN_NODE) {
                size_t name_start = offset;
                struct node *node;
    
                if (depth + 1 == FDT_MAX_DEPTH)
                    return DTB_ERR_CAPACITY;
                while (offset < view->structure_size &&
                       view->structure[offset] != '\0')
                    ++offset;
                if (offset == view->structure_size)
                    return DTB_ERR_BOUNDS;
                ++depth;
                node = &nodes[depth];
                memset(node, 0, sizeof(*node));
                node->name = (const char *)view->structure + name_start;
                node->name_length = offset - name_start;
                node->address_cells = 2;
                node->size_cells = 1;
                node->reserved_memory = depth == 1 &&
                                        name_equal(node, "reserved-memory");
                node->cpus = depth == 1 && name_equal(node, "cpus");
                ++offset;
                offset = (offset + 3U) & ~(size_t)3U;
                if (depth == 0) {
                    if (saw_root || node->name_length != 0)
                        return DTB_ERR_STRUCTURE;
                    saw_root = true;
                }
            } else if (token == FDT_END_NODE) {
                enum dtb_error error;
    
                if (depth < 0)
                    return DTB_ERR_STRUCTURE;
                error = finish_node(nodes, depth, info);
                if (error != DTB_OK)
                    return error;
                --depth;
            } else if (token == FDT_PROP) {
                uint32_t length;
                uint32_t name_offset;
                const char *name;
                enum dtb_error error;
    
                if (depth < 0 || !range_within(view->structure_size, offset, 8))
                    return DTB_ERR_STRUCTURE;
                length = read_be32(view->structure + offset);
                name_offset = read_be32(view->structure + offset + 4);
                offset += 8;
                if (!range_within(view->structure_size, offset, length))
                    return DTB_ERR_BOUNDS;
                error = property_name(view, name_offset, &name);
                if (error != DTB_OK)
                    return error;
                error = record_property(&nodes[depth], name,
                                        view->structure + offset, length, info);
                if (error != DTB_OK)
                    return error;
                offset += length;
                offset = (offset + 3U) & ~(size_t)3U;
            } else if (token == FDT_NOP) {
                continue;
            } else if (token == FDT_END) {
                return saw_root && depth == -1 ? DTB_OK : DTB_ERR_STRUCTURE;
            } else {
                return DTB_ERR_STRUCTURE;
            }
        }
        return DTB_ERR_STRUCTURE;
    }
    
    enum dtb_error dtb_parse(const void *blob, struct boot_info *info)
    {
    ✓ • 0ms

## Assistant

对，`address = 0`、`size = 4096` 是一条有效的保留区域记录。

接下来，`dtb_parse` 的最后一步是：

```c
return parse_structure(&view, info);
```

这一部分代码比较长，我们先学它的骨架：**如何顺序读取一串字节，同时记住当前位于树的哪一层。**

DTB 的结构块使用一些四字节标记，代码称为 `token`：

| 标记 | 数值 | 含义 |
|---|---:|---|
| `FDT_BEGIN_NODE` | 1 | 开始一个节点 |
| `FDT_END_NODE` | 2 | 结束当前节点 |
| `FDT_PROP` | 3 | 当前节点的一条属性 |
| `FDT_NOP` | 4 | 跳过这个标记 |
| `FDT_END` | 9 | 整个结构结束 |

例如，下面这棵示意树：

```text
根节点
├─ memory
└─ cpus
```

可以按这样的顺序表达：

```text
BEGIN_NODE 根节点
    BEGIN_NODE memory
        PROP ...
    END_NODE
    BEGIN_NODE cpus
        PROP ...
    END_NODE
END_NODE
END
```

**节点的开始与结束像成对的括号，嵌套关系就表示树的层级。**

项目没有用递归函数遍历，而是用一个数组保存当前路径：

```c
struct node nodes[FDT_MAX_DEPTH];
size_t offset = 0;
int depth = -1;
bool saw_root = false;
```

分别表示：

- `nodes`：保存当前尚未结束的节点，最多 32 层。
- `offset`：当前读到结构块内哪个字节。
- `depth`：当前节点的层级，`-1` 表示还没进入节点。
- `saw_root`：是否已经见过根节点，用于检查结构是否合法。

主循环每次读取一个标记：

```c
while (range_within(view->structure_size, offset, 4)) {
    uint32_t token = read_be32(view->structure + offset);
    offset += 4;

    /* 根据 token 处理 */
}
```

注意，这里的 `offset` 是**相对结构块开头**的偏移；上一节保留内存表中的 `offset` 则是相对整个 DTB 开头的偏移。看偏移时，一定要确认它加在哪个基址上。

遇到 `BEGIN_NODE` 时，主要动作是：

```c
++depth;
node = &nodes[depth];
memset(node, 0, sizeof(*node));
```

进入更深一层，取出对应槽位，清空后记录新节点的信息。

遇到 `END_NODE` 时：

```c
error = finish_node(nodes, depth, info);
if (error != DTB_OK)
    return error;
--depth;
```

先整理当前节点的信息，将需要的硬件配置写入 `boot_info`，再退回父节点。

我们用上面的树手推：

| 读到的标记 | `depth` 的变化 | 当前节点 |
|---|---|---|
| 开始根节点 | `-1 → 0` | `nodes[0]`，根 |
| 开始 memory | `0 → 1` | `nodes[1]`，memory |
| 结束 memory | `1 → 0` | 回到根 |
| 开始 cpus | `0 → 1` | `nodes[1]`，cpus |
| 结束 cpus | `1 → 0` | 回到根 |
| 结束根节点 | `0 → -1` | 所有节点已结束 |

你会发现，`memory` 和 `cpus` 使用的是同一个槽位 `nodes[1]`。这是安全的，因为开始处理 `cpus` 时，`memory` 已经处理完了。

所以这个数组**不保存整棵树，只保存从根到当前节点的路径**。后续解析地址时，需要查询父节点，这也是保留这条路径的原因。

最后遇到 `FDT_END`：

```c
return saw_root && depth == -1 ? DTB_OK : DTB_ERR_STRUCTURE;
```

必须确实见过根节点，而且所有节点都已结束，才报告成功。

确认一下这个数组的作用：**如果根节点下面有 100 个依次出现的子节点，但没有更深的嵌套，会仅仅因为子节点超过 32 个，就触发这里的深度上限吗？**

## User

不会，因为可能多个node写在一个node槽位

## Assistant

对。更准确地说，是**同一层的兄弟节点先后复用同一个槽位**：前一个处理结束后，才写入下一个。因此限制的是嵌套深度，不是节点总数。

接下来拆开 `FDT_BEGIN_NODE` 分支，看看一个节点刚开始时要记录什么。

标记之后紧跟着节点名称，名称以 `'\0'` 结束。代码先找这个结束位置：

```c
size_t name_start = offset;

while (offset < view->structure_size &&
       view->structure[offset] != '\0')
    ++offset;

if (offset == view->structure_size)
    return DTB_ERR_BOUNDS;
```

这里为什么不用刚学过的 `strlen`？

因为解析器不能先假定输入一定合法。如果一直没有 `'\0'`，`strlen` 会继续读下去；这里每读一个字节，都确保没有超出结构块。

找到结尾后，记录名称：

```c
++depth;
node = &nodes[depth];
memset(node, 0, sizeof(*node));

node->name = (const char *)view->structure + name_start;
node->name_length = offset - name_start;
```

注意两点：

- `name` 直接指向原始 DTB 中的名称，没有复制字符串。
- `name_length` 不包含结尾的 `'\0'`。

例如名字是 `"cpus"`：

```text
字节： 'c' 'p' 'u' 's' '\0'
        ↑               ↑
    name_start        offset

name_length = 4
```

然后跳过结尾零，并移动到下一个四字节边界：

```c
++offset;
offset = (offset + 3U) & ~(size_t)3U;
```

这是**向上对齐到 4 的倍数**。先看效果：

```text
原 offset：  8   9  10  11  12
对齐后：    8  12  12  12  12
```

为什么这样算？

```text
加 3：让不是 4 的倍数的数跨到下一个四字节区间
清除最低两位：得到这个区间开头，也就是 4 的倍数
```

这些跳过的填充字节不属于名称，也不能当成下一个 token 读取。

代码还会设置两个默认值：

```c
node->address_cells = 2;
node->size_cells = 1;
```

先记住：一个 `cell` 是四字节。这两个字段描述**子节点的地址、长度各用几个 cell 编码**。后面读到 `#address-cells`、`#size-cells` 属性时，可以更新它们；解析 `reg` 时我们再具体展开。

接着识别两个需要特殊处理的节点：

```c
node->reserved_memory = depth == 1 &&
                        name_equal(node, "reserved-memory");

node->cpus = depth == 1 &&
             name_equal(node, "cpus");
```

`depth == 1` 表示它必须是根节点的直接子节点。名称相同、但出现在更深的位置，不会被这里认作目标节点。

最后，如果这是根节点，还要检查：

```c
if (depth == 0) {
    if (saw_root || node->name_length != 0)
        return DTB_ERR_STRUCTURE;
    saw_root = true;
}
```

项目要求根节点名称为空，而且不能重复出现根节点。

到这里，一个新节点就准备好了：名称已记录，默认值已设置，读取位置也对齐到了下一项。下一步便可以处理它的属性。

先确认一下对齐：**如果跳过名称末尾的 `'\0'` 后，`offset` 是 13，对齐后是多少？**

## User

16

## Assistant

对，`13` 向上对齐到四字节边界就是 `16`。

接下来，解析器可能读到 `FDT_PROP`，表示**当前节点的一条属性**。比如设备的寄存器范围 `reg`、设备类型 `compatible`，都是属性。

一条属性在结构块里的布局是：

```text
┌───────────────┐
│ FDT_PROP      │ 4 字节：标记
├───────────────┤
│ length        │ 4 字节：属性值占多少字节
├───────────────┤
│ name_offset   │ 4 字节：属性名在字符串块内的偏移
├───────────────┤
│ value         │ length 字节：属性值
├───────────────┤
│ 对齐填充      │ 0～3 字节
└───────────────┘
```

这里最重要的是：**属性名和属性值放在不同地方。**

```text
结构块中的属性
    ├─ name_offset → 字符串块里的名称，例如 "reg"
    └─ value       → 紧随当前属性头部的原始数据
```

多个节点都可能有 `"reg"` 属性，把名称集中放进字符串块，就可以复用同一个名字。

我们沿实际代码走一遍。读到 `FDT_PROP` 时，主循环已经读完 token，并将 `offset` 前进了四字节。

首先检查：

```c
if (depth < 0 ||
    !range_within(view->structure_size, offset, 8))
    return DTB_ERR_STRUCTURE;
```

两项检查分别保证：

- 当前确实位于某个节点内部，属性有归属。
- 接下来能完整读取 `length` 和 `name_offset`。

然后读取它们：

```c
length = read_be32(view->structure + offset);
name_offset = read_be32(view->structure + offset + 4);
offset += 8;
```

现在 `offset` 指向属性值开头。读取属性值之前，还要检查：

```c
if (!range_within(view->structure_size, offset, length))
    return DTB_ERR_BOUNDS;
```

**属性自己声称有多少字节，解析器不能直接相信，要确认这段数据实际位于结构块范围内。**

接下来，根据 `name_offset` 找到名称：

```c
error = property_name(view, name_offset, &name);
```

这个辅助函数也很短：

```c
static enum dtb_error property_name(const struct fdt_view *view,
                                    uint32_t offset, const char **name)
{
    if (offset >= view->strings_size)
        return DTB_ERR_BOUNDS;

    for (size_t i = offset; i < view->strings_size; ++i) {
        if (view->strings[i] == '\0') {
            *name = (const char *)view->strings + offset;
            return DTB_OK;
        }
    }
    return DTB_ERR_BOUNDS;
}
```

它先检查名称起点，再确认字符串块结束之前存在 `'\0'`。检查通过，才把名称指针交给调用者。

这里的 `const char **name` 是为了**修改调用者的指针变量**：

```c
const char *name;
property_name(view, name_offset, &name);
```

函数通过 `*name = ...`，让外面的 `name` 指向属性名称。没有复制字符串。

名称和值都定位好后，就可以记录到当前节点：

```c
error = record_property(&nodes[depth], name,
                        view->structure + offset, length, info);
```

最后跳过属性值与填充：

```c
offset += length;
offset = (offset + 3U) & ~(size_t)3U;
```

下一轮又可以读取一个四字节 token。

这一段有两套偏移，容易混淆。我们确认一下：**如果 `name_offset = 20`，应该从“结构块开头 + 20”读取属性名，还是从“字符串块开头 + 20”读取？**

## User

后者

## Assistant

对，从**字符串块开头加 20 字节**的位置读取名称。

现在名称和属性值都找到了，进入：

```c
record_property(&nodes[depth], name,
                view->structure + offset, length, info);
```

这个函数根据属性名，决定如何处理属性值。我们先看两种处理方式。

第一种：**立即解码数值**。

```c
if (string_equal(name, "#address-cells")) {
    if (length != 4)
        return DTB_ERR_PROPERTY;
    node->address_cells = read_be32(value);
}
```

`string_equal` 判断名称是否匹配，它的实际实现是：

```c
static bool string_equal(const char *left, const char *right)
{
    while (*left && *left == *right) {
        ++left;
        ++right;
    }
    return *left == *right;
}
```

它不断跳过相同字符，直到遇到不同字符或左侧结尾，再比较当前位置。

例如 `"reg"` 与 `"register"`：前三个字符相同，但随后一边是 `'\0'`，另一边是 `'i'`，所以返回假，不会把前缀相同误判成完整匹配。前面的 `property_name` 已确认属性名在合法范围内有结尾零，所以这里可以按字符串读取。

匹配 `"#address-cells"` 后，代码要求属性值恰好四字节，再用 `read_be32` 解码。

这个属性是什么意思？**规定当前节点的直接子节点，描述地址时使用几个四字节单元。**

```text
#address-cells = 1 → 地址用 4 字节
#address-cells = 2 → 地址用 8 字节
```

同理：

```c
} else if (string_equal(name, "#size-cells")) {
    if (length != 4)
        return DTB_ERR_PROPERTY;
    node->size_cells = read_be32(value);
}
```

规定子节点描述区域长度时使用几个单元。

第二种：**先保存位置和长度，稍后解码**。

例如：

```c
} else if (string_equal(name, "reg")) {
    node->reg = value;
    node->reg_length = length;
}
```

这里没有马上读出设备地址，只记住：

```text
reg 属性值在哪里？
一共有多少字节？
```

因为解释 `reg` 时，还需要结合父节点的 `address_cells`、`size_cells` 等信息。等到节点结束，`finish_node` 会汇总处理。

举一个示意例子：

```text
父节点：
    #address-cells = 2
    #size-cells    = 1

子节点：
    reg = [00000000 10000000 00000100]
```

方括号里每组八位十六进制数表示一个四字节 cell：

```text
前两个 cell：00000000 10000000 → 地址 0x10000000
后一个 cell：00000100          → 大小 0x100，即 256 字节
```

这里得到的是父总线地址空间中的地址；是否还要转换成 CPU 物理地址，后面由 `ranges` 等信息决定。

同一个函数还会记录 `compatible`、`status`、`interrupts` 等属性的位置。暂时不认识这些名称也没关系，处理对应设备时我们再逐一展开。

你现在只需要抓住这个配合关系：

```text
父节点的 cells 属性 → 告诉解析器怎样分组
子节点的 reg 数据   → 按这个分组规则解码
```

确认一下：**如果父节点的 `#address-cells = 2`、`#size-cells = 2`，子节点 `reg` 中的一组“地址＋大小”共占多少字节？**

## User

DTB解析这里我看的很抽象，有没有什么好的方法或者资料教教我

## Assistant

前面直接沿解析函数讲，缺了一份能对照的设备树实例，确实容易只看到指针和偏移。我会先找项目里的实际 DTB 或可读示例，再挑两份适合入门的资料，把讲法改成“先看配置内容，再看它怎样变成字节”。

## Activity

    $ rg --files docs build | head -60; command -v dtc; command -v qemu-system-riscv64; sed -n '1,180p' docs/notes/device-tree.md
    build/kernel.elf
    build/m4-test.o
    build/user.elf
    build/scheduler.o
    build/plic.o
    build/heap.o
    build/sbi.o
    build/trap.o
    build/head.o
    build/vm.o
    build/dtb.o
    build/main.o
    build/log.o
    build/runtime.o
    build/qemu-virt.o
    build/trap-asm.o
    build/context-asm.o
    build/user-program.o
    build/page_alloc.o
    build/user.bin
    build/user.o
    build/sync.o
    build/user-image.o
    docs/m1.md
    docs/m2.md
    docs/syscall-abi.md
    docs/roadmap-v2.md
    docs/notes/device-tree.md
    docs/notes/run.md
    docs/notes/riscv.md
    docs/notes/csr.md
    docs/notes/plic.md
    docs/notes/why-S-mode-can-manipulate-UART0?.md
    docs/notes/Linux-ISAs.md
    docs/boot.md
    docs/m4.md
    docs/linux-comparison.md
    docs/m3.md
    docs/memory-layout.md
    /usr/bin/qemu-system-riscv64
    The Device Tree ecosystem is a family of related concepts used mainly in Linux, U-Boot, BSD, and embedded systems to describe hardware separately from kernel source code.
    
    A useful way to understand it is:
    
    ```text
    DTS / DTSI
       │
       │  compiled by dtc
       ▼
    DTB
       │
       │  loaded by bootloader
       ▼
    FDT in memory
       │
       │  parsed by kernel
       ▼
    Linux device model / drivers
    ```
    
    ### Core concepts
    
    | Term              | Full name                  | Meaning                                                     |
    | ----------------- | -------------------------- | ----------------------------------------------------------- |
    | **Device Tree**   | Device Tree                | A hierarchical description of hardware                      |
    | **DTS**           | Device Tree Source         | Human-readable source form                                  |
    | **DTSI**          | Device Tree Source Include | Reusable/include fragment of DTS                            |
    | **DTB**           | Device Tree Blob           | Compiled binary Device Tree                                 |
    | **FDT**           | Flattened Device Tree      | Binary data format used by DTB                              |
    | **DTC**           | Device Tree Compiler       | Tool converting DTS ↔ DTB                                   |
    | **DTBO**          | Device Tree Blob Overlay   | Binary overlay applied to a base DTB                        |
    | **DTO / Overlay** | Device Tree Overlay        | Mechanism for modifying a base tree                         |
    | **OF**            | Open Firmware              | Historical specification/API behind Device Tree conventions |
    
    The most important distinction is:
    
    > **DTS is source code, DTB is the compiled file, and FDT is the binary format/layout used by the DTB.**
    
    ---
    
    ## 1. Device Tree
    
    A **Device Tree** describes the hardware topology of a system.
    
    For example:
    
    ```dts
    / {
        model = "Example Board";
    
        memory@80000000 {
            device_type = "memory";
            reg = <0x80000000 0x40000000>;
        };
    
        uart@10000000 {
            compatible = "ns16550a";
            reg = <0x10000000 0x100>;
            status = "okay";
        };
    };
    ```
    
    It describes things such as:
    
    * CPUs
    * RAM
    * buses
    * UART
    * I²C
    * SPI
    * GPIO
    * clocks
    * interrupts
    * Ethernet
    * displays
    * regulators
    * sensors
    
    It describes **hardware**, not driver implementation.
    
    The kernel uses properties such as:
    
    ```text
    compatible
    reg
    interrupts
    clocks
    status
    ```
    
    to discover devices and bind them to appropriate drivers.
    
    ---
    
    ## 2. DTS — Device Tree Source
    
    A `.dts` file is the main human-readable Device Tree file for a board.
    
    Example:
    
    ```text
    my-board.dts
    ```
    
    Typical structure:
    
    ```dts
    /dts-v1/;
    
    / {
        compatible = "vendor,my-board";
    
        chosen {
            bootargs = "console=ttyS0,115200";
        };
    
        uart0: serial@1000 {
            compatible = "vendor,uart";
            reg = <0x1000 0x100>;
        };
    };
    ```
    
    You can think of DTS as roughly analogous to source code:
    
    ```text
    C source        → object/binary
    DTS source      → DTB binary
    ```
    
    ---
    
    ## 3. DTSI — Device Tree Source Include
    
    A `.dtsi` file is normally a reusable Device Tree fragment.
    
    For example:
    
    ```text
    soc.dtsi
    board-common.dtsi
    chip-family.dtsi
    ```
    
    A SoC vendor might define common hardware in:
    
    ```dts
    soc.dtsi
    ```
    
    such as:
    
    ```dts
    / {
        soc {
            uart0: serial@1000 {
                compatible = "vendor,uart";
                reg = <0x1000 0x100>;
                status = "disabled";
            };
        };
    };
    ```
    
    A board DTS can include it:
    
    ```dts
    #include "soc.dtsi"
    
    &uart0 {
        status = "okay";
    };
    ```
    
    This allows one SoC description to be reused by many boards.
    
    A common hierarchy looks like:
    
    ```text
    ✓ • 0ms

## Activity

    Searched the web for site.devicetree.org device tree usage model reg address-cells, site.devicetree-specification.readthedocs.io flattened devicetree structure

## Activity

    Opened https://devicetree-specification.readthedocs.io/en/stable/devicetree-basics.html

## Assistant

有。我们应该换个讲法：**先看一份具体的硬件配置，再观察它怎样编码成 DTB，最后让解析代码把它还原出来。**

前面我推进得太偏向函数细节了。你需要同时理解树、二进制布局、指针偏移和硬件属性，缺少一个固定例子作参照，确实容易抽象。

之后我们固定追踪一个问题：**内核怎样知道 RAM 从哪里开始、有多大？** 先把这条路径讲透，再扩展到 UART 等设备。

先看下面这份教学用 DTS。DTS 是设备树的人类可读形式；这不是从当前 QEMU 导出的完整配置：

```dts
/dts-v1/;

/ {
    #address-cells = <2>;
    #size-cells = <2>;

    memory@80000000 {
        device_type = "memory";
        reg = <0x0 0x80000000 0x0 0x10000000>;
    };
};
```

它想表达的事情其实很简单：

> 这台机器有一段 RAM，从物理地址 `0x80000000` 开始，大小是 `0x10000000`，也就是 256 MiB。

我们把语法和含义一一对应：

| DTS 内容 | 用人话解释 |
|---|---|
| `/ { ... }` | 整台机器的根节点 |
| `memory@80000000` | 描述一段从这个地址开始的内存 |
| `device_type = "memory"` | 这是内存节点 |
| `#address-cells = <2>` | 子节点的地址用两个 32 位数表示 |
| `#size-cells = <2>` | 子节点的大小也用两个 32 位数表示 |

所以 `reg` 的四个数应该这样分组：

```text
reg = <0x0 0x80000000    0x0 0x10000000>;
       └────地址────┘    └────大小────┘

地址 = (0 << 32) | 0x80000000
大小 = (0 << 32) | 0x10000000
```

父节点规定子节点 `reg` 的编码方式，这是设备树规范定义的规则。[设备树基础：属性与地址编码](https://devicetree-specification.readthedocs.io/en/stable/devicetree-basics.html)

**整个解析器的工作，就是从这份配置的二进制版本，最后得到：**

```c
boot_info.memory[0].base = 0x80000000;
boot_info.memory[0].size = 0x10000000;
boot_info.memory_count = 1;
```

有了这个明确的终点，再回头看函数就容易理解：

```text
这段 RAM 的描述
    ↓ 编码
DTB 中的节点、属性、字节
    ↓ 解析
boot_info.memory[0]
    ↓ 使用
物理页分配器知道 RAM 的范围
```

我们接下来的课程就用这一份例子，分三次走：

1. **读懂 DTS**：只关心它描述了什么硬件。
2. **展开 DTB**：把同一份配置画成 token、名称、属性值和字节位置。
3. **模拟执行解析器**：每一步同时展示“读到了什么”和“哪个变量发生变化”。

边界检查、错误路径放在这条正常路径理解之后补齐。你不需要自己跳文件，我会把同一份数据和相关代码并排带进聊天。

资料建议只保留这三处，按需要查，不用现在通读：

- 项目里的 [device-tree.md](/home/celestee/prj/minicore/docs/notes/device-tree.md)：先看开头的术语对照，分清 DTS、DTB、DTC。里面的示例较简化，地址编码我们用上面明确写出 cells 的例子。
- [官方规范第 2 章](https://devicetree-specification.readthedocs.io/en/stable/devicetree-basics.html)：先看示例图，以及 `#address-cells`、`#size-cells`、`reg`，解决“配置是什么意思”。
- [官方规范第 5 章](https://devicetree-specification.readthedocs.io/en/stable/flattened-format.html)：等我们展开二进制时再看，其中给出了头部、结构块和字符串块的格式。

这一轮先不考位运算。你只要抓住一个目的：**我们在读一份机器配置，把“RAM 在哪里、有多大”提取成 C 结构体。**

## User

继续

## Assistant

我们继续用同一份配置，看看它**如何变成内存中的字节**。这一轮暂时不看 C 函数。

```dts
/ {
    #address-cells = <2>;
    #size-cells = <2>;

    memory@80000000 {
        device_type = "memory";
        reg = <0x0 0x80000000 0x0 0x10000000>;
    };
};
```

我们的目标仍然是得到：

```text
RAM 起始地址：0x80000000
RAM 大小：    0x10000000，也就是 256 MiB
```

**第一步：把大括号变成开始、结束标记。**

DTS 里用 `{ }` 表示嵌套，DTB 用 `BEGIN_NODE` 和 `END_NODE`。把上面这份配置展开，大致就是：

```text
BEGIN_NODE ""                         ← 根节点名称为空
    PROP "#address-cells" = 2
    PROP "#size-cells" = 2

    BEGIN_NODE "memory@80000000"
        PROP "device_type" = "memory"
        PROP "reg" = 四个数
    END_NODE

END_NODE
END
```

它仍然表达同一棵树，只是现在可以从头到尾顺序读取了。

**第二步：把属性名称集中放到字符串块。**

为了让偏移具体可见，我们人为安排如下字符串块。实际生成的 DTB 可以采用不同的排列：

| 字符串块内的字节偏移 | 内容 | 含结尾零的长度 |
|---:|---|---:|
| 0 | `"#address-cells\0"` | 15 字节 |
| 15 | `"#size-cells\0"` | 12 字节 |
| 27 | `"device_type\0"` | 12 字节 |
| 39 | `"reg\0"` | 4 字节 |

于是，结构块里的属性不必重复写名称，只需记录偏移：

```text
PROP：名称在字符串块偏移 0，值为 2
PROP：名称在字符串块偏移 15，值为 2
PROP：名称在字符串块偏移 27，值为 "memory"
PROP：名称在字符串块偏移 39，值为四个数
```

这里有一个容易混淆的区别：

```text
"device_type" 是属性名 → 放在字符串块
"memory" 是属性值     → 留在结构块
```

**字符串块并不存放所有字符串，它存放属性名称。** 节点名称也留在结构块里。

**第三步：只放大 `reg` 这一条属性。**

它在 DTS 中是：

```dts
reg = <0x0 0x80000000 0x0 0x10000000>;
```

四个 cell，每个四字节，所以属性值长 **16 字节**。

按照我们安排的字符串块，`"reg"` 的偏移是 **39，即十六进制 `0x27`**。

因此，这条属性编码成：

```text
字节内容                         含义
────────────────────────────────────────────
00 00 00 03                      FDT_PROP 标记
00 00 00 10                      属性值长度：16
00 00 00 27                      属性名称偏移：39

00 00 00 00                      地址的高 32 位
80 00 00 00                      地址的低 32 位
00 00 00 00                      大小的高 32 位
10 00 00 00                      大小的低 32 位
```

这整条属性占 **28 字节：12 字节属性头，加 16 字节属性值**。已经按四字节对齐，不需要额外填充。

现在站在解析器的角度，它会做：

```text
读到 3
  → 知道这是一条属性

读到长度 16
  → 知道后面的属性值占 16 字节

读到名称偏移 39
  → 去字符串块 +39，找到 "reg"

取出后面的 16 字节
  → 根据父节点规定，前 8 字节是地址，后 8 字节是大小
```

前面那些变量现在都有了具体对应：

```text
length      = 16
name_offset = 39
name        → "reg"
value       → 那 16 字节属性值的开头
```

这就是从 DTS 中的一行 `reg = ...`，到解析器手里的四个变量的全过程。

我们先停在这张字节表上：**其中的 `00 00 00 27` 是 RAM 地址的一部分吗？还是用来寻找 `"reg"` 这个名称的？**

## User

hz v

## User

后者

## Assistant

对，`0x27` 是名称在字符串块中的偏移，和 RAM 地址无关。

现在把这条具体的 `reg` 数据送进项目代码。我们继续使用：

```dts
reg = <0x0 0x80000000 0x0 0x10000000>;
```

解析到这里，已经得到：

```text
name   → "reg"
value  → 16 字节属性值
length = 16
```

`record_property` 遇到 `"reg"` 时，执行：

```c
node->reg = value;
node->reg_length = length;
```

**此刻还没有算出 RAM 地址，只是记住原始数据在哪里。**

同一个节点里的：

```dts
device_type = "memory";
```

也会被记录下来：

```c
node->device_type = value;
node->device_type_length = length;
```

因此，读完 `memory@80000000` 的属性后，当前节点大致保存着：

```text
nodes[1]
├─ name        → "memory@80000000"
├─ device_type → "memory"
└─ reg         → 00 00 00 00 80 00 00 00
                 00 00 00 00 10 00 00 00
```

而父节点的编码规则仍保存在：

```text
nodes[0]
├─ address_cells = 2
└─ size_cells    = 2
```

这就是为什么前面要保存父节点：**子节点保存数据，父节点告诉我们怎样解释数据。**

接下来读到 `END_NODE`，解析器调用：

```c
finish_node(nodes, depth, info);
```

此时 `depth = 1`。处理内存节点时，后续代码会根据父节点规则，把 `reg` 分成：

```text
第 1、2 个 cell → 地址
第 3、4 个 cell → 大小
```

负责把 cell 合成数值的函数是：

```c
static enum dtb_error decode_cells(const uint8_t *data,
                                   uint32_t cells,
                                   uint64_t *value)
{
    if (cells > 2)
        return DTB_ERR_UNSUPPORTED;

    *value = 0;
    for (uint32_t i = 0; i < cells; ++i)
        *value = (*value << 32) | read_be32(data + i * 4U);

    return DTB_OK;
}
```

先看地址的两个 cell：

```text
00000000 80000000
```

循环这样计算：

```text
初始 value = 0

第 1 轮：
value = (0 << 32) | 0x00000000
      = 0

第 2 轮：
value = (0 << 32) | 0x80000000
      = 0x80000000
```

每轮的动作就是：**把已经读到的内容向高位移动 32 位，再接上新的四字节。**

大小的两个 cell 同理：

```text
00000000 10000000 → 0x10000000
```

最终，把解码后的数值加入内存范围数组：

```text
boot_info.memory[0].base = 0x80000000
boot_info.memory[0].size = 0x10000000
boot_info.memory_count  = 1
```

这里描述的是整段 RAM；其中哪些区域属于固件、内核或其他保留用途，还要由后面的分配器排除。

现在整条路径就连起来了：

```text
DTS：reg = <0 0x80000000 0 0x10000000>
  ↓
DTB：16 字节大端数据
  ↓
record_property：记住数据位置
  ↓
节点结束：按父节点的 cells 规则解码
  ↓
boot_info：RAM 从哪里开始、有多大
```

注意最后发生的变化：`node->reg` 是**指向原始字节的指针**，而 `boot_info.memory[0].base` 已经是**解码后的物理地址数值**。

## User

继续

## Assistant

接下来对照实际代码，把“识别内存节点 → 解码 `reg` → 写入 `boot_info`”这段补完整，仍然用刚才那段 256 MiB RAM 作例子。

## Activity

    $ sed -n '215,350p' src/platform/dtb.c
        return DTB_OK;
    }
    
    static enum dtb_error parse_reg(const struct node *nodes, int depth,
                                    struct physical_range *ranges, size_t *count,
                                    size_t capacity)
    {
        const struct node *node = &nodes[depth];
        const struct node *parent;
        size_t tuple_cells;
        size_t tuple_size;
    
        if (!node->reg)
            return DTB_ERR_PROPERTY;
        if (depth == 0)
            return DTB_ERR_STRUCTURE;
        parent = &nodes[depth - 1];
        if (parent->address_cells > 2 || parent->size_cells > 2)
            return DTB_ERR_UNSUPPORTED;
        tuple_cells = (size_t)parent->address_cells + parent->size_cells;
        tuple_size = tuple_cells * 4U;
        if (tuple_size == 0 || node->reg_length == 0 ||
            node->reg_length % tuple_size != 0)
            return DTB_ERR_PROPERTY;
    
        for (size_t offset = 0; offset < node->reg_length; offset += tuple_size) {
            uint64_t address;
            uint64_t size;
            enum dtb_error error;
    
            error = decode_cells(node->reg + offset, parent->address_cells,
                                 &address);
            if (error != DTB_OK)
                return error;
            error = decode_cells(node->reg + offset + parent->address_cells * 4U,
                                 parent->size_cells, &size);
            if (error != DTB_OK)
                return error;
            error = translate_address(nodes, depth, size, &address);
            if (error != DTB_OK)
                return error;
            error = add_range(ranges, count, capacity, address, size);
            if (error != DTB_OK)
                return error;
        }
        return DTB_OK;
    }
    
    static enum dtb_error parse_first_reg(const struct node *nodes, int depth,
                                          struct physical_range *range)
    {
        size_t count = 0;
        enum dtb_error error = parse_reg(nodes, depth, range, &count, 1);
    
        if (error != DTB_OK)
            return error;
        return count == 1 ? DTB_OK : DTB_ERR_PROPERTY;
    }
    
    static enum dtb_error finish_node(const struct node *nodes, int depth,
                                      struct boot_info *info)
    {
        const struct node *node = &nodes[depth];
        bool enabled = node_enabled(node);
        enum dtb_error error;
    
        if (enabled && node->device_type &&
            bytes_equal_string(node->device_type, node->device_type_length,
                               "memory")) {
            error = parse_reg(nodes, depth, info->memory, &info->memory_count,
                              BOOT_MAX_MEMORY_RANGES);
            if (error != DTB_OK)
                return error;
        }
    
        if (enabled && depth > 0 && nodes[depth - 1].reserved_memory &&
            node->reg) {
            error = parse_reg(nodes, depth, info->reserved,
                              &info->reserved_count, BOOT_MAX_RESERVED_RANGES);
            if (error != DTB_OK)
                return error;
        }
    
        if (!enabled)
            return DTB_OK;
    
        if (compatible_has(node, "ns16550a") ||
            compatible_has(node, "sifive,uart0")) {
            if (info->uart_present)
                return DTB_ERR_CAPACITY;
            error = parse_first_reg(nodes, depth, &info->uart);
            if (error != DTB_OK)
                return error;
            if (node->interrupts) {
                if (node->interrupts_length != 4)
                    return DTB_ERR_UNSUPPORTED;
                info->uart_irq = read_be32(node->interrupts);
            }
            info->uart_present = true;
        }
    
        if (compatible_has(node, "sifive,plic-1.0.0") ||
            compatible_has(node, "riscv,plic0")) {
            if (info->plic_present)
                return DTB_ERR_CAPACITY;
            error = parse_first_reg(nodes, depth, &info->plic);
            if (error != DTB_OK)
                return error;
            if (node->plic_source_count) {
                if (node->plic_source_count_length != 4)
                    return DTB_ERR_PROPERTY;
                info->plic_source_count = read_be32(node->plic_source_count);
            }
            info->plic_present = true;
        }
    
        if (compatible_has(node, "virtio,mmio")) {
            if (info->virtio_count == BOOT_MAX_VIRTIO_DEVICES)
                return DTB_ERR_CAPACITY;
            error = parse_first_reg(nodes, depth,
                                    &info->virtio[info->virtio_count]);
            if (error != DTB_OK)
                return error;
            if (node->interrupts) {
                if (node->interrupts_length != 4)
                    return DTB_ERR_UNSUPPORTED;
                info->virtio_irqs[info->virtio_count] =
                    read_be32(node->interrupts);
            }
            ++info->virtio_count;
        }
    
        return DTB_OK;
    }
    
    static enum dtb_error validate_header(const void *blob, struct fdt_view *view)
    ✓ • 0ms

## Assistant

上一轮我们看到了最后的结果，这次把中间两步对上实际代码。例子保持不变：

```dts
/ {
    #address-cells = <2>;
    #size-cells = <2>;

    memory@80000000 {
        device_type = "memory";
        reg = <0 0x80000000 0 0x10000000>;
    };
};
```

当解析器读到这个内存节点的 `END_NODE`，就调用 `finish_node`。

**它首先判断：这是一个需要处理的内存节点吗？**

相关代码是：

```c
const struct node *node = &nodes[depth];
bool enabled = node_enabled(node);

if (enabled && node->device_type &&
    bytes_equal_string(node->device_type,
                       node->device_type_length, "memory")) {
    error = parse_reg(nodes, depth,
                      info->memory, &info->memory_count,
                      BOOT_MAX_MEMORY_RANGES);
    if (error != DTB_OK)
        return error;
}
```

这里并不是只看节点名称里有没有 `"memory"`，而是检查 `device_type` 属性。

我们的例子没有写 `status`，项目的 `node_enabled` 会默认认为它启用。如果写了 `status`，则要求值是 `"okay"` 或 `"ok"`。

`bytes_equal_string` 检查属性值是否恰好是 `"memory\0"`：长度、结尾零和字符内容都要匹配。

条件满足后，进入 `parse_reg`。传入的输出位置是：

```text
info->memory         → 保存 RAM 范围的数组
&info->memory_count  → 已记录范围的数量
```

**接下来，确定一组“地址＋大小”占多少字节。**

```c
const struct node *node = &nodes[depth];
parent = &nodes[depth - 1];

tuple_cells = (size_t)parent->address_cells + parent->size_cells;
tuple_size = tuple_cells * 4U;
```

这里 `tuple` 就是“一组数据”，在本例中：

```text
地址 2 个 cell + 大小 2 个 cell
= 4 个 cell
= 16 字节
```

所以 `reg` 的属性值必须由完整的 16 字节组构成：

```c
if (tuple_size == 0 || node->reg_length == 0 ||
    node->reg_length % tuple_size != 0)
    return DTB_ERR_PROPERTY;
```

比如属性值长 18 字节，就不可能按这种规则完整分组，解析器会报错。

**然后循环处理每一组。**

为什么需要循环？因为一个 `reg` 可以描述多段区域。我们当前只有一组，所以执行一次：

```c
for (size_t offset = 0;
     offset < node->reg_length;
     offset += tuple_size) {
    /* 解码并保存一组区域 */
}
```

循环里的两次解码分别是：

```c
decode_cells(node->reg + offset,
             parent->address_cells, &address);

decode_cells(node->reg + offset + parent->address_cells * 4U,
             parent->size_cells, &size);
```

代入本例，位置就很清楚了：

```text
reg 起点 +0：00 00 00 00 80 00 00 00 → address = 0x80000000
reg 起点 +8：00 00 00 00 10 00 00 00 → size    = 0x10000000
```

接着调用：

```c
translate_address(nodes, depth, size, &address);
```

它负责处理总线地址转换。**本例的内存节点直接位于根节点下面，`depth = 1`，不需要穿过中间总线，地址保持不变。** 等讲 UART 所在的总线时，我们再展开这个函数。

最后：

```c
add_range(ranges, count, capacity, address, size);
```

把结果存进之前传入的 `info->memory`，并增加计数：

```text
memory[0] = { base: 0x80000000, size: 0x10000000 }
memory_count = 1
```

所以这两个函数的分工是：

```text
finish_node：判断这个节点描述什么，决定结果存到哪里
parse_reg：按照父节点规则，逐组解码地址和大小
```

这次我们不算偏移，换一个理解题：**如果这个节点的 `reg` 有 32 字节，而父节点仍规定地址占两个 cell、大小占两个 cell，它描述了几段区域？**

## User

我们跳过DTB解析吧，不想看了

## Assistant

好，DTB 解析先跳过。我们把它当成一个已经完成的步骤：

```text
输入：OpenSBI 传来的 DTB
输出：boot_info，包含 RAM 范围、设备地址、时钟频率等信息
```

以后用到其中某个字段时，我再解释它的含义，不继续展开解析代码。

接下来进入**异常和中断**。这一部分能回答一个关键问题：CPU 正在执行内核代码时，遇到错误或定时器到期，怎么转去处理，再回来？

在项目里，完成硬件信息检查后，会执行：

```c
trap_init();

if (!trap_run_m1_tests(boot_info.timebase_frequency))
    panic("trap/timer self-test failed");
```

先建立直觉：普通函数调用由代码主动发出：

```text
执行 call → 进入函数 → 执行 ret → 回到调用处
```

而异常或中断可以在没有 `call` 的情况下，让 CPU 改变执行位置。RISC-V 把这类进入处理程序的过程统称为 **trap**。

两种典型来源是：

| 来源 | 例子 | 与当前指令的关系 |
|---|---|---|
| 同步异常 | 执行非法指令、访问未映射地址 | 由当前指令触发 |
| 中断 | 定时器到期 | 由异步事件触发 |

之前你说的“没有页表映射，触发 page fault”，就是同步异常。

CPU 进入 S-mode trap 时，几个专用寄存器帮助内核接手：

| 寄存器 | 作用 |
|---|---|
| `stvec` | 指定 trap 入口 |
| `sepc` | 记录返回所需的程序位置 |
| `scause` | 记录发生了什么异常或中断 |
| `stval` | 在适用的异常中提供额外信息，例如出错地址 |

你可以先把路径理解成：

```text
正在执行代码
    ↓ 异常或中断
CPU 记录原因和程序位置，转到 stvec 指定的入口
    ↓
内核保存现场、处理事件
    ↓
恢复现场，通过 sret 返回
```

这里和普通调用有一个很重要的区别：**被打断的代码没有机会提前准备。** 它可能刚把一个计算结果放进 `a0`，下一条指令还要使用它。

因此，trap 处理程序不能随意覆盖这些寄存器。接下来我们就沿着 `trap_init → 汇编入口 → C 处理函数`，看 MiniCore 怎样保存和恢复现场。

## User

继续

## Assistant

我先对照 `trap_init` 和汇编入口，带你看 CPU 跳入处理程序后，最先保存哪些状态。

## Activity

    $ rg -n 'trap_init|trap_handler|trap_dispatch|stvec' src/arch/riscv/trap.c src/arch/riscv/trap.S && sed -n '1,210p' src/arch/riscv/trap.S && sed -n '1,140p' src/arch/riscv/trap.h && rg -n -A25 'void trap_init' src/arch/riscv/trap.c
    src/arch/riscv/trap.S:69:    call trap_dispatch
    src/arch/riscv/trap.c:121:void trap_dispatch(struct trap_frame *frame)
    src/arch/riscv/trap.c:176:void trap_init(void)
    src/arch/riscv/trap.c:180:    csr_write_stvec((uintptr_t)trap_entry);
    #define TF_X(n) ((n) * 8)
    #define TF_SEPC (32 * 8)
    #define TF_SSTATUS (33 * 8)
    #define TF_SCAUSE (34 * 8)
    #define TF_STVAL (35 * 8)
    #define TF_SIZE (36 * 8)
    
        .section .text.trap, "ax", @progbits
        .align 2
        .globl trap_entry
        .type trap_entry, @function
    trap_entry:
        /* sscratch is zero in S-mode.  While U-mode runs it contains the kernel
         * stack pointer, so this swap selects a safe stack before any store. */
        csrrw sp, sscratch, sp
        bnez sp, .Ltrap_have_stack
        csrrw sp, sscratch, sp
    .Ltrap_have_stack:
        addi sp, sp, -TF_SIZE
        sd zero, TF_X(0)(sp)
        sd ra, TF_X(1)(sp)
        sd t0, TF_X(5)(sp)
        addi t0, sp, TF_SIZE
        sd t0, TF_X(2)(sp)
        sd gp, TF_X(3)(sp)
        sd tp, TF_X(4)(sp)
        sd t1, TF_X(6)(sp)
        sd t2, TF_X(7)(sp)
        sd s0, TF_X(8)(sp)
        sd s1, TF_X(9)(sp)
        sd a0, TF_X(10)(sp)
        sd a1, TF_X(11)(sp)
        sd a2, TF_X(12)(sp)
        sd a3, TF_X(13)(sp)
        sd a4, TF_X(14)(sp)
        sd a5, TF_X(15)(sp)
        sd a6, TF_X(16)(sp)
        sd a7, TF_X(17)(sp)
        sd s2, TF_X(18)(sp)
        sd s3, TF_X(19)(sp)
        sd s4, TF_X(20)(sp)
        sd s5, TF_X(21)(sp)
        sd s6, TF_X(22)(sp)
        sd s7, TF_X(23)(sp)
        sd s8, TF_X(24)(sp)
        sd s9, TF_X(25)(sp)
        sd s10, TF_X(26)(sp)
        sd s11, TF_X(27)(sp)
        sd t3, TF_X(28)(sp)
        sd t4, TF_X(29)(sp)
        sd t5, TF_X(30)(sp)
        sd t6, TF_X(31)(sp)
    
        csrr t0, sepc
        sd t0, TF_SEPC(sp)
        csrr t0, sstatus
        sd t0, TF_SSTATUS(sp)
        csrr t0, scause
        sd t0, TF_SCAUSE(sp)
        csrr t0, stval
        sd t0, TF_STVAL(sp)
    
        /* C code must not inherit a deliberately clobbered or future user gp. */
        .option push
        .option norelax
        la gp, __global_pointer$
        .option pop
        mv a0, sp
        call trap_dispatch
    
        ld t0, TF_SEPC(sp)
        csrw sepc, t0
        ld t0, TF_SSTATUS(sp)
        csrw sstatus, t0
    
        /* Keep the saved privilege bit as an epilogue discriminator. */
        andi t0, t0, (1 << 8)
        sd t0, TF_STVAL(sp)
    
        ld ra, TF_X(1)(sp)
        ld gp, TF_X(3)(sp)
        ld tp, TF_X(4)(sp)
        ld t2, TF_X(7)(sp)
        ld s0, TF_X(8)(sp)
        ld s1, TF_X(9)(sp)
        ld a0, TF_X(10)(sp)
        ld a1, TF_X(11)(sp)
        ld a2, TF_X(12)(sp)
        ld a3, TF_X(13)(sp)
        ld a4, TF_X(14)(sp)
        ld a5, TF_X(15)(sp)
        ld a6, TF_X(16)(sp)
        ld a7, TF_X(17)(sp)
        ld s2, TF_X(18)(sp)
        ld s3, TF_X(19)(sp)
        ld s4, TF_X(20)(sp)
        ld s5, TF_X(21)(sp)
        ld s6, TF_X(22)(sp)
        ld s7, TF_X(23)(sp)
        ld s8, TF_X(24)(sp)
        ld s9, TF_X(25)(sp)
        ld s10, TF_X(26)(sp)
        ld s11, TF_X(27)(sp)
        ld t3, TF_X(28)(sp)
        ld t4, TF_X(29)(sp)
        ld t5, TF_X(30)(sp)
        ld t6, TF_X(31)(sp)
    
        ld t0, TF_STVAL(sp)
        beqz t0, .Ltrap_return_user
        csrw sscratch, zero
        ld t0, TF_X(5)(sp)
        ld t1, TF_X(6)(sp)
        ld sp, TF_X(2)(sp)
        sret
    
    .Ltrap_return_user:
        ld t0, TF_X(2)(sp)
        csrw sscratch, t0
        ld t0, TF_X(5)(sp)
        ld t1, TF_X(6)(sp)
        addi sp, sp, TF_SIZE
        csrrw sp, sscratch, sp
        sret
        .size trap_entry, . - trap_entry
    
        .section .text, "ax", @progbits
        .align 2
        .globl trap_trigger_test_exceptions
        .type trap_trigger_test_exceptions, @function
    trap_trigger_test_exceptions:
        csrw cycle, zero
        ebreak
        ld zero, 0(zero)
        ret
        .size trap_trigger_test_exceptions, . - trap_trigger_test_exceptions
    
        .align 2
        .globl trap_register_test
        .type trap_register_test, @function
    trap_register_test:
        addi sp, sp, -128
        sd ra, 0(sp)
        sd gp, 8(sp)
        sd tp, 16(sp)
        sd s0, 24(sp)
        sd s1, 32(sp)
        sd s2, 40(sp)
        sd s3, 48(sp)
        sd s4, 56(sp)
        sd s5, 64(sp)
        sd s6, 72(sp)
        sd s7, 80(sp)
        sd s8, 88(sp)
        sd s9, 96(sp)
        sd s10, 104(sp)
        sd s11, 112(sp)
        .option push
        .option norelax
        la t0, trap_register_test_sp
        .option pop
        sd sp, 0(t0)
    
        li ra, 1
        li gp, 3
        li tp, 4
        li t0, 5
        li t1, 6
        li t2, 7
        li s0, 8
        li s1, 9
        li a0, 10
        li a1, 11
        li a2, 12
        li a3, 13
        li a4, 14
        li a5, 15
        li a6, 16
        li a7, 17
        li s2, 18
        li s3, 19
        li s4, 20
        li s5, 21
        li s6, 22
        li s7, 23
        li s8, 24
        li s9, 25
        li s10, 26
        li s11, 27
        li t3, 28
        li t4, 29
        li t5, 30
        li t6, 31
        .globl trap_register_window_start
    trap_register_window_start:
        wfi
        j trap_register_window_start
    
        .globl trap_register_window_end
    trap_register_window_end:
        .option push
        .option norelax
        la t0, trap_register_test_sp
        .option pop
        ld sp, 0(t0)
        ld ra, 0(sp)
        ld gp, 8(sp)
        ld tp, 16(sp)
        ld s0, 24(sp)
        ld s1, 32(sp)
    #ifndef MINICORE_RISCV_TRAP_H
    #define MINICORE_RISCV_TRAP_H
    
    #include <stdbool.h>
    #include <stdint.h>
    
    struct trap_frame {
        uint64_t x[32];
        uint64_t sepc;
        uint64_t sstatus;
        uint64_t scause;
        uint64_t stval;
    };
    
    void trap_init(void);
    bool trap_run_m1_tests(uint64_t timebase_frequency);
    bool trap_expect_page_fault(volatile uint64_t *address, bool write);
    void trap_dispatch(struct trap_frame *frame);
    
    #endif
    176:void trap_init(void)
    177-{
    178-    csr_clear_sstatus(SSTATUS_SIE);
    179-    csr_clear_sie(UINT64_MAX);
    180-    csr_write_stvec((uintptr_t)trap_entry);
    181-}
    182-
    183-bool trap_run_m1_tests(uint64_t timebase_frequency)
    184-{
    185-    exception_index = 0;
    186-    timer_ticks = 0;
    187-    register_ticks = 0;
    188-    register_failures = 0;
    189-    test_running = true;
    190-
    191-    trap_trigger_test_exceptions();
    192-    if (exception_index !=
    193-        sizeof(expected_exceptions) / sizeof(expected_exceptions[0]))
    194-        return false;
    195-
    196-    timer_interval = timebase_frequency / 100;
    197-    if (timer_interval == 0)
    198-        timer_interval = 1;
    199-    sbi_set_timer(csr_read_time() + timer_interval);
    200-    csr_set_sie(SIE_STIE);
    201-    csr_set_sstatus(SSTATUS_SIE);
    ✓ • 0ms

## Assistant

我们先聚焦一个场景：**内核正在 S-mode 执行，突然发生异常。** 用户态进入内核的情况，后面再讲。

第一步是提前设置入口。项目的 `trap_init` 只有三行：

```c
void trap_init(void)
{
    csr_clear_sstatus(SSTATUS_SIE);
    csr_clear_sie(UINT64_MAX);
    csr_write_stvec((uintptr_t)trap_entry);
}
```

这里的 CSR 是 CPU 的控制和状态寄存器，和用来计算的 `a0`、`t0` 等通用寄存器用途不同。

三行分别做：

| 代码 | 作用 |
|---|---|
| 清除 `sstatus.SIE` | 关闭 S-mode 的中断总开关 |
| 清除 `sie` 中各位 | 关闭各类中断的单独使能 |
| 写入 `stvec` | 指定 trap 入口为 `trap_entry` |

注意：**关闭中断不会关闭同步异常。** 即使定时器中断没启用，执行非法指令仍然会触发异常。

`trap_entry` 在汇编中按四字节对齐，因此地址低两位为零。把这个地址直接写入 `stvec`，选择的是 Direct 模式：交给 S-mode 处理的 trap 统一进入这个入口。

现在入口准备好了。假设内核某条指令引发异常，CPU 会保存程序位置、原因等状态，然后跳到 `trap_entry`。

但有一件关键的事：**CPU 不会自动把所有通用寄存器压入栈，也不会像 `call` 那样把返回位置写进 `ra`。**

异常返回位置保存在 `sepc`，而通用寄存器需要入口汇编自己保存。

项目用一个 C 结构表示保存的现场：

```c
struct trap_frame {
    uint64_t x[32];
    uint64_t sepc;
    uint64_t sstatus;
    uint64_t scause;
    uint64_t stval;
};
```

它叫 **trap frame，异常现场保存区**：

```text
x[32]    → 32 个通用寄存器的槽位
sepc     → 返回所需的程序位置
sstatus  → 特权级、中断状态等
scause   → 异常或中断原因
stval    → 相关附加信息
```

每项八字节，总大小是：

```text
(32 + 4) × 8 = 288 字节
```

入口先选择合适的栈，然后执行：

```asm
addi sp, sp, -TF_SIZE
```

其中 `TF_SIZE = 288`。这和之前函数预留栈空间的方式一样，只是这次需要保存整个整数寄存器现场。

随后逐个写入：

```asm
sd ra, TF_X(1)(sp)
sd t0, TF_X(5)(sp)
...
sd a0, TF_X(10)(sp)
```

`TF_X(n)` 定义为 `n * 8`，正好对应 C 结构中的 `x[n]`。例如，`a0` 是 `x10`，所以保存到 `sp + 80`。

保存完成后，汇编这样调用 C 处理函数：

```asm
mv a0, sp
call trap_dispatch
```

也就是：

```c
trap_dispatch(指向这份现场的指针);
```

这里有个非常重要的顺序：**先保存原来的 `a0`，再用 `a0` 传参。** 否则被打断代码的 `a0` 就丢了。

于是目前这段流程是：

```text
CPU 进入 trap_entry
    → 汇编准备栈上的 trap_frame
    → 保存寄存器和异常信息
    → 把 trap_frame 指针传给 trap_dispatch
```

下一步我们会具体拆入口的选栈和保存顺序。先确认：**为什么不能一进入 `trap_entry` 就直接 `call trap_dispatch`，等 C 函数里面再保存原来的 `ra`？**

## User

不知道

## Assistant

因为 **`call` 本身就会改写 `ra`，等进入 C 函数再保存，已经晚了。**

我们用具体数值走一遍。假设异常发生前：

```text
ra = 0x80201000   ← 被打断的函数以后要返回的位置
```

CPU 进入 `trap_entry` 时，这个 `ra` 仍然保留着。但如果入口马上执行：

```asm
call trap_dispatch
```

调用指令就会把 `ra` 改为：

```text
ra = trap_entry 中 call 后面那条指令的地址
```

此时原来的 `0x80201000` 已经被覆盖。C 函数即使把 `ra` 保存到栈上，保存的也是**新返回地址**。

正确顺序因此是：

```asm
sd ra, TF_X(1)(sp)    # 先备份被打断代码的 ra
...
call trap_dispatch   # 现在可以改写 ra
```

处理结束后，再从保存区恢复原来的 `ra`。

这里要分清两个返回位置：

| 寄存器 | 此时保存的意义 |
|---|---|
| `sepc` | 异常处理完后，回到被打断代码的哪里 |
| 原来的 `ra` | 被打断的那个函数，将来返回其调用者的位置 |

例如：

```text
函数 A 调用函数 B
              ↓
          B 执行时发生异常
              ↓
          trap 处理程序
```

`sepc` 帮我们回到 **B 的执行现场**；B 原来的 `ra` 帮 B 以后回到 **A**。两者都要保留。

所以即使 `sret` 能靠 `sepc` 正确回到 B，如果没有恢复 B 原来的 `ra`，B 之后执行 `ret` 时仍然会跳错位置。

## User

明白了

## Assistant

接下来我们看：**保存寄存器之前，入口怎样确定往哪块栈里保存。**

`trap_entry` 最前面的代码是：

```asm
csrrw sp, sscratch, sp
bnez sp, .Ltrap_have_stack
csrrw sp, sscratch, sp

.Ltrap_have_stack:
    addi sp, sp, -TF_SIZE
```

`sscratch` 是一个供 S-mode 软件使用的暂存 CSR。MiniCore 约定：

```text
执行 S-mode 内核代码时：sscratch = 0
执行 U-mode 用户代码时：sscratch = 可用的内核栈指针
```

这是项目的软件约定，CPU 不会自动替我们维持它。

先认识第一条指令：

```asm
csrrw sp, sscratch, sp
```

它将旧的 `sscratch` 读入 `sp`，同时将旧的 `sp` 写入 `sscratch`。对这里来说，就是**交换二者**：

```text
执行前：sp = A，sscratch = B
执行后：sp = B，sscratch = A
```

为什么需要交换？因为内核和用户程序可能使用不同的栈。来自用户态的 trap，不能直接信任用户的栈来保存内核现场。

但我们现在只走 **S-mode 内核发生异常** 这条路径。假设异常发生前：

```text
sp       = K    ← 当前内核栈指针
sscratch = 0
```

第一条交换后：

```text
sp       = 0
sscratch = K
```

接着：

```asm
bnez sp, .Ltrap_have_stack
```

`bnez` 表示非零就跳转。现在 `sp` 为零，所以不跳，继续执行第二次交换：

```asm
csrrw sp, sscratch, sp
```

恢复为：

```text
sp       = K
sscratch = 0
```

于是，来自 S-mode 的异常会继续使用原来的内核栈。

你可能会担心：中间 `sp` 短暂变成零，会不会出问题？这几条指令没有通过 `sp` 读写内存，也没有调用函数，因此不会因为这次赋零而访问零地址。

接下来才真正预留保存区：

```asm
addi sp, sp, -TF_SIZE
```

`TF_SIZE` 是 288 字节：

```text
高地址
K         ← 异常发生前的 sp
┌──────────────────────┐
│ 288 字节 trap_frame  │
└──────────────────────┘
K - 288   ← 当前 sp
低地址
```

现在可以开始保存寄存器了：

```asm
sd zero, TF_X(0)(sp)
sd ra,   TF_X(1)(sp)
sd t0,   TF_X(5)(sp)

addi t0, sp, TF_SIZE
sd t0, TF_X(2)(sp)
```

前三条分别记录 `x0`、`ra` 和原来的 `t0`。这里即使 `x0` 永远为零，也给它保留槽位，方便数组下标和寄存器编号对应。

后两条专门保存**异常发生前的 `sp`**：

```text
当前 sp = K - 288
t0 = 当前 sp + 288 = K
将 K 保存到 x[2]，因为 sp 就是 x2
```

注意这个小心安排的顺序：先保存原来的 `t0`，才把 `t0` 当临时变量使用。

确认一下：**如果直接把减去 288 之后的当前 `sp` 保存到 `x[2]`，以后恢复时，栈指针会回到原来的 K 吗？**

## User

不会

## Assistant

对，会恢复成 `K - 288`，而不是原来的 `K`。所以必须先算出旧栈指针，再保存。

我们接着把现场保存完。普通寄存器可以直接写入内存：

```asm
sd gp, TF_X(3)(sp)
sd tp, TF_X(4)(sp)
sd t1, TF_X(6)(sp)
...
sd a0, TF_X(10)(sp)
...
sd t6, TF_X(31)(sp)
```

但 `sepc` 等 CSR 不能直接作为 `sd` 的源寄存器，要先读进通用寄存器：

```asm
csrr t0, sepc
sd t0, TF_SEPC(sp)

csrr t0, sstatus
sd t0, TF_SSTATUS(sp)

csrr t0, scause
sd t0, TF_SCAUSE(sp)

csrr t0, stval
sd t0, TF_STVAL(sp)
```

这里反复使用 `t0` 是安全的，因为它原来的值已经保存了。

保存完成后，栈上的 288 字节就对应这份结构：

```c
struct trap_frame {
    uint64_t x[32];
    uint64_t sepc;
    uint64_t sstatus;
    uint64_t scause;
    uint64_t stval;
};
```

**汇编写入的偏移，必须与 C 结构体的字段位置一致。** 比如：

```text
sepc：偏移 32 × 8 = 256 字节
```

这样 C 代码访问 `frame->sepc` 时，才能读到汇编保存的那个值。

进入 C 函数前，项目还重新设置了 `gp`：

```asm
.option push
.option norelax
la gp, __global_pointer$
.option pop
```

你之前见过它：为 C 代码准备全局数据访问的基准。被打断现场中的 `gp` 不一定是内核期望的值——例如项目的寄存器测试会故意改写它。因此，先保存旧值，再设置内核需要的值。

最后：

```asm
mv a0, sp
call trap_dispatch
```

对应的 C 声明是：

```c
void trap_dispatch(struct trap_frame *frame);
```

`a0` 传入的是**栈上这份现场的地址**。C 函数直接读取它，就能知道异常原因；必要时也可以修改它，影响之后的恢复。

例如，我们可以用下面这条示意语句理解它的效果：

```c
frame->sepc = 某个恢复地址;
```

处理函数返回后，汇编会执行：

```asm
ld t0, TF_SEPC(sp)
csrw sepc, t0
```

它从现场保存区重新取出 `sepc`，写回 CSR。因此，恢复位置并不一定是最初保存的位置，也可能是处理程序修改后的值。

这就为“处理异常后跳过某条测试指令”提供了机制。但是否跳过、跳到哪里，要由异常处理逻辑决定，不能对所有异常统一加一个固定长度。

现在我们已经从汇编入口走到了 C 函数门口：

```text
选择栈 → 保存整数寄存器 → 保存 CSR
       → 设置内核 gp → 传入现场指针 → trap_dispatch
```

下一步看 `trap_dispatch` 如何通过 `scause`，区分“定时器中断”和“某条指令触发的异常”。

## User

继续

## Assistant

接下来把 `scause` 的判读和项目中的分支对上，再选一次实际测试异常走完整条处理路径。

## Activity

    $ sed -n '1,205p' src/arch/riscv/trap.c
    #include "trap.h"
    
    #include <stddef.h>
    
    #include "csr.h"
    #include "sbi.h"
    #include "../../kernel/user.h"
    #include "../../kernel/scheduler.h"
    #include "../../platform/platform.h"
    
    #define EXCEPTION_ILLEGAL_INSTRUCTION UINT64_C(2)
    #define EXCEPTION_BREAKPOINT UINT64_C(3)
    #define EXCEPTION_LOAD_ACCESS_FAULT UINT64_C(5)
    #define EXCEPTION_LOAD_PAGE_FAULT UINT64_C(13)
    #define EXCEPTION_STORE_PAGE_FAULT UINT64_C(15)
    #define INTERRUPT_SUPERVISOR_TIMER UINT64_C(5)
    
    _Static_assert(sizeof(struct trap_frame) == 36 * sizeof(uint64_t),
                   "trap frame must match trap.S");
    _Static_assert(offsetof(struct trap_frame, sepc) == 32 * sizeof(uint64_t),
                   "sepc offset must match trap.S");
    _Static_assert(offsetof(struct trap_frame, stval) == 35 * sizeof(uint64_t),
                   "stval offset must match trap.S");
    
    extern void trap_entry(void);
    extern void trap_trigger_test_exceptions(void);
    extern void trap_register_test(void);
    extern char trap_register_window_start[], trap_register_window_end[];
    
    uint64_t trap_register_test_sp;
    
    static const uint64_t expected_exceptions[] = {
        EXCEPTION_ILLEGAL_INSTRUCTION,
        EXCEPTION_BREAKPOINT,
        EXCEPTION_LOAD_ACCESS_FAULT,
    };
    
    static volatile uint64_t timer_interval;
    static volatile uint32_t timer_ticks;
    static volatile uint32_t register_ticks;
    static volatile uint32_t register_failures;
    static size_t exception_index;
    static bool test_running;
    static volatile bool page_fault_test_running;
    static volatile bool page_fault_seen;
    static volatile uint64_t expected_page_fault;
    static volatile uintptr_t expected_fault_address;
    
    static void print_trap(const char *kind, const struct trap_frame *frame)
    {
        console_puts(kind);
        console_puts(": scause=");
        console_puthex(frame->scause);
        console_puts(" sepc=");
        console_puthex(frame->sepc);
        console_puts(" stval=");
        console_puthex(frame->stval);
        console_putc('\n');
    }
    
    static const char *exception_name(uint64_t code)
    {
        switch (code) {
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            return "illegal instruction";
        case EXCEPTION_BREAKPOINT:
            return "breakpoint";
        case EXCEPTION_LOAD_ACCESS_FAULT:
            return "load access fault";
        default:
            return "unexpected exception";
        }
    }
    
    static _Noreturn void unexpected(const char *kind,
                                     const struct trap_frame *frame)
    {
        print_trap(kind, frame);
        platform_exit(false);
    }
    
    static size_t trapped_instruction_size(uint64_t sepc)
    {
        const volatile uint16_t *instruction = (const volatile uint16_t *)sepc;
    
        return ((*instruction & 3U) == 3U) ? 4U : 2U;
    }
    
    static bool in_register_window(uint64_t sepc)
    {
        return sepc >= (uintptr_t)trap_register_window_start &&
               sepc < (uintptr_t)trap_register_window_end;
    }
    
    static void check_saved_registers(struct trap_frame *frame)
    {
        for (size_t index = 1; index < 32; ++index) {
            uint64_t expected = index == 2 ? trap_register_test_sp : index;
    
            if (frame->x[index] != expected)
                ++register_failures;
        }
    }
    
    static void handle_timer(struct trap_frame *frame)
    {
        if (scheduler_timer_active()) {
            scheduler_tick();
            return;
        }
        ++timer_ticks;
        if (test_running && in_register_window(frame->sepc)) {
            check_saved_registers(frame);
            ++register_ticks;
            if (register_ticks == 3)
                frame->sepc = (uintptr_t)trap_register_window_end;
        }
        sbi_set_timer(csr_read_time() + timer_interval);
    }
    
    void trap_dispatch(struct trap_frame *frame)
    {
        uint64_t code = frame->scause & ~SCAUSE_INTERRUPT;
    
        if ((frame->scause & SCAUSE_INTERRUPT) != 0) {
            if (code == INTERRUPT_SUPERVISOR_TIMER) {
                handle_timer(frame);
                return;
            }
            unexpected("unexpected interrupt", frame);
        }
    
        if (user_handle_trap(frame, code))
            return;
    
        if (page_fault_test_running && code == expected_page_fault &&
            frame->stval == expected_fault_address) {
            print_trap(code == EXCEPTION_LOAD_PAGE_FAULT ? "load page fault"
                                                         : "store page fault",
                       frame);
            page_fault_seen = true;
            frame->sepc += trapped_instruction_size(frame->sepc);
            return;
        }
    
        print_trap(exception_name(code), frame);
        if (test_running && exception_index <
                sizeof(expected_exceptions) / sizeof(expected_exceptions[0]) &&
            code == expected_exceptions[exception_index]) {
            ++exception_index;
            frame->sepc += trapped_instruction_size(frame->sepc);
            return;
        }
    
        unexpected("unhandled exception", frame);
    }
    
    bool trap_expect_page_fault(volatile uint64_t *address, bool write)
    {
        uint64_t ignored = 0;
    
        expected_page_fault = write ? EXCEPTION_STORE_PAGE_FAULT
                                    : EXCEPTION_LOAD_PAGE_FAULT;
        expected_fault_address = (uintptr_t)address;
        page_fault_seen = false;
        page_fault_test_running = true;
        if (write)
            *address = UINT64_C(0xfeedfacecafebeef);
        else
            ignored = *address;
        page_fault_test_running = false;
        __asm__ volatile("" : : "r"(ignored) : "memory");
        return page_fault_seen;
    }
    
    void trap_init(void)
    {
        csr_clear_sstatus(SSTATUS_SIE);
        csr_clear_sie(UINT64_MAX);
        csr_write_stvec((uintptr_t)trap_entry);
    }
    
    bool trap_run_m1_tests(uint64_t timebase_frequency)
    {
        exception_index = 0;
        timer_ticks = 0;
        register_ticks = 0;
        register_failures = 0;
        test_running = true;
    
        trap_trigger_test_exceptions();
        if (exception_index !=
            sizeof(expected_exceptions) / sizeof(expected_exceptions[0]))
            return false;
    
        timer_interval = timebase_frequency / 100;
        if (timer_interval == 0)
            timer_interval = 1;
        sbi_set_timer(csr_read_time() + timer_interval);
        csr_set_sie(SIE_STIE);
        csr_set_sstatus(SSTATUS_SIE);
        trap_register_test();
        csr_clear_sstatus(SSTATUS_SIE);
        csr_clear_sie(SIE_STIE);
        sbi_set_timer(UINT64_MAX);
    ✓ • 0ms

## Assistant

现在来到 C 函数：

```c
void trap_dispatch(struct trap_frame *frame)
```

它拿到刚才汇编保存的现场，首先回答：**这次是中断，还是同步异常？**

RV64 的 `scause` 可以这样拆：

```text
bit 63                 bits 62～0
┌─────────────────────┬──────────────────────┐
│ 1：中断，0：异常    │       原因编号       │
└─────────────────────┴──────────────────────┘
```

所以项目先取出原因编号：

```c
uint64_t code = frame->scause & ~SCAUSE_INTERRUPT;
```

`SCAUSE_INTERRUPT` 是最高位掩码；取反再按位与，相当于清除最高位，保留下面的编号。

然后分流：

```c
if ((frame->scause & SCAUSE_INTERRUPT) != 0) {
    if (code == INTERRUPT_SUPERVISOR_TIMER) {
        handle_timer(frame);
        return;
    }
    unexpected("unexpected interrupt", frame);
}
```

如果是定时器中断，就交给 `handle_timer`；其他中断在当前这条分发路径中属于未预期情况。

**必须先判断最高位，再解释编号。** 比如项目里，编号 `5` 有两种不同含义：

| `scause` | 含义 |
|---|---|
| `0x8000000000000005` | S-mode 定时器中断 |
| `0x0000000000000005` | 读取访问故障 |

我们暂时沿异常路径继续，用项目启动时故意触发的**非法指令异常**作例子。

测试汇编的第一条是：

```asm
csrw cycle, zero
```

它试图向只读的 `cycle` CSR 写入零，从而触发非法指令异常。

这时：

```text
sepc   = 这条 csrw 指令的地址
scause = 2，即非法指令异常
```

CPU 进入 `trap_entry`，保存现场，再调用我们正在看的 `trap_dispatch`。

由于最高位为零，不进入中断分支。接着代码会尝试用户态异常处理和专门的页故障测试处理；我们这次是 S-mode 的非法指令测试，不属于这两类，最终来到：

```c
print_trap(exception_name(code), frame);
```

这里先把编号转成名称，再打印 `scause`、`sepc`、`stval`，方便观察现场。

随后检查这是不是测试期待的异常：

```c
if (test_running &&
    exception_index <
        sizeof(expected_exceptions) / sizeof(expected_exceptions[0]) &&
    code == expected_exceptions[exception_index]) {

    ++exception_index;
    frame->sepc += trapped_instruction_size(frame->sepc);
    return;
}
```

测试期待的顺序是：

```text
第 0 次：非法指令
第 1 次：断点
第 2 次：读取访问故障
```

当前是第 0 次，编号匹配，于是做两件事：

```c
++exception_index;
```

记下这次测试已经通过，下一次应检查断点异常。

```c
frame->sepc += trapped_instruction_size(frame->sepc);
```

把保存的返回位置移到**出错指令之后**。

这一步很关键：这是一条故意执行的非法指令，测试完成后需要跳过它。否则返回原位置，又会执行它，再次触发同一个异常。

整个过程就是：

```text
执行故意非法的 csrw
    → trap_entry 保存现场
    → trap_dispatch 确认是预期异常
    → 将保存的 sepc 前移一条指令
    → 汇编恢复现场
    → sret 从下一条指令继续
```

这只是测试代码对预期异常的处理方式。若异常不匹配任何处理分支，项目会打印信息并报告失败，不会随意跳过。

下一步需要弄懂 `trapped_instruction_size`：**为什么这里没有直接写 `frame->sepc += 4`？** 项目启用了 RISC-V 压缩指令，一条指令可能占两字节，也可能占四字节，因此必须先判断当前指令的长度。

## User

继续

## Assistant

我们先看它怎样判断指令长度，再把异常返回走完。

实际函数是：

```c
static size_t trapped_instruction_size(uint64_t sepc)
{
    const volatile uint16_t *instruction =
        (const volatile uint16_t *)sepc;

    return ((*instruction & 3U) == 3U) ? 4U : 2U;
}
```

`sepc` 是出错指令的地址。这里把它转换为 `uint16_t *`，读取指令开头的 **两个字节**，再检查最低两位：

```c
*instruction & 3U
```

因为 `3` 的二进制是 `11`，所以结果只保留最低两位。

对于本项目使用的 16 位压缩指令和 32 位指令：

```text
最低两位不是 11 → 16 位指令 → 2 字节
最低两位是   11 → 32 位指令 → 4 字节
```

这是针对本项目指令范围的简化判定，不是支持所有更长 RISC-V 指令编码的通用解码器。

例如刚才的：

```asm
csrw cycle, zero
```

是四字节指令，所以测试处理程序执行：

```c
frame->sepc += 4;
```

假设原来：

```text
frame->sepc = 0x……1000
```

现在变为：

```text
frame->sepc = 0x……1004
```

然后 C 函数返回，回到汇编中 `call trap_dispatch` 后面的位置。**此时只是退出 C 处理函数，还没有退出 trap。**

汇编接着恢复两个 CSR：

```asm
ld t0, TF_SEPC(sp)
csrw sepc, t0

ld t0, TF_SSTATUS(sp)
csrw sstatus, t0
```

于是，刚才修改后的返回地址正式写进 CPU 的 `sepc`。

随后代码从保存的 `sstatus` 中提取 bit 8，也就是 `SPP`：

```asm
andi t0, t0, (1 << 8)
sd t0, TF_STVAL(sp)
```

这个位记录进入 trap 前的特权级：

```text
SPP = 1 → 来自 S-mode
SPP = 0 → 来自 U-mode
```

这里把提取结果临时放进原来的 `stval` 槽位，因为本次 C 异常处理已经结束，后面的返回代码不再需要那份 `stval`，可以复用它的空间。

然后逐个恢复通用寄存器：

```asm
ld ra, TF_X(1)(sp)
ld gp, TF_X(3)(sp)
...
ld a0, TF_X(10)(sp)
...
```

注意，`t0`、`t1`、`sp` 被留到最后。尤其不能提前恢复 `sp`，因为后面还要用当前 `sp` 定位整份现场。

对于我们正在追踪的 S-mode 路径，最后执行：

```asm
csrw sscratch, zero
ld t0, TF_X(5)(sp)
ld t1, TF_X(6)(sp)
ld sp, TF_X(2)(sp)
sret
```

逐条看：

```text
sscratch = 0  → 恢复“内核运行时 sscratch 为零”的约定
恢复 t0、t1  → 临时寄存器也恢复原值
恢复 sp      → 回到异常发生前的栈位置 K
sret         → 使用 sepc 和相关状态，退出 trap
```

**`sret` 使用 `sepc` 返回；普通函数的 `ret` 使用 `ra` 返回。**

所以这次测试结束后，CPU 从非法指令后面的那条指令继续执行；被打断代码的整数寄存器恢复原值，启动测试得以向下进行。

我们把两层返回分清就好：

```text
trap_dispatch 执行 ret
    → 回到 trap_entry 中的恢复代码

trap_entry 执行 sret
    → 回到原来被异常打断的代码
```

确认一个顺序问题：**如果先把 `sp` 恢复为原来的 K，再执行 `ld a0, TF_X(10)(sp)`，还能读到刚才保存的 `a0` 吗？**

## User

不能

## Assistant

对，恢复 `sp` 后，原来的偏移就指向别处了。我们接下来走定时器中断：看内核怎样预约一次中断，以及它和刚才的异常测试有什么不同。

## Activity

    $ sed -n '183,235p' src/arch/riscv/trap.c && sed -n '1,160p' src/arch/riscv/sbi.c && sed -n '1,140p' src/arch/riscv/csr.h
    bool trap_run_m1_tests(uint64_t timebase_frequency)
    {
        exception_index = 0;
        timer_ticks = 0;
        register_ticks = 0;
        register_failures = 0;
        test_running = true;
    
        trap_trigger_test_exceptions();
        if (exception_index !=
            sizeof(expected_exceptions) / sizeof(expected_exceptions[0]))
            return false;
    
        timer_interval = timebase_frequency / 100;
        if (timer_interval == 0)
            timer_interval = 1;
        sbi_set_timer(csr_read_time() + timer_interval);
        csr_set_sie(SIE_STIE);
        csr_set_sstatus(SSTATUS_SIE);
        trap_register_test();
        csr_clear_sstatus(SSTATUS_SIE);
        csr_clear_sie(SIE_STIE);
        sbi_set_timer(UINT64_MAX);
        test_running = false;
    
        console_puts("timer ticks = ");
        console_puthex(timer_ticks);
        console_puts(" register checks = ");
        console_puthex(register_ticks);
        console_putc('\n');
        return register_ticks >= 3 && register_failures == 0;
    }
    #include "sbi.h"
    
    #define SBI_EXT_BASE UINT64_C(0x10)
    #define SBI_BASE_PROBE_EXTENSION UINT64_C(3)
    #define SBI_TIME_SET_TIMER UINT64_C(0)
    
    struct sbi_return {
        long error;
        long value;
    };
    
    static struct sbi_return sbi_call(uint64_t extension, uint64_t function,
                                      uint64_t arg0, uint64_t arg1,
                                      uint64_t arg2)
    {
        register uint64_t a0 __asm__("a0") = arg0;
        register uint64_t a1 __asm__("a1") = arg1;
        register uint64_t a2 __asm__("a2") = arg2;
        register uint64_t a6 __asm__("a6") = function;
        register uint64_t a7 __asm__("a7") = extension;
    
        __asm__ volatile("ecall"
                         : "+r"(a0), "+r"(a1)
                         : "r"(a2), "r"(a6), "r"(a7)
                         : "memory");
        return (struct sbi_return){(long)a0, (long)a1};
    }
    
    bool sbi_probe_extension(uint64_t extension_id)
    {
        struct sbi_return result =
            sbi_call(SBI_EXT_BASE, SBI_BASE_PROBE_EXTENSION, extension_id, 0, 0);
    
        return result.error == 0 && result.value != 0;
    }
    
    void sbi_set_timer(uint64_t absolute_time)
    {
        (void)sbi_call(SBI_EXT_TIME, SBI_TIME_SET_TIMER, absolute_time, 0, 0);
    }
    #ifndef MINICORE_RISCV_CSR_H
    #define MINICORE_RISCV_CSR_H
    
    #include <stdint.h>
    
    #define SCAUSE_INTERRUPT (UINT64_C(1) << 63)
    
    #define SSTATUS_SIE (UINT64_C(1) << 1)
    #define SSTATUS_SPIE (UINT64_C(1) << 5)
    #define SSTATUS_SPP (UINT64_C(1) << 8)
    #define SSTATUS_SUM (UINT64_C(1) << 18)
    #define SIE_STIE (UINT64_C(1) << 5)
    
    static inline uint64_t csr_read_time(void)
    {
        uint64_t value;
    
        __asm__ volatile("rdtime %0" : "=r"(value));
        return value;
    }
    
    static inline uint64_t csr_read_sstatus(void)
    {
        uint64_t value;
    
        __asm__ volatile("csrr %0, sstatus" : "=r"(value));
        return value;
    }
    
    static inline void csr_write_stvec(uint64_t value)
    {
        __asm__ volatile("csrw stvec, %0" : : "r"(value) : "memory");
    }
    
    static inline void csr_set_sstatus(uint64_t bits)
    {
        __asm__ volatile("csrs sstatus, %0" : : "r"(bits) : "memory");
    }
    
    static inline void csr_clear_sstatus(uint64_t bits)
    {
        __asm__ volatile("csrc sstatus, %0" : : "r"(bits) : "memory");
    }
    
    static inline void csr_set_sie(uint64_t bits)
    {
        __asm__ volatile("csrs sie, %0" : : "r"(bits) : "memory");
    }
    
    static inline void csr_clear_sie(uint64_t bits)
    {
        __asm__ volatile("csrc sie, %0" : : "r"(bits) : "memory");
    }
    
    static inline uint64_t csr_read_satp(void)
    {
        uint64_t value;
    
        __asm__ volatile("csrr %0, satp" : "=r"(value));
        return value;
    }
    
    #endif
    ✓ • 0ms

## Assistant

对，恢复 `sp` 后，同样的偏移就指向别处，所以它必须留到最后。

现在进入**定时器中断测试**。异常测试是主动执行一条会出错的指令；定时器测试则是设置一个未来时刻，让 CPU 到时收到中断。

项目的设置代码是：

```c
timer_interval = timebase_frequency / 100;
if (timer_interval == 0)
    timer_interval = 1;

sbi_set_timer(csr_read_time() + timer_interval);
csr_set_sie(SIE_STIE);
csr_set_sstatus(SSTATUS_SIE);
```

我们拆成三步。

**第一步：算出等待多久。**

`timebase_frequency` 表示时间计数器每秒增加多少次，来自刚才略过的 DTB 解析。注意，它不等于 CPU 每秒执行多少条指令。

例如，假设计数频率是：

```text
timebase_frequency = 10,000,000 次/秒
```

那么：

```text
timer_interval = 10,000,000 / 100
               = 100,000 次计数
               ≈ 10 毫秒
```

代码中的保底判断，是避免整数除法得到零间隔。

**第二步：预约一个绝对时刻。**

```c
sbi_set_timer(csr_read_time() + timer_interval);
```

假设当前计数是 `500,000`：

```text
当前时间：500,000
间隔：    100,000
预约时刻：600,000
```

传给 `sbi_set_timer` 的是 **600,000 这个目标时刻**，不是“等待 100,000”的时长。

`csr_read_time` 用一条指令读取当前计数：

```c
static inline uint64_t csr_read_time(void)
{
    uint64_t value;
    __asm__ volatile("rdtime %0" : "=r"(value));
    return value;
}
```

这里 `%0` 表示输出操作数；`"=r"(value)` 告诉编译器，指令会把结果写入一个寄存器，并作为 `value` 的值。

`sbi_set_timer` 则把预约请求交给 OpenSBI。SBI 调用内部的寄存器和 `ecall`，我们下一步再拆。

**第三步：打开两个中断开关。**

```c
csr_set_sie(SIE_STIE);
csr_set_sstatus(SSTATUS_SIE);
```

对当前正在 S-mode 运行的内核来说：

```text
sie.STIE    → 允许定时器中断
sstatus.SIE → 允许 S-mode 中断
```

可以把它们理解成分开关和总开关。预约定时器与允许 CPU 响应中断，是两件不同的事。

设置完成后，测试代码进入 `trap_register_test()`。到达预约时刻、中断条件满足时，就走我们已经熟悉的入口：

```text
定时器中断
  → trap_entry 保存现场
  → trap_dispatch 识别中断编号
  → handle_timer
  → 恢复现场并返回
```

处理函数在这轮启动测试中，会再次预约：

```c
sbi_set_timer(csr_read_time() + timer_interval);
```

因此，重复触发来自**每次处理后重新设置下一次时刻**；不是第一次设置后就自动永久按周期触发。

还有一个区别先记住：普通定时器处理中，不应该像刚才的非法指令测试那样把 `sepc` 加四。被中断的程序需要从正确的恢复位置继续，随意加四会跳过本来需要执行的指令。

确认一下时间参数：**当前计数是 800，想在 50 个计数之后收到中断，应该传给 `sbi_set_timer` 的是 50，还是 850？**

## User

后者

## Assistant

对，传入的是绝对时刻 850。接下来沿 `sbi_set_timer` 往下，看看内核怎样把这个数交给 OpenSBI。

## Activity

    $ sed -n '1,125p' src/arch/riscv/sbi.c && sed -n '1,100p' src/arch/riscv/sbi.h
    #include "sbi.h"
    
    #define SBI_EXT_BASE UINT64_C(0x10)
    #define SBI_BASE_PROBE_EXTENSION UINT64_C(3)
    #define SBI_TIME_SET_TIMER UINT64_C(0)
    
    struct sbi_return {
        long error;
        long value;
    };
    
    static struct sbi_return sbi_call(uint64_t extension, uint64_t function,
                                      uint64_t arg0, uint64_t arg1,
                                      uint64_t arg2)
    {
        register uint64_t a0 __asm__("a0") = arg0;
        register uint64_t a1 __asm__("a1") = arg1;
        register uint64_t a2 __asm__("a2") = arg2;
        register uint64_t a6 __asm__("a6") = function;
        register uint64_t a7 __asm__("a7") = extension;
    
        __asm__ volatile("ecall"
                         : "+r"(a0), "+r"(a1)
                         : "r"(a2), "r"(a6), "r"(a7)
                         : "memory");
        return (struct sbi_return){(long)a0, (long)a1};
    }
    
    bool sbi_probe_extension(uint64_t extension_id)
    {
        struct sbi_return result =
            sbi_call(SBI_EXT_BASE, SBI_BASE_PROBE_EXTENSION, extension_id, 0, 0);
    
        return result.error == 0 && result.value != 0;
    }
    
    void sbi_set_timer(uint64_t absolute_time)
    {
        (void)sbi_call(SBI_EXT_TIME, SBI_TIME_SET_TIMER, absolute_time, 0, 0);
    }
    #ifndef MINICORE_RISCV_SBI_H
    #define MINICORE_RISCV_SBI_H
    
    #include <stdbool.h>
    #include <stdint.h>
    
    #define SBI_EXT_TIME UINT64_C(0x54494d45)
    
    bool sbi_probe_extension(uint64_t extension_id);
    void sbi_set_timer(uint64_t absolute_time);
    
    #endif
    ✓ • 0ms

## Activity

    Searched the web for site.github.com/riscv-non-isa/riscv-sbi-doc binary calling convention a7 a6 a0 a1 set_timer absolute time

## Assistant

对，是 `850`。

接下来看看这次预约如何到达 OpenSBI。项目中的函数是：

```c
void sbi_set_timer(uint64_t absolute_time)
{
    (void)sbi_call(SBI_EXT_TIME, SBI_TIME_SET_TIMER,
                   absolute_time, 0, 0);
}
```

这次调用传递三类信息：

```text
选择哪类服务：TIME 定时器扩展
选择哪个操作：设置定时器
操作参数：    目标时刻 850
```

SBI 是内核与固件之间的接口约定，OpenSBI 是提供这些服务的实现。它规定用哪些寄存器传递服务编号、参数和结果。[SBI 调用约定](https://docs.riscv.org/reference/sbi/v3.0/binary-encoding.html)

对本次调用，执行 `ecall` 前要准备：

| 寄存器 | 放入的值 | 含义 |
|---|---|---|
| `a7` | `0x54494d45` | TIME 扩展编号 |
| `a6` | `0` | 设置定时器的功能编号 |
| `a0` | `850` | 绝对目标时刻 |
| `a1`、`a2` | `0` | 此次不使用的参数 |

`TIME` 的扩展编号和操作定义来自 [SBI 定时器规范](https://docs.riscv.org/reference/sbi/ext-time.html)。

项目如何保证数值放进指定寄存器？看 `sbi_call` 中的声明：

```c
register uint64_t a0 __asm__("a0") = arg0;
register uint64_t a1 __asm__("a1") = arg1;
register uint64_t a2 __asm__("a2") = arg2;
register uint64_t a6 __asm__("a6") = function;
register uint64_t a7 __asm__("a7") = extension;
```

这里不只是把 C 变量取名为 `a0`。`__asm__("a0")` 配合后面的汇编操作数，明确要求使用硬件寄存器 `a0`。

准备完成后执行：

```c
__asm__ volatile("ecall"
                 : "+r"(a0), "+r"(a1)
                 : "r"(a2), "r"(a6), "r"(a7)
                 : "memory");
```

先抓住核心：**真正发出请求的是 `ecall` 指令。**

在本项目的启动环境中，S-mode 内核执行它，会陷入 M-mode 的 OpenSBI。OpenSBI 读取这些寄存器，设置定时器，然后返回内核继续执行。

```text
MiniCore，S-mode
    │ 准备参数，执行 ecall
    ▼
OpenSBI，M-mode
    │ 设置定时器，返回结果
    ▼
MiniCore，继续执行 ecall 后面的代码
```

这次进入的是固件的处理入口，不是我们刚读过的 S-mode `trap_entry`。

再看汇编周围的约束：

- `"+r"(a0)`、`"+r"(a1)`：既是输入，也是输出，调用后值可能改变。
- `"r"(a2)` 等：提供输入参数。
- `"memory"`：告诉编译器这次操作可能影响内存，约束相关优化；它本身不是硬件内存屏障。
- `volatile`：要求编译器保留这次有副作用的汇编操作。

返回后，SBI 约定 `a0` 保存错误码，`a1` 保存返回值。项目把它们包装成结构体：

```c
return (struct sbi_return){(long)a0, (long)a1};
```

不过，`sbi_set_timer` 前面的 `(void)` 表示它主动忽略这份返回结果。因此这个封装没有检查本次调用的错误码。

最后区分两个时间点：**`ecall` 是现在请求设置定时器；定时器中断是以后到时再发生。** 设置调用返回后，内核可以继续工作。

所以，预约时刻为 850 时，`sbi_set_timer(850)` 会一直等到 850 才返回吗？

## User

把今天这次对话总结一下，删除没用的内容，保留要点，输出为doc下的一个md文件

## Assistant

我会把这次学习整理成一份精简笔记，保留代码主线、关键概念和易错点，去掉重复问答，并写入项目的 `doc` 目录。

## Activity

    $ git status --short && ls -ld doc docs .agents .codex && rg --files -g AGENTS.md -g '*2026-09-26*' -g '*summary*'
     M Makefile
     M README.md
     M docs/linux-comparison.md
     M docs/syscall-abi.md
     M src/arch/riscv/csr.h
     M src/arch/riscv/trap.c
     M src/kernel/user.c
     M src/main.c
     M user/user.S
    ?? docs/m4.md
    ?? src/arch/riscv/context.S
    ?? src/kernel/list.h
    ?? src/kernel/log.c
    ?? src/kernel/log.h
    ?? src/kernel/m4_test.c
    ?? src/kernel/m4_test.h
    ?? src/kernel/refcount.h
    ?? src/kernel/scheduler.c
    ?? src/kernel/scheduler.h
    ?? src/kernel/sync.c
    ?? src/kernel/sync.h
    ?? src/mm/heap.c
    ?? src/mm/heap.h
    ls: cannot access 'doc': No such file or directory
    dr-xr-xr-x 2 celestee celestee   40 Sep 26 03:39 .agents
    dr-xr-xr-x 2 celestee celestee   40 Sep 26 03:39 .codex
    drwxrwxr-x 4 celestee celestee 4096 Sep 24 18:34 docs
    ✗ (2) • 0ms

## Activity

    $ rg --files --hidden -g AGENTS.md -g '!build/**' -g '!.git/**' . /home/celestee/prj 2>/dev/null
    ✗ (1) • 0ms

## Activity

    file changes: Completed · 1 changes
    Add: /home/celestee/prj/minicore/doc/learning-summary-2026-09-26.md
    # MiniCore 学习笔记：启动、内存映射与异常中断
    
    日期：2026-09-26。整理本次教学对话，删除重复问答，保留执行主线、代码含义和易错点。DTB 解析按学习安排暂时跳过，仅保留必要概念。
    
    ## 1. 项目与学习主线
    
    MiniCore 是运行在 QEMU `virt` 上的 RV64 S-mode 内核，通过 OpenSBI 启动。当前代码覆盖启动、异常与中断、内存管理、用户态、线程调度等 M0～M4 内容；路线图中的后续阶段不代表已经实现。
    
    学习目标是能解释函数的职责、调用场景、输入输出、状态变化、资源归属、失败路径及验证方式。阅读顺序以真实执行过程为主：
    
    ```text
    OpenSBI
      → _start：保存参数，建立启动页表，开启 Sv39
      → 跳入高地址入口：初始化 gp、sp，清零 BSS
      → kernel_main：输出、自检、发现硬件
      → trap 与定时器测试
      → 物理页、正式页表、堆和调度器
      → 用户态与线程测试
      → M4 PASS，退出 QEMU
    ```
    
    主要入口：[head.S](../src/arch/riscv/head.S)、[main.c](../src/main.c)。
    
    ## 2. 函数调用、寄存器与栈
    
    - RISC-V 优先用 `a0～a7` 传递整数和指针参数，简单返回值通常放在 `a0`。
    - 调用指令保存返回地址到 `ra`；`ret` 根据 `ra` 返回，不会自动从栈取回旧的 `ra`。
    - 嵌套调用会改写 `ra`。函数需要先保存自己原来的返回地址，通常保存在栈上。
    - 简单叶子函数可以不使用栈；普通 C 代码运行前仍必须准备可用栈。
    - 栈就是内存。栈向低地址增长：减小 `sp` 预留空间，恢复时增大 `sp`。
    
    `jal` 使用相对当前 PC 的偏移，范围约为 ±1 MiB；`jr t0` 使用寄存器中的目标地址，不受这个相对距离限制。64 位 CPU 不意味着每条指令都能直接编码完整的 64 位地址。
    
    ## 3. Sv39：虚拟地址、PTE 与物理地址
    
    ### 地址与表项格式
    
    ```text
    64 位虚拟地址：
    [63:39 复制 bit38][38:30 VPN2][29:21 VPN1][20:12 VPN0][11:0 偏移]
    
    64 位 PTE：
    [63:54 扩展/保留][53:28 PPN2][27:19 PPN1][18:10 PPN0]
    [9:8 RSW][7 D][6 A][5 G][4 U][3 X][2 W][1 R][0 V]
    ```
    
    - VPN 用于选择页表项；PPN 指明下一级页表或映射目标的物理位置。
    - `V` 表示有效；`R/W/X` 表示读、写、执行权限；`U` 表示用户态访问属性；`A/D` 是访问、脏标志；`G` 表示全局映射；`RSW` 留给软件。
    - 合法有效表项中，`R/W/X` 全零表示非叶子；`R` 或 `X` 为一表示叶子。`R=0,W=1` 是非法编码。
    - PPN 共 44 位，以 4 KiB 为单位：`物理页号 = 物理地址 >> 12`。
    - PPN 加 12 位偏移可编码最多 56 位物理地址，实际机器可以支持更少。
    - 字段宽度和数值大小不同：44 位字段可以保存 `0x80000`，省略的高位全是零。
    
    ### 大页与普通页
    
    | 叶子层级 | 页大小 | 虚拟地址中用作偏移的位 | 物理基址对齐 |
    |---|---|---|---|
    | L2 | 1 GiB | 低 30 位 | 1 GiB |
    | L1 | 2 MiB | 低 21 位 | 2 MiB |
    | L0 | 4 KiB | 低 12 位 | 4 KiB |
    
    PPN 分段便于表达大页地址拼接。L2 叶子的 `PPN1/PPN0` 必须为零；L1 叶子的 `PPN0` 必须为零。大页中，相应的低位来自虚拟地址，而不是继续查下一级表。
    
    以 `VA=0x80200123` 为例：`VPN2=2, VPN1=1, VPN0=0`，低 12 位为 `0x123`。
    
    ```text
    1 GiB 映射：
    satp → root[2]，遇到叶子
    物理基址 0x80000000 + 低30位偏移 0x200123 = PA 0x80200123
    
    4 KiB 映射（教学示例）：
    satp → L2[2] → L1[1] → L0[0]，遇到叶子
    若数据页基址为 0x82005000，则 PA = 0x82005000 + 0x123
    ```
    
    硬件查表使用物理地址；TLB 命中时可以省去页表遍历。
    
    ## 4. 启动页表与高地址入口
    
    OpenSBI 传入 `a0=hart ID`、`a1=DTB 物理地址`，启动汇编先保存到 `s0/s1`。
    
    启动根页表占 4 KiB，共 512 项，每项 8 字节。先清零，再填写：
    
    - `root[0～3]`：前 4 GiB 的恒等映射。
    - `root[256～259]`：同一物理范围映射到从 `0xffffffc000000000` 开始的高地址区域。
    
    这些都是 L2 的 1 GiB 叶子，启动权限标志为 `0xcf`，即 `V/R/W/X/A/D`。例如：
    
    ```text
    root[2] = (0x80000000 >> 2) | 0xcf = 0x200000cf
    ```
    
    右移两位来自 `(PA >> 12) << 10`：页号在 PTE 中从 bit 10 开始存放。表项下标决定虚拟范围，表项中的 PPN 决定物理范围。
    
    `root[256]` 偏移为 2048，超过 `sd` 有符号立即数的正向上限 2047，因此代码先把基址前移 2040，再用小偏移访问。
    
    ### 启用翻译
    
    `satp` 保存 `MODE=8`（Sv39）、`ASID=0` 和根页表物理页号。写入后执行：
    
    ```asm
    csrw satp, t0
    sfence.vma zero, zero
    ```
    
    `sfence.vma` 同步之前的页表写入与后续硬件查表，并使相关旧地址翻译缓存失效。两个 `zero` 表示本 hart 的所有虚拟地址、所有地址空间，包括全局映射；不是仅处理地址零。它不会清空页表内存，也不会自动同步其他 hart。
    
    写 `satp` 不会自动把 PC 改成高地址。恒等映射保证切换后的低地址取指仍然有效，之后显式跳转：
    
    ```asm
    la t0, .Lhigh_entry_address  # 存放目标地址的位置
    ld t0, 0(t0)                # 读取完整高地址
    jr t0                      # 跳到目标代码
    ```
    
    `.dword .Lhigh_entry` 存放链接器确定的地址。代码没有搬家，改变的是访问它的虚拟地址。
    
    ### 准备 C 环境
    
    - 初始化 `gp`，为全局数据访问准备基准；临时使用 `.option norelax`，避免初始化本身被优化成依赖尚未初始化的 `gp`。
    - `sp` 指向链接脚本预留的 12 KiB 启动栈顶。下方另留 4 KiB guard 区，正式页表建立后才通过不映射实现保护。
    - 清零 BSS，满足未显式初始化的静态存储期变量初始为零的要求。启动栈也在这段区域中，此时尚未使用它保存数据。
    - 恢复 `a0=hart ID`；将 DTB 物理地址加 direct-map 基址形成高虚拟地址，放入 `a1`。
    - 调用 `kernel_main`；若意外返回，进入 panic。
    
    相关代码：[linker.ld](../src/linker/linker.ld)。
    
    ## 5. 输出、基础运行库与失败退出
    
    相关代码：[qemu-virt.c](../src/platform/qemu-virt.c)、[runtime.c](../src/runtime.c)。
    
    ### 串口输出
    
    `console_puts` 遍历字符串直到 `\0`，逐字符调用 `console_putc`。
    
    `console_putc` 访问写死的 UART 物理地址 `0x10000000` 对应的高虚拟地址：
    
    1. 遇到 `\n`，先输出 `\r`。
    2. 轮询 `UART0[5]` 的 bit 5，等待发送保持寄存器空。
    3. 向 `UART0[0]` 写入字符。
    
    这是 MMIO：读写设备地址触发设备行为。`volatile uint8_t *` 保留逐字节设备访问，不能把它等同于锁或硬件内存屏障。
    
    bit 5（THRE）表示可以提交下一个字符；bit 6（TEMT）才表示发送保持部分和移位寄存器都空。函数返回不保证字符已经在线路上发送完毕。不检查可写状态就持续写入，可能丢数据。
    
    在解析 DTB 前能输出，是因为 UART 地址已写死、启动映射已建立，且启动环境允许该物理访问。
    
    ### 数字与内存函数
    
    - `console_puthex` 从 bit 60 开始，每次提取四位，用 `"0123456789abcdef"` 转成字符，固定打印 16 位十六进制数及 `0x` 前缀。
    - 从低四位开始输出叫数字倒序，不叫小端序；端序讨论的是多字节数在内存中的字节排列。
    - `memset` 按字节填充值；用它将一个 32 位整数的四字节都填为 1，结果是 `0x01010101`。
    - `memcpy` 按指定字节数复制，要求源目标不重叠，遇到 `\0` 不停止。
    - `memmove` 允许重叠。目标低于源时向前复制，目标高于源时向后复制，避免覆盖尚未读取的源数据。
    - `strlen` 在第一个 `\0` 停止，不计结束符。`{'A','\0','B','\0'}` 的字符串长度为 1，但复制四字节会复制整个数组。
    
    启动自检将 `test` 从全零变成 `{'M','0','\0','\0'}`，再右移前三字节，得到 `{'M','M','0','\0'}`；检查 `strlen(test+1)==2`，并检查 DTB 非空、BSS 测试变量为零。
    
    ### 失败路径
    
    `panic` 打印原因并调用 `platform_exit(false)`。后者向 QEMU 测试退出设备写入 `0x3333` 表示失败，`0x5555` 表示成功，之后以无限 `wfi` 循环兜底。
    
    `_Noreturn` 是“不正常返回”的声明，不会自动实现停机。
    
    ## 6. DTB：仅保留必要知识
    
    DTB 的内部解析暂不继续展开。当前只需要知道：`dtb_parse` 将硬件描述整理为 `boot_info`，包括 RAM、保留区域、UART、PLIC、VirtIO 和时间计数频率。
    
    - DTS 是可读文本，DTB 是二进制形式。
    - DTB 中多字节整数按大端存储；直接通过整数指针读取使用 CPU 数据端序，不会自动识别格式。
    - `read_be32` 逐字节移位组合，因此 `00 00 01 00` 得到 256；4 是字节数，不是数值。
    - 父节点的 `#address-cells/#size-cells` 决定子节点 `reg` 的地址与大小各占几个四字节单元。
    - 属性名称放在字符串块；属性值和节点名称在结构块。
    - 解析器的节点数组保存当前祖先路径，兄弟节点先后复用槽位，限制的是深度而非总节点数。
    - 保留内存表以地址和大小同时为零结束；地址零、大小非零仍是有效记录。
    - `range_within(total, offset, length)` 使用 `offset <= total && length <= total-offset` 避免加法溢出。非空指针及内部边界验证，不等于能安全访问任意输入地址。
    
    ## 7. Trap：从异常入口到恢复执行
    
    相关代码：[trap.S](../src/arch/riscv/trap.S)、[trap.c](../src/arch/riscv/trap.c)、[trap.h](../src/arch/riscv/trap.h)。目前重点是 **S-mode 内核被打断**，用户态路径尚未展开。
    
    ### 初始化与硬件状态
    
    `trap_init` 关闭 `sstatus.SIE` 及 `sie` 的中断使能，并将 `trap_entry` 写入 `stvec`。入口四字节对齐，使用 Direct 模式。关闭中断不禁止同步异常。
    
    | CSR | 用途 |
    |---|---|
    | `stvec` | trap 入口 |
    | `sepc` | 恢复执行所需的位置 |
    | `scause` | 中断/异常类别与原因编号 |
    | `stval` | 出错地址等附加信息，具体含义取决于异常 |
    | `sstatus` | 特权级、中断状态等 |
    
    CPU 不会自动将所有整数寄存器压栈。入口汇编必须先保存现场，才能调用 C；否则 `call` 会覆盖原来的 `ra`，准备参数会覆盖原来的 `a0`。
    
    `sepc` 和 `ra` 不可混淆：前者让 trap 返回被打断的函数，后者让该函数以后返回自己的调用者。
    
    ### 选栈与保存
    
    项目约定 S-mode 运行时 `sscratch=0`，U-mode 运行时存放内核栈指针。入口：
    
    ```asm
    csrrw sp, sscratch, sp
    bnez sp, .Ltrap_have_stack
    csrrw sp, sscratch, sp
    ```
    
    对 S-mode 路径，第一次交换将 `sp` 置零，分支不跳，再交换恢复原栈。期间没有通过零值 `sp` 访问内存。
    
    随后 `sp -= 288`，保存：
    
    ```c
    struct trap_frame {
        uint64_t x[32];
        uint64_t sepc, sstatus, scause, stval;
    };
    ```
    
    汇编偏移必须与 C 布局一致。先保存原来的 `t0`，再用它计算旧 `sp = 当前 sp + 288`，存入 `x[2]`。CSR 先读入临时寄存器，再写入现场。
    
    保存完成后重新设置内核 `gp`，以 `a0=sp` 调用 `trap_dispatch`。
    
    ### 分发与测试异常
    
    RV64 的 `scause` 最高位表示是否为中断，其余位表示原因编号。必须先判断类别：编号 5 可表示定时器中断，也可表示读取访问故障。
    
    `trap_dispatch` 依次处理：中断、用户态异常、预期页故障测试、启动异常测试；未处理情况打印现场并报告失败。
    
    启动异常测试依次触发非法指令、断点、读取访问故障。例如 `csrw cycle, zero` 试图写只读 CSR，触发非法指令异常。确认符合测试预期后增加测试计数，并推进保存的 `sepc`，跳过故意出错的指令。
    
    `trapped_instruction_size` 读取指令开头两字节，根据最低两位区分本项目中的 2/4 字节指令；它不是支持任意长编码的通用解码器。跳过指令是特定测试策略，不能用于所有异常。
    
    ### 恢复与返回
    
    1. C 函数返回汇编入口。
    2. 从现场写回 `sepc/sstatus`，因此 C 对保存现场的修改会影响恢复。
    3. 根据保存的 `SPP` 判断返回 S-mode 还是 U-mode；代码临时复用 `stval` 槽位保存该判断。
    4. 恢复通用寄存器；S-mode 路径清零 `sscratch`，最后恢复 `t0/t1/sp`。
    5. 执行 `sret`，从 `sepc` 指定的位置恢复。
    
    必须最后恢复 `sp`，否则后续按原偏移加载寄存器会读错位置。`ret` 返回普通调用者；`sret` 返回被 trap 打断的执行环境。
    
    ## 8. 定时器与 SBI 调用
    
    相关代码：[sbi.c](../src/arch/riscv/sbi.c)、[csr.h](../src/arch/riscv/csr.h)。
    
    ```c
    timer_interval = timebase_frequency / 100;
    if (timer_interval == 0)
        timer_interval = 1;
    sbi_set_timer(csr_read_time() + timer_interval);
    csr_set_sie(SIE_STIE);
    csr_set_sstatus(SSTATUS_SIE);
    ```
    
    - `timebase_frequency` 是时间计数器频率，不是指令执行速度；除以 100 得到约 10 ms 的计数间隔。
    - `rdtime` 读取当前计数。设置定时器传绝对时刻：当前 800，等待 50，应传 850。
    - 对 S-mode 内核，需要允许定时器中断 `sie.STIE`，并打开总开关 `sstatus.SIE`。
    - 定时器中断进入同一 trap 入口，由 `handle_timer` 处理。启动测试中每次重新预约下次时间，不是自动永久周期触发。
    - 普通定时器处理不应随意推进 `sepc`；项目寄存器测试有专门的跳转退出逻辑，尚待展开。
    
    ### 请求如何交给 OpenSBI
    
    执行 `sbi_set_timer(850)` 时，`sbi_call` 准备：
    
    | 寄存器 | 内容 |
    |---|---|
    | `a7` | TIME 扩展编号 `0x54494d45` |
    | `a6` | 设置定时器功能编号 0 |
    | `a0` | 目标时刻 850 |
    | `a1/a2` | 本次未使用，置零 |
    
    随后执行 `ecall`。在本项目环境中，S-mode 请求进入 M-mode OpenSBI，完成设置后返回；不是进入内核自己的 S-mode `trap_entry`。
    
    调用约定用 `a0` 返回错误码、`a1` 返回结果。内联汇编的 `+r` 表示读写操作数，`r` 表示输入；`memory` 约束编译器的内存优化，不是硬件屏障。`sbi_set_timer` 当前通过 `(void)` 忽略返回结果。
    
    **设置定时器不是睡眠：请求设置完成后即返回，不会等到目标时刻。中断在到时后另行发生。**
    
    ## 9. 下次从哪里继续
    
    继续阅读定时器寄存器测试：
    
    1. `trap_register_test` 怎样给寄存器填入已知值。
    2. `handle_timer`、`check_saved_registers` 怎样检查现场。
    3. 第三次检查后为何修改 `sepc`，使测试离开等待循环。
    4. 测试结束后怎样关闭中断并检查结果。
    
    DTB 内部解析暂时跳过；用户态 trap、物理页分配、正式页表、堆、线程调度仍待学习。
    
    ## 参考资料
    
    - [RISC-V 特权架构：Sv39、CSR、trap 与 SFENCE.VMA](https://docs.riscv.org/reference/isa/priv/supervisor.html)
    - [RISC-V 基础指令：JAL/JALR](https://docs.riscv.org/reference/isa/v20240411/unpriv/rv32.html)
    - [SBI 调用约定](https://docs.riscv.org/reference/sbi/v3.0/binary-encoding.html)
    - [SBI TIME 扩展](https://docs.riscv.org/reference/sbi/ext-time.html)
    - [设备树基础](https://devicetree-specification.readthedocs.io/en/stable/devicetree-basics.html)
    - [DTB 二进制格式](https://devicetree-specification.readthedocs.io/en/stable/flattened-format.html)

## Activity

    $ wc -l doc/learning-summary-2026-09-26.md && git diff --check && git status --short doc/learning-summary-2026-09-26.md
    297 doc/learning-summary-2026-09-26.md
    ?? doc/learning-summary-2026-09-26.md
    ✓ • 0ms

## Assistant

已整理到 [doc/learning-summary-2026-09-26.md](/home/celestee/prj/minicore/doc/learning-summary-2026-09-26.md)。

保留了启动、Sv39、基础函数、trap 和 SBI 定时器的要点与易错点；压缩了 DTB 内容，去掉重复问答，并标记了下次学习的起点。

## User

接你的问题，不会等到850再返回，只是设置下次中断的计数次数，会直接返回，计数到达850时会出发timer中断

## Assistant

对，只需把“计数次数”说得更准确些：850 是目标计数值，设置完成就返回；到时中断变为待处理，在中断使能等条件满足时进入处理程序。接下来看看测试怎样利用它检查寄存器是否保存正确。

## Activity

    $ sed -n '140,250p' src/arch/riscv/trap.S && sed -n '89,122p' src/arch/riscv/trap.c && sed -n '183,224p' src/arch/riscv/trap.c
        .type trap_register_test, @function
    trap_register_test:
        addi sp, sp, -128
        sd ra, 0(sp)
        sd gp, 8(sp)
        sd tp, 16(sp)
        sd s0, 24(sp)
        sd s1, 32(sp)
        sd s2, 40(sp)
        sd s3, 48(sp)
        sd s4, 56(sp)
        sd s5, 64(sp)
        sd s6, 72(sp)
        sd s7, 80(sp)
        sd s8, 88(sp)
        sd s9, 96(sp)
        sd s10, 104(sp)
        sd s11, 112(sp)
        .option push
        .option norelax
        la t0, trap_register_test_sp
        .option pop
        sd sp, 0(t0)
    
        li ra, 1
        li gp, 3
        li tp, 4
        li t0, 5
        li t1, 6
        li t2, 7
        li s0, 8
        li s1, 9
        li a0, 10
        li a1, 11
        li a2, 12
        li a3, 13
        li a4, 14
        li a5, 15
        li a6, 16
        li a7, 17
        li s2, 18
        li s3, 19
        li s4, 20
        li s5, 21
        li s6, 22
        li s7, 23
        li s8, 24
        li s9, 25
        li s10, 26
        li s11, 27
        li t3, 28
        li t4, 29
        li t5, 30
        li t6, 31
        .globl trap_register_window_start
    trap_register_window_start:
        wfi
        j trap_register_window_start
    
        .globl trap_register_window_end
    trap_register_window_end:
        .option push
        .option norelax
        la t0, trap_register_test_sp
        .option pop
        ld sp, 0(t0)
        ld ra, 0(sp)
        ld gp, 8(sp)
        ld tp, 16(sp)
        ld s0, 24(sp)
        ld s1, 32(sp)
        ld s2, 40(sp)
        ld s3, 48(sp)
        ld s4, 56(sp)
        ld s5, 64(sp)
        ld s6, 72(sp)
        ld s7, 80(sp)
        ld s8, 88(sp)
        ld s9, 96(sp)
        ld s10, 104(sp)
        ld s11, 112(sp)
        addi sp, sp, 128
        ret
        .size trap_register_test, . - trap_register_test
    
        .align 2
        .globl user_enter
        .type user_enter, @function
    user_enter:
        addi sp, sp, -128
        sd ra, 0(sp)
        sd gp, 8(sp)
        sd tp, 16(sp)
        sd s0, 24(sp)
        sd s1, 32(sp)
        sd s2, 40(sp)
        sd s3, 48(sp)
        sd s4, 56(sp)
        sd s5, 64(sp)
        sd s6, 72(sp)
        sd s7, 80(sp)
        sd s8, 88(sp)
        sd s9, 96(sp)
        sd s10, 104(sp)
        sd s11, 112(sp)
        .option push
        .option norelax
        la t0, user_kernel_sp
        .option pop
        sd sp, 0(t0)
    
    static bool in_register_window(uint64_t sepc)
    {
        return sepc >= (uintptr_t)trap_register_window_start &&
               sepc < (uintptr_t)trap_register_window_end;
    }
    
    static void check_saved_registers(struct trap_frame *frame)
    {
        for (size_t index = 1; index < 32; ++index) {
            uint64_t expected = index == 2 ? trap_register_test_sp : index;
    
            if (frame->x[index] != expected)
                ++register_failures;
        }
    }
    
    static void handle_timer(struct trap_frame *frame)
    {
        if (scheduler_timer_active()) {
            scheduler_tick();
            return;
        }
        ++timer_ticks;
        if (test_running && in_register_window(frame->sepc)) {
            check_saved_registers(frame);
            ++register_ticks;
            if (register_ticks == 3)
                frame->sepc = (uintptr_t)trap_register_window_end;
        }
        sbi_set_timer(csr_read_time() + timer_interval);
    }
    
    void trap_dispatch(struct trap_frame *frame)
    {
    bool trap_run_m1_tests(uint64_t timebase_frequency)
    {
        exception_index = 0;
        timer_ticks = 0;
        register_ticks = 0;
        register_failures = 0;
        test_running = true;
    
        trap_trigger_test_exceptions();
        if (exception_index !=
            sizeof(expected_exceptions) / sizeof(expected_exceptions[0]))
            return false;
    
        timer_interval = timebase_frequency / 100;
        if (timer_interval == 0)
            timer_interval = 1;
        sbi_set_timer(csr_read_time() + timer_interval);
        csr_set_sie(SIE_STIE);
        csr_set_sstatus(SSTATUS_SIE);
        trap_register_test();
        csr_clear_sstatus(SSTATUS_SIE);
        csr_clear_sie(SIE_STIE);
        sbi_set_timer(UINT64_MAX);
        test_running = false;
    
        console_puts("timer ticks = ");
        console_puthex(timer_ticks);
        console_puts(" register checks = ");
        console_puthex(register_ticks);
        console_putc('\n');
        return register_ticks >= 3 && register_failures == 0;
    }
    ✓ • 0ms

## Assistant

对，你理解正确。精确一点说：**850 是绝对目标计数值，不是再计数 850 次。** 设置调用返回后，内核继续执行；到时中断变为待处理，中断使能等条件满足时，CPU 才进入处理程序。

接下来，项目要验证一个问题：**经历定时器中断后，寄存器里的值还在不在？**

测试方法很直接：先给寄存器填入已知值，等待中断，再检查保存的现场。

调用的是：

```c
trap_register_test();
```

它首先在栈上保存自己的返回地址和需要保留的寄存器：

```asm
addi sp, sp, -128
sd ra, 0(sp)
sd gp, 8(sp)
sd tp, 16(sp)
sd s0, 24(sp)
...
```

这里的 128 字节是**测试函数自己的保存区**，与进入 trap 后额外预留的 288 字节现场区不是一回事。

然后，它故意把寄存器改成容易核对的值：

```asm
li ra, 1       # ra 是 x1
li gp, 3       # gp 是 x3
li tp, 4       # tp 是 x4
li t0, 5       # t0 是 x5
...
li a0, 10      # a0 是 x10
...
li t6, 31      # t6 是 x31
```

规律是：**让寄存器的值等于它的编号。**

但 `sp`，也就是 `x2`，不能设成 `2`——中断入口还需要这个栈。因此代码保留可用的 `sp`，并把它记录到全局变量 `trap_register_test_sp`，作为检查时的预期值。

准备好以后，进入等待循环：

```asm
trap_register_window_start:
    wfi
    j trap_register_window_start

trap_register_window_end:
    # 后面是恢复测试函数现场的代码
```

这个循环不会改写刚才填好的寄存器值。定时器中断到来时，`trap_entry` 会把它们保存到 `trap_frame`。

处理函数通过下面的代码检查：

```c
static void check_saved_registers(struct trap_frame *frame)
{
    for (size_t index = 1; index < 32; ++index) {
        uint64_t expected =
            index == 2 ? trap_register_test_sp : index;

        if (frame->x[index] != expected)
            ++register_failures;
    }
}
```

例如：

```text
frame->x[1] 应该是 1
frame->x[2] 应该是记录好的栈指针
frame->x[3] 应该是 3
……
frame->x[31] 应该是 31
```

`x0` 恒为零，因此循环从 `1` 开始。

不过，中断可能在准备寄存器的过程中就到达。那时还没全部填好，不能检查。所以项目先判断保存的 `sepc` 是否位于等待循环：

```c
return sepc >= (uintptr_t)trap_register_window_start &&
       sepc <  (uintptr_t)trap_register_window_end;
```

只有确实在这个范围内，才执行寄存器检查。

现在你可以看到测试的用意了：

```text
填入已知值
  → 进入等待循环
  → 定时器中断
  → 保存现场
  → 检查保存值
  → 恢复现场，继续等待
```

连续几次中断还能帮助检查恢复过程：如果上一次恢复错了，下一次保存的值就可能不再符合预期。

先留一个关键问题：**假如只检查一次，确认保存到 `trap_frame` 的值都正确，能不能据此断定“从 `trap_frame` 恢复寄存器”的代码也一定正确？**

## User

不能，因为这里的检查是从S mode到S mode的，没有U mode到Smode的

## Assistant

你指出了一个真实的覆盖缺口：**这个测试只覆盖 S-mode 被打断，不能证明 U-mode 进入内核时的切栈和恢复也正确。**

不过，我刚才的问题还有一个更直接的原因：**第一次检查发生时，第一次恢复还没执行。**

看时间顺序：

```text
寄存器填入预期值
    ↓
第一次中断：保存寄存器
    ↓
检查 trap_frame：全部正确 ✓
    ↓
恢复寄存器 ← 这里仍可能有 bug
    ↓
回到等待循环
```

比如恢复代码漏掉了 `a0`：

```asm
# 本应执行，却漏写了：
ld a0, TF_X(10)(sp)
```

第一次检查仍然可以通过，因为 `frame->x[10]` 确实保存了 `10`。但恢复后，`a0` 可能保留了处理程序使用过的值。

等第二次中断：

```text
把错误的 a0 保存到新 trap_frame
    ↓
检查 frame->x[10] 是否等于 10
    ↓
发现错误
```

所以，多次中断有助于串起来检查**上一次恢复 → 下一次保存**。但它仍不是完整证明，例如最后一次恢复之后没有下一轮检查。

我们这里要区分两个测试维度：

| 维度 | 要验证什么 |
|---|---|
| 保存与恢复 | 写入现场正确，不代表读回寄存器也正确 |
| S-mode 与 U-mode | 内核态路径正确，不代表用户态切栈路径也正确 |

你说的是第二个，我刚才想强调的是第一个。两者都需要考虑。

接下来，项目在第三次有效检查后这样退出等待循环：

```c
if (register_ticks == 3)
    frame->sepc = (uintptr_t)trap_register_window_end;
```

它没有修改循环里的指令，而是修改**这次中断的返回地址**。汇编恢复 `sepc` 后执行 `sret`，CPU 就直接来到循环后面的清理代码。

清理代码恢复测试函数一开始保存的 `ra`、`gp`、`s0` 等，归还 128 字节栈空间，再执行 `ret`，回到 `trap_run_m1_tests`。

因此这里有两层恢复：

```text
trap 返回：恢复测试中人为设置的寄存器值
    ↓
测试函数退出：恢复调用测试之前的寄存器值
```

尤其是 `ra`：测试中它被故意设成了 `1`，只有恢复最初保存的真实返回地址，最后的 `ret` 才能正确返回。

## User

继续

## Assistant

我们先把定时器测试的收尾讲完，再接回 `kernel_main`，进入物理页分配。这样能看清内核何时开始管理可用 RAM。

## Activity

    $ sed -n '195,228p' src/arch/riscv/trap.c && sed -n '150,215p' src/main.c && sed -n '1,150p' src/mm/page_alloc.c
    
        timer_interval = timebase_frequency / 100;
        if (timer_interval == 0)
            timer_interval = 1;
        sbi_set_timer(csr_read_time() + timer_interval);
        csr_set_sie(SIE_STIE);
        csr_set_sstatus(SSTATUS_SIE);
        trap_register_test();
        csr_clear_sstatus(SSTATUS_SIE);
        csr_clear_sie(SIE_STIE);
        sbi_set_timer(UINT64_MAX);
        test_running = false;
    
        console_puts("timer ticks = ");
        console_puthex(timer_ticks);
        console_puts(" register checks = ");
        console_puthex(register_ticks);
        console_putc('\n');
        return register_ticks >= 3 && register_failures == 0;
    }
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
    #include "page_alloc.h"
    
    #include "layout.h"
    
    struct free_page {
        struct free_page *next;
    };
    
    static struct free_page *free_list;
    static size_t free_pages;
    static size_t total_pages;
    
    static uint64_t align_up(uint64_t value)
    {
        if (value > UINT64_MAX - PAGE_MASK)
            return UINT64_MAX & ~PAGE_MASK;
        return (value + PAGE_MASK) & ~PAGE_MASK;
    }
    
    static bool overlaps(uint64_t base, uint64_t end, uint64_t other_base,
                         uint64_t other_size)
    {
        uint64_t other_end;
    
        if (other_size == 0)
            return false;
        other_end = other_base > UINT64_MAX - other_size
                        ? UINT64_MAX
                        : other_base + other_size;
        return base < other_end && other_base < end;
    }
    
    static bool page_reserved(const struct boot_info *info, uint64_t base,
                              uint64_t kernel_start, uint64_t kernel_end,
                              uint64_t dtb_start, uint64_t dtb_size,
                              uint64_t firmware_start)
    {
        uint64_t end = base + PAGE_SIZE;
    
        if (overlaps(base, end, kernel_start, kernel_end - kernel_start) ||
            overlaps(base, end, dtb_start, dtb_size) ||
            (base >= firmware_start && base < kernel_start))
            return true;
        for (size_t i = 0; i < info->reserved_count; ++i) {
            if (overlaps(base, end, info->reserved[i].base,
                         info->reserved[i].size))
                return true;
        }
        return false;
    }
    
    bool page_allocator_init(const struct boot_info *info, uint64_t kernel_start,
                             uint64_t kernel_end, uint64_t dtb_start,
                             uint64_t dtb_size)
    {
        uint64_t firmware_start = kernel_start;
    
        if (!info || kernel_start >= kernel_end)
            return false;
        free_list = 0;
        free_pages = 0;
        total_pages = 0;
    
        /* The RAM range containing the loaded kernel also contains OpenSBI below
         * it.  Treat that prefix as firmware-owned even if the DTB omits it. */
        for (size_t i = 0; i < info->memory_count; ++i) {
            uint64_t end = info->memory[i].base + info->memory[i].size;
    
            if (kernel_start >= info->memory[i].base && kernel_start < end) {
                firmware_start = info->memory[i].base;
                break;
            }
        }
    
        for (size_t i = 0; i < info->memory_count; ++i) {
            uint64_t base = align_up(info->memory[i].base);
            uint64_t raw_end = info->memory[i].base + info->memory[i].size;
            uint64_t end = raw_end & ~PAGE_MASK;
    
            if (raw_end < info->memory[i].base)
                return false;
            for (uint64_t page = base; page < end; page += PAGE_SIZE) {
                struct free_page *node;
    
                if (page_reserved(info, page, kernel_start, kernel_end,
                                  dtb_start, dtb_size, firmware_start))
                    continue;
                node = phys_to_virt(page);
                node->next = free_list;
                free_list = node;
                ++free_pages;
                ++total_pages;
            }
        }
        return free_pages != 0;
    }
    
    uint64_t page_alloc(void)
    {
        struct free_page *page = free_list;
    
        if (!page)
            return 0;
        free_list = page->next;
        --free_pages;
        return virt_to_phys(page);
    }
    
    void page_free(uint64_t physical)
    {
        struct free_page *page;
    
        if ((physical & PAGE_MASK) != 0 || physical == 0)
            return;
        page = phys_to_virt(physical);
        page->next = free_list;
        free_list = page;
        ++free_pages;
    }
    
    size_t page_free_count(void)
    {
        return free_pages;
    }
    
    size_t page_total_count(void)
    {
        return total_pages;
    }
    ✓ • 0ms

## Assistant

测试函数返回后，先把定时器测试收尾：

```c
csr_clear_sstatus(SSTATUS_SIE);
csr_clear_sie(SIE_STIE);
sbi_set_timer(UINT64_MAX);
test_running = false;
```

分别是：关闭中断总开关、关闭定时器中断使能、把下一次定时器事件推到最大计数值，再退出测试状态。

最后判断：

```c
return register_ticks >= 3 && register_failures == 0;
```

至少完成三次有效寄存器检查，而且没有发现错误，测试才算通过。`kernel_main` 随后打印 `M1 PASS`。

接下来进入内存管理：

```c
page_allocator_init(&boot_info,
                    virt_to_phys(__kernel_start),
                    virt_to_phys(__kernel_end),
                    boot_info.dtb_base,
                    boot_info.dtb_size);
```

我们先理解它要解决什么问题：**内核知道 RAM 在哪里，但还需要知道哪些页可以分配，以及如何记住它们。**

前面我们一直使用的启动页表、栈，都是提前预留的内存。现在要建立一个能在运行中不断分配、回收物理页的机制。

MiniCore 每次分配 **4 KiB**，用一条链表管理空闲页：

```c
struct free_page {
    struct free_page *next;
};

static struct free_page *free_list;
static size_t free_pages;
static size_t total_pages;
```

这里有一个巧妙之处：**链表节点就放在空闲页自身的开头。**

假设有三页空闲内存 A、B、C：

```text
free_list
    │
    ▼
页 A：前 8 字节存 next ─→ 页 B：前 8 字节存 next ─→ 页 C：next = NULL
```

不用额外申请一块内存存放节点。页还没分配出去，内核可以暂时借用它的开头来记账。

初始化时，代码遍历 DTB 提供的 RAM 范围，按页检查，并排除：

- 内核镜像和启动栈占用的范围。
- DTB 自身占用的范围。
- 固件占用的范围。
- DTB 声明的其他保留范围。

找到一个可用页后，就把它加入链表：

```c
node = phys_to_virt(page);
node->next = free_list;
free_list = node;
++free_pages;
++total_pages;
```

这里要区分两个地址：

```text
page：物理地址，用于描述这个物理页
node：对应的内核虚拟地址，用于真正写入 next
```

CPU 现在已经开启地址翻译，内核通过高地址直接映射访问这些物理页。这也说明了启动映射的作用：正式内存管理还没建好时，内核已经能访问并组织 RAM。

加入链表采用的是头插法。例如：

```text
原来：free_list → A → B

加入 C：
C.next = A
free_list = C

结果：free_list → C → A → B
```

分配一页时，只需取走链表头：

```c
uint64_t page_alloc(void)
{
    struct free_page *page = free_list;

    if (!page)
        return 0;

    free_list = page->next;
    --free_pages;
    return virt_to_phys(page);
}
```

这几行做完，原来的头节点便不再属于空闲链表，它的物理地址交给调用者。**函数没有清零页面，也没有复制页面内容，只是改变了管理状态。**

先确认这个设计的关键：**页 A 已经从空闲链表取出并分配给调用者后，调用者能不能覆盖它开头原先存放 `next` 的那八字节？**

## User

可以

## Assistant

对。页 A 已经不在空闲链表里，它开头原先的 `next` 就没有用途了，调用者可以使用整页，包括那八字节。

现在看反向操作：**调用者不需要这一页时，怎样还回来？**

项目里的 `page_free` 是：

```c
void page_free(uint64_t physical)
{
    struct free_page *page;

    if ((physical & PAGE_MASK) != 0 || physical == 0)
        return;

    page = phys_to_virt(physical);
    page->next = free_list;
    free_list = page;
    ++free_pages;
}
```

先检查传入地址：

```c
(physical & PAGE_MASK) != 0
```

其中：

```text
PAGE_SIZE = 4096 = 0x1000
PAGE_MASK = 4095 = 0x0fff
```

按位与 `0xfff`，取出的就是地址低 12 位。页起始地址必须按 4 KiB 对齐，所以低 12 位必须全为零：

```text
0x80400000 → 对齐，可以继续
0x80400008 → 不对齐，直接返回
```

它还拒绝地址零，因为本项目用零表示分配失败。

接着：

```c
page = phys_to_virt(physical);
```

把物理地址转换为可供内核解引用的高虚拟地址，然后头插到空闲链表：

```c
page->next = free_list;
free_list = page;
++free_pages;
```

例如：

```text
释放前：free_list → B → C

归还 A：
A.next = B
free_list = A

释放后：free_list → A → B → C
```

这也意味着：**调用 `page_free` 后，调用者就不能再把这一页当成自己拥有的内存使用。** 内核已经覆盖了它开头的内容，并且随时可能把整页分配给别人。

这里顺便区分三个操作：

| 操作 | 改变什么 |
|---|---|
| `page_alloc` | 从空闲链表取走一页 |
| `page_free` | 把一页放回空闲链表 |
| 页表映射与解除映射 | 改变某个虚拟地址能否访问某个物理页 |

**归还物理页不会自动解除旧的虚拟地址映射。** 后面读虚拟内存管理时，我们会看到调用者怎样配合处理。

还有一个重要边界：当前 `page_free` 只检查了非零和对齐。它没有检查这个地址是否属于可分配 RAM，也没有检查这一页是否已经释放过。正确使用依赖调用者。

我们用一个例子看看重复释放的后果。第一次释放 A 后：

```text
free_list → A → B
```

马上再释放一次 A：

```c
page->next = free_list;   // 此时 page 和 free_list 都是 A
free_list = page;
```

结果变成：

```text
free_list → A ─┐
            ↑ │
            └─┘
```

A 的 `next` 指向了自己。之后分配时，可能反复返回同一页，让不同调用者同时使用同一块物理内存。

所以这套分配器的约定是：**只能归还自己持有的、合法且尚未归还的页，并且归还后停止使用。**

下一步我们回到初始化，看看它怎样保证最初加入链表的每一页，确实是可以分配的 RAM。

## User

申请和释放使用的都是高地址，为什么中间还要转一次物理地址？

## Assistant

这里要纠正一个前提：**分配器内部用高虚拟地址操作链表，但它对外返回、接收的都是物理地址。**

看函数接口：

```c
uint64_t page_alloc(void);          // 返回物理地址
void page_free(uint64_t physical);  // 接收物理地址
```

两种地址各有用途：

| 地址 | 用途 |
|---|---|
| 高虚拟地址 | 内核读写这一页的内容，例如 `page->next` |
| 物理地址 | 标识实际分配的页，例如填入页表 PTE |

完整过程是：

```text
空闲链表中的高虚拟地址
    ↓ page_alloc：virt_to_phys
返回物理地址给调用者
    ↓ 调用者使用
page_free 接收物理地址
    ↓ phys_to_virt
通过高虚拟地址写 next，放回链表
```

为什么对外选择物理地址？因为这个模块分配的是**物理页**，而它的重要使用者是页表管理代码。你之前已经学过：**PTE 中需要填写物理页号，不能填写内核虚拟地址。**

例如，假设分配到：

```text
物理地址 PA = 0x80400000
```

调用者可以把它映射到某个虚拟地址，构造 PTE 时使用：

```c
pte = ((physical >> 12) << 10) | flags;
```

如果调用者要清零这一页，则需要先找到可访问的虚拟地址：

```c
void *p = phys_to_virt(physical);
memset(p, 0, PAGE_SIZE);
```

也就是同一页有两种视角：

```text
“把哪一页填进页表？” → 用物理地址
“现在往这一页写数据” → 用内核虚拟地址
```

项目中的转换也没有搬运数据，只是加减固定偏移：

```c
虚拟地址 = KERNEL_DIRECT_BASE + 物理地址;
物理地址 = 虚拟地址 - KERNEL_DIRECT_BASE;
```

后一条只适用于这里的 direct-map 地址，不能用于任意虚拟地址。

当然，也可以设计一个返回内核虚拟指针的分配器，但调用者建立 PTE 时仍需要转换成物理地址。MiniCore 选择在分配器接口处统一使用物理地址，让“分配物理页”和“如何访问它”区分清楚。

## User

继续

## Assistant

我们回到 `page_allocator_init`：**它要从 RAM 中挑出完整、未被占用的 4 KiB 页，再串成空闲链表。**

先清空管理状态：

```c
if (!info || kernel_start >= kernel_end)
    return false;

free_list = 0;
free_pages = 0;
total_pages = 0;
```

这里的 `kernel_start`、`kernel_end` 都是物理地址，表示内核占用的半开区间：

```text
[kernel_start, kernel_end)
```

“半开”表示包含起点，不包含终点。

接下来，代码找到包含内核的 RAM 区域，将它的起点记为 `firmware_start`，并保留：

```text
[firmware_start, kernel_start)
```

这是项目针对当前启动布局采取的保守策略：内核之前的这段区域按固件所有处理，即使 DTB 没有明确列出来，也不交给分配器。

然后逐段处理 RAM：

```c
uint64_t base = align_up(info->memory[i].base);
uint64_t raw_end = info->memory[i].base + info->memory[i].size;
uint64_t end = raw_end & ~PAGE_MASK;
```

这一步做的是：**起点向上对齐，终点向下对齐，只留下完整页。**

举一个假设的 RAM 范围：

```text
起点：0x80000003
终点：0x80005007，不包含这个地址
```

对齐后：

```text
base = 0x80001000
end  = 0x80005000
```

可以考虑分配的完整页是：

```text
0x80001000～0x80001fff
0x80002000～0x80002fff
0x80003000～0x80003fff
0x80004000～0x80004fff
```

开头和末尾的零碎空间舍弃，否则可能把不属于这段 RAM 的字节交给调用者。

代码还检查 `base + size` 是否发生无符号溢出：

```c
if (raw_end < info->memory[i].base)
    return false;
```

范围确定后，按页遍历：

```c
for (uint64_t page = base; page < end; page += PAGE_SIZE) {
    if (page_reserved(info, page, kernel_start, kernel_end,
                      dtb_start, dtb_size, firmware_start))
        continue;

    node = phys_to_virt(page);
    node->next = free_list;
    free_list = node;
    ++free_pages;
    ++total_pages;
}
```

头插法我们已经懂了，现在重点看 `page_reserved`：**这一页是否碰到了不能分配的区域？**

它检查内核、DTB、固件前缀，以及 DTB 中记录的保留区域。其中，内核和 DTB 等范围使用 `overlaps` 判断是否相交。

两个半开区间：

```text
当前页：[base, end)
保留区：[other_base, other_end)
```

相交条件是：

```c
base < other_end && other_base < end
```

例如，一页是：

```text
[0x80400000, 0x80401000)
```

而 DTB 仅占据其中一小段：

```text
[0x80400800, 0x80400900)
```

这**整页都不能分配**。因为分配器的单位是 4 KiB，拿到这一页的调用者可以覆盖全部内容，不能要求它避开中间那 256 字节。

反过来，两个区域只是首尾相接：

```text
当前页：[0x80400000, 0x80401000)
保留区：[0x80401000, 0x80402000)
```

则没有重叠。这正是比较中使用 `<` 而非 `<=` 的原因。

初始化最后返回：

```c
return free_pages != 0;
```

只要至少找到一页可分配内存，就报告成功。此时：

```text
free_pages  = 当前空闲页数
total_pages = 初始化时纳入分配器的总页数
```

分配、释放会改变 `free_pages`，不会改变 `total_pages`。后者不等于机器的全部 RAM 页数，因为保留页已经被排除了。

确认一下：**如果 DTB 只有 100 字节，但恰好跨过两个 4 KiB 页的边界，这个分配器需要保留一页还是两页？**

## User

0

## Assistant

需要保留 **两页**。虽然 DTB 只有 100 字节，但它在两页里都占了空间。

假设分布是：

```text
第一页：4096 字节
┌──────────────────────────┬─────────┐
│                          │ DTB 前50│
└──────────────────────────┴─────────┘
第二页：4096 字节
┌─────────┬──────────────────────────┐
│ DTB 后50│                          │
└─────────┴──────────────────────────┘
```

分配器只能整页分配。任何一页交给调用者后，都可能被整页清零，破坏其中的 DTB。因此，只要一页与保留区域有重叠，就必须排除整页。

这里不能用 `100 / 4096 = 0` 来计算需要保留多少页；要看这 100 字节实际覆盖了哪些页。

## User

继续

## Assistant

接下来进入正式页表：物理页分配器准备好了，内核才能申请新的页表页，把启动时宽松的大页映射换成更精细的映射。

## Activity

    $ sed -n '1,230p' src/mm/vm.c && sed -n '230,440p' src/mm/vm.c && sed -n '1,145p' src/mm/vm.h
    #include "vm.h"
    
    #include "layout.h"
    #include "page_alloc.h"
    #include "../runtime.h"
    
    #define PTE_PPN_MASK UINT64_C(0x003ffffffffffc00)
    #define SATP_SV39 (UINT64_C(8) << 60)
    
    extern char __text_start[], __text_end[], __rodata_start[], __rodata_end[];
    extern char __data_start[], __kernel_end[], __boot_stack_guard[];
    
    static bool canonical(uint64_t address)
    {
        uint64_t upper = address >> 39;
        return upper == 0 || upper == UINT64_C(0x1ffffff);
    }
    
    static uint64_t pte_physical(uint64_t pte)
    {
        return (pte & PTE_PPN_MASK) << 2;
    }
    
    static uint64_t make_table_pte(uint64_t physical)
    {
        return (physical >> 2) | PTE_V;
    }
    
    static uint64_t *root_pointer(const struct page_table *table)
    {
        return phys_to_virt(table->root);
    }
    
    static size_t vpn_index(uint64_t address, unsigned level)
    {
        return (address >> (12 + level * 9)) & 0x1ffU;
    }
    
    static bool valid_leaf_flags(uint64_t flags)
    {
        const uint64_t allowed = PTE_R | PTE_W | PTE_X | PTE_U | PTE_G;
    
        return (flags & ~allowed) == 0 && (flags & (PTE_R | PTE_X)) != 0 &&
               !((flags & PTE_W) != 0 && (flags & PTE_R) == 0);
    }
    
    static uint64_t leaf_pte(uint64_t physical, uint64_t flags)
    {
        uint64_t hardware = flags | PTE_V | PTE_A;
    
        if ((flags & PTE_W) != 0)
            hardware |= PTE_D;
        return (physical >> 2) | hardware;
    }
    
    bool vm_create(struct page_table *table)
    {
        if (!table)
            return false;
        table->root = page_alloc();
        if (!table->root)
            return false;
        memset(phys_to_virt(table->root), 0, PAGE_SIZE);
        return true;
    }
    
    bool vm_create_user(struct page_table *table,
                        const struct page_table *kernel_table)
    {
        uint64_t *root;
        const uint64_t *kernel_root;
    
        if (!kernel_table || !kernel_table->root || !vm_create(table))
            return false;
        root = root_pointer(table);
        kernel_root = root_pointer(kernel_table);
        /* Sv39 lower half is private to the process.  The upper-half kernel
         * mappings are shared and contain no PTE_U leaves. */
        memcpy(root + 256, kernel_root + 256, 256 * sizeof(*root));
        return true;
    }
    
    static uint64_t *walk(struct page_table *table, uint64_t address, bool create)
    {
        uint64_t *entries;
    
        if (!table || !table->root || !canonical(address))
            return 0;
        entries = root_pointer(table);
        for (unsigned level = 2; level > 0; --level) {
            uint64_t *entry = &entries[vpn_index(address, level)];
    
            if ((*entry & PTE_V) == 0) {
                uint64_t page;
    
                if (!create)
                    return 0;
                page = page_alloc();
                if (!page)
                    return 0;
                memset(phys_to_virt(page), 0, PAGE_SIZE);
                *entry = make_table_pte(page);
            } else if ((*entry & (PTE_R | PTE_W | PTE_X)) != 0) {
                return 0;
            }
            entries = phys_to_virt(pte_physical(*entry));
        }
        return &entries[vpn_index(address, 0)];
    }
    
    bool vm_map(struct page_table *table, uint64_t virtual_address,
                uint64_t physical_address, uint64_t flags)
    {
        uint64_t *entry;
    
        if ((virtual_address & PAGE_MASK) != 0 ||
            (physical_address & PAGE_MASK) != 0 || !valid_leaf_flags(flags))
            return false;
        entry = walk(table, virtual_address, true);
        if (!entry || (*entry & PTE_V) != 0)
            return false;
        *entry = leaf_pte(physical_address, flags);
        return true;
    }
    
    static bool table_empty(const uint64_t *entries)
    {
        for (size_t i = 0; i < 512; ++i) {
            if ((entries[i] & PTE_V) != 0)
                return false;
        }
        return true;
    }
    
    bool vm_unmap(struct page_table *table, uint64_t address)
    {
        uint64_t *levels[3];
        size_t indices[3];
    
        if (!table || !table->root || (address & PAGE_MASK) != 0 ||
            !canonical(address))
            return false;
        levels[2] = root_pointer(table);
        for (unsigned level = 2; level > 0; --level) {
            uint64_t entry;
    
            indices[level] = vpn_index(address, level);
            entry = levels[level][indices[level]];
            if ((entry & PTE_V) == 0 ||
                (entry & (PTE_R | PTE_W | PTE_X)) != 0)
                return false;
            levels[level - 1] = phys_to_virt(pte_physical(entry));
        }
        indices[0] = vpn_index(address, 0);
        if ((levels[0][indices[0]] & (PTE_V | PTE_R | PTE_X)) == PTE_V)
            return false;
        if ((levels[0][indices[0]] & PTE_V) == 0)
            return false;
        levels[0][indices[0]] = 0;
        for (unsigned level = 0; level < 2; ++level) {
            if (!table_empty(levels[level]))
                break;
            page_free(virt_to_phys(levels[level]));
            levels[level + 1][indices[level + 1]] = 0;
        }
        vm_flush_all();
        return true;
    }
    
    bool vm_protect(struct page_table *table, uint64_t address, uint64_t flags)
    {
        uint64_t *entry;
    
        if ((address & PAGE_MASK) != 0 || !valid_leaf_flags(flags))
            return false;
        entry = walk(table, address, false);
        if (!entry || (*entry & PTE_V) == 0 ||
            (*entry & (PTE_R | PTE_X)) == 0)
            return false;
        *entry = leaf_pte(pte_physical(*entry), flags);
        vm_flush_all();
        return true;
    }
    
    bool vm_query(const struct page_table *table, uint64_t address,
                  uint64_t *physical, uint64_t *flags)
    {
        uint64_t *entry = walk((struct page_table *)table, address, false);
    
        if (!entry || (*entry & PTE_V) == 0 ||
            (*entry & (PTE_R | PTE_X)) == 0)
            return false;
        if (physical)
            *physical = pte_physical(*entry) | (address & PAGE_MASK);
        if (flags)
            *flags = *entry & 0x3ffU;
        return true;
    }
    
    static void destroy_level(uint64_t physical, unsigned level)
    {
        uint64_t *entries = phys_to_virt(physical);
    
        if (level != 0) {
            for (size_t i = 0; i < 512; ++i) {
                uint64_t entry = entries[i];
    
                if ((entry & PTE_V) != 0 &&
                    (entry & (PTE_R | PTE_W | PTE_X)) == 0)
                    destroy_level(pte_physical(entry), level - 1);
            }
        }
        page_free(physical);
    }
    
    void vm_destroy(struct page_table *table)
    {
        if (!table || !table->root)
            return;
        destroy_level(table->root, 2);
        table->root = 0;
    }
    
    void vm_destroy_user(struct page_table *table)
    {
        uint64_t *root;
    
        if (!table || !table->root)
            return;
        root = root_pointer(table);
        root = root_pointer(table);
        for (size_t i = 0; i < 256; ++i) {
            uint64_t entry = root[i];
    
            if ((entry & PTE_V) != 0 &&
                (entry & (PTE_R | PTE_W | PTE_X)) == 0)
                destroy_level(pte_physical(entry), 1);
        }
        page_free(table->root);
        table->root = 0;
    }
    
    void vm_flush_all(void)
    {
        __asm__ volatile("sfence.vma zero, zero" : : : "memory");
    }
    
    void vm_activate(const struct page_table *table)
    {
        uint64_t satp = SATP_SV39 | (table->root >> 12);
    
        __asm__ volatile("csrw satp, %0\nsfence.vma zero, zero"
                         : : "r"(satp) : "memory");
    }
    
    static uint64_t kernel_page_flags(uint64_t physical)
    {
        uint64_t text_start = virt_to_phys(__text_start);
        uint64_t text_end = virt_to_phys(__text_end);
        uint64_t rodata_start = virt_to_phys(__rodata_start);
        uint64_t rodata_end = virt_to_phys(__rodata_end);
    
        if (physical >= (text_start & ~PAGE_MASK) &&
            physical < ((text_end + PAGE_MASK) & ~PAGE_MASK))
            return PTE_R | PTE_X | PTE_G;
        if (physical >= (rodata_start & ~PAGE_MASK) &&
            physical < ((rodata_end + PAGE_MASK) & ~PAGE_MASK))
            return PTE_R | PTE_G;
        return PTE_R | PTE_W | PTE_G;
    }
    
    static bool map_physical_range(struct page_table *table, uint64_t base,
                                   uint64_t size, bool ram)
    {
        uint64_t first = base & ~PAGE_MASK;
        uint64_t end;
    
        if (base > UINT64_MAX - size || base + size > UINT64_MAX - PAGE_MASK)
            return false;
        end = (base + size + PAGE_MASK) & ~PAGE_MASK;
        for (uint64_t physical = first; physical < end; physical += PAGE_SIZE) {
            uint64_t virtual_address = KERNEL_DIRECT_BASE + physical;
            uint64_t flags = ram ? kernel_page_flags(physical) : PTE_R | PTE_W | PTE_G;
    
            if (virtual_address == (uint64_t)(uintptr_t)__boot_stack_guard)
                continue;
            if (!vm_map(table, virtual_address, physical, flags))
                return false;
        }
        return true;
    }
    
    bool vm_build_kernel(struct page_table *table, const struct boot_info *info)
    {
        if (!info || !vm_create(table))
            return false;
        for (size_t i = 0; i < info->memory_count; ++i) {
            if (!map_physical_range(table, info->memory[i].base,
                                    info->memory[i].size, true))
                goto fail;
        }
        if (!map_physical_range(table, UINT64_C(0x100000), PAGE_SIZE, false) ||
            (info->uart_present &&
             !map_physical_range(table, info->uart.base, info->uart.size, false)) ||
            (info->plic_present &&
             !map_physical_range(table, info->plic.base, info->plic.size, false)))
            goto fail;
        for (size_t i = 0; i < info->virtio_count; ++i) {
            if (!map_physical_range(table, info->virtio[i].base,
                                    info->virtio[i].size, false))
                goto fail;
        }
        return true;
    fail:
        vm_destroy(table);
        return false;
    }
    #ifndef MINICORE_MM_VM_H
    #define MINICORE_MM_VM_H
    
    #include <stdbool.h>
    #include <stdint.h>
    
    #include "../platform/dtb.h"
    
    #define PTE_V (UINT64_C(1) << 0)
    #define PTE_R (UINT64_C(1) << 1)
    #define PTE_W (UINT64_C(1) << 2)
    #define PTE_X (UINT64_C(1) << 3)
    #define PTE_U (UINT64_C(1) << 4)
    #define PTE_G (UINT64_C(1) << 5)
    #define PTE_A (UINT64_C(1) << 6)
    #define PTE_D (UINT64_C(1) << 7)
    
    struct page_table {
        uint64_t root;
    };
    
    bool vm_create(struct page_table *table);
    bool vm_create_user(struct page_table *table,
                        const struct page_table *kernel_table);
    void vm_destroy(struct page_table *table);
    void vm_destroy_user(struct page_table *table);
    bool vm_map(struct page_table *table, uint64_t virtual_address,
                uint64_t physical_address, uint64_t flags);
    bool vm_unmap(struct page_table *table, uint64_t virtual_address);
    bool vm_protect(struct page_table *table, uint64_t virtual_address,
                    uint64_t flags);
    bool vm_query(const struct page_table *table, uint64_t virtual_address,
                  uint64_t *physical_address, uint64_t *flags);
    void vm_activate(const struct page_table *table);
    void vm_flush_all(void);
    bool vm_build_kernel(struct page_table *table, const struct boot_info *info);
    
    #endif
    ✓ • 0ms

## Assistant

物理页分配器准备好后，`kernel_main` 紧接着做：

```c
if (!vm_build_kernel(&kernel_page_table, &boot_info))
    panic("cannot build kernel page table");

vm_activate(&kernel_page_table);
```

这两步分别是：**在内存中建立新页表，然后让 CPU 切换过去。**

先回答一个问题：启动页表已经能用，为什么还要再建一份？

启动时用的是 1 GiB 大页，权限统一允许读、写、执行。这样方便启动，但无法分别保护代码、只读数据和栈。

正式页表改用 4 KiB 页，实现：

| 区域 | 正式映射权限 |
|---|---|
| 内核代码 `.text` | 可读、可执行，不可写 |
| 只读数据 `.rodata` | 可读，不可写、不可执行 |
| 普通数据、栈等 RAM | 可读写，不可执行 |
| 启动栈下方 guard page | 不映射 |
| 映射的设备寄存器区域 | 可读写，不可执行 |

因此，这一阶段不仅是“换一张表”，也是**开始建立内存保护边界**。

我们先看最小的一步：申请一个空根页表。

项目用这个结构管理一套页表：

```c
struct page_table {
    uint64_t root;
};
```

`root` 存的是**根页表的物理地址**，不是把 512 个表项直接装进结构体。

创建函数是：

```c
bool vm_create(struct page_table *table)
{
    if (!table)
        return false;

    table->root = page_alloc();
    if (!table->root)
        return false;

    memset(phys_to_virt(table->root), 0, PAGE_SIZE);
    return true;
}
```

这段正好把上一节的地址转换用起来：

```text
page_alloc()
    → 得到一页的物理地址
    → 保存到 table->root
    → 转成内核虚拟地址
    → 清零这一页的 4096 字节
```

为什么必须清零？`page_alloc` 不负责清零，页面开头甚至还残留着原来的空闲链表指针。如果把旧内容直接当作 PTE，可能出现意外有效的表项。

清零后，512 个 PTE 的有效位都为零，得到一张空表。

假设这次分配得到的物理地址是 `0x80400000`：

```text
kernel_page_table.root = 0x80400000
                                  │
                                  ▼
                         一页 4096 字节的 RAM
                         ┌─────────────────┐
                         │ root[0]   = 0   │
                         │ root[1]   = 0   │
                         │ ...             │
                         │ root[511] = 0   │
                         └─────────────────┘
```

这个地址只是教学示例，实际取决于分配器。

注意此时的状态：**CPU 仍在使用旧的启动页表。** 新根页表只是 RAM 中一块正在准备的数据，还没有写入 `satp`。

旧的 direct map 允许内核访问这块 RAM，所以我们可以一边用旧页表运行，一边填写新页表：

```text
旧页表：保证当前代码、栈和内存访问正常
新页表：逐步建立正式映射
    ↓ 填写完成
修改 satp，切换到新页表
```

下一步会用 `vm_map` 给新页表逐页建立映射。它不仅要填写最终的 L0 叶子，还可能需要临时申请 L1、L0 页表页。

先确认一个概念：**`vm_create` 申请的这一页，是用来存放程序数据，还是用来存放描述地址映射的表项？**

## User

后者

## Assistant

对，这一页专门存放页表项。接下来我们给空页表加一条映射。

用一个具体例子：

```text
虚拟地址：0xffffffc080400000
物理地址：0x0000000080400000
权限：    可读、可写
```

调用形式是：

```c
vm_map(table,
       0xffffffc080400000,
       0x80400000,
       PTE_R | PTE_W);
```

它映射的是从这两个地址开始的 **一页，也就是 4 KiB**。

前面学过，4 KiB 映射需要走到 L0。但我们刚创建的只有一张空 L2 根页表，所以还需要补出两张表：

```text
L2 根页表 → L1 页表 → L0 页表 → 目标物理页
```

**前三张页保存映射规则，最后一页才是被映射的内存。**

先看 `vm_map` 的主体：

```c
if ((virtual_address & PAGE_MASK) != 0 ||
    (physical_address & PAGE_MASK) != 0 ||
    !valid_leaf_flags(flags))
    return false;

entry = walk(table, virtual_address, true);
if (!entry || (*entry & PTE_V) != 0)
    return false;

*entry = leaf_pte(physical_address, flags);
return true;
```

先检查地址是否按页对齐、权限是否合法，再调用 `walk` 找到应该填写的 L0 表项。第三个参数 `true` 表示：**路径上的页表不存在时，可以创建。**

我们沿这次调用走一遍。

这个虚拟地址拆出来的索引是：

```text
VPN[2] = 258
VPN[1] = 2
VPN[0] = 0
```

索引由这个辅助函数计算：

```c
static size_t vpn_index(uint64_t address, unsigned level)
{
    return (address >> (12 + level * 9)) & 0x1ffU;
}
```

`level=2` 时右移 30 位，`level=1` 时右移 21 位，`level=0` 时右移 12 位；再取九位。

**第一步，检查 L2 根页表的第 258 项。**

根页表刚清零，所以它无效。`walk` 会执行：

```c
page = page_alloc();
if (!page)
    return 0;

memset(phys_to_virt(page), 0, PAGE_SIZE);
*entry = make_table_pte(page);
```

申请一页、清零，把它作为 L1 页表，然后让根表项指向它。

`make_table_pte` 是：

```c
static uint64_t make_table_pte(uint64_t physical)
{
    return (physical >> 2) | PTE_V;
}
```

它只设置 `V`，没有设置 `R/W/X`，所以这是**指向下一级页表的非叶子表项**。

**第二步，进入新建的 L1 页表，检查第 2 项。**

这一项也为零，因此再申请一页作为 L0 页表，并填写指向它的非叶子表项。

**第三步，找到 L0 页表的第 0 项。**

`walk` 返回这个表项的指针。它返回的不是目标物理地址，而是：

> “你应该把映射规则写到这里。”

此时的结构是：

```text
L2[258] → L1 页表
              L1[2] → L0 页表
                          L0[0] = 0 ← walk 返回这个槽位的地址
```

回到 `vm_map`，确认这个槽位没有已有映射，再写入：

```c
*entry = leaf_pte(physical_address, flags);
```

最终：

```text
L2[258] → L1[2] → L0[0] → 物理页 0x80400000
```

这里还要区分：`walk` 是**内核主动执行的 C 代码**，用来查找或建立页表结构；前面学过的 CPU 硬件查表，是在实际访问虚拟地址时使用这些结构。

完成这次映射后，新根页表之外又申请了两页，分别存放 L1、L0。`vm_map` 没有申请目标数据页，目标物理地址由调用者提供。

接下来想一下：**如果再映射紧邻的下一页 `0xffffffc080401000`，它的 L2、L1 索引不变，只有 L0 索引从 0 变成 1，还需要重新申请两张下级页表吗？**

## User

不用

## Assistant

对。相邻页可以复用已经建立的 L1、L0 页表，只需要填写 `L0[1]`：

```text
L2[258] → L1[2] → L0 页表
                    ├─ [0] → 第一页物理内存
                    └─ [1] → 第二页物理内存
```

一张 L0 页表有 512 项，每项映射 4 KiB，因此能描述 **2 MiB 的虚拟地址范围**。只有进入需要其他下级页表的范围时，才可能继续申请页表页。

接下来把 `vm_map` 最后一步拆开：

```c
*entry = leaf_pte(physical_address, flags);
```

项目中的 `leaf_pte` 是：

```c
static uint64_t leaf_pte(uint64_t physical, uint64_t flags)
{
    uint64_t hardware = flags | PTE_V | PTE_A;

    if ((flags & PTE_W) != 0)
        hardware |= PTE_D;

    return (physical >> 2) | hardware;
}
```

它做两件事：编码物理页号，补上硬件使用的标志位。

假设调用者要求：

```c
flags = PTE_R | PTE_W;
```

函数会补上：

```text
V：表项有效
A：预先设置访问位
D：可写映射预先设置脏位
```

最终标志为：

```text
V | R | W | A | D = 0xc7
```

对物理地址 `0x80400000`，最终 PTE 是：

```text
地址部分：0x80400000 >> 2 = 0x20100000
标志部分：                  0x000000c7
合并结果：                  0x201000c7
```

这里设置 `R/W`，说明它是叶子，CPU 不再把它当成下一张页表的地址。

现在正式页表的构建过程就比较清楚了。`vm_build_kernel` 大体上是：

```text
创建空根页表
    ↓
遍历 RAM，每 4 KiB 建立一条高地址映射
    ↓
根据物理页属于哪个内核段，设置不同权限
    ↓
跳过启动栈 guard page
    ↓
映射需要访问的设备区域
```

对 RAM，选择权限的代码主要是：

```c
if (physical 属于 text)
    return PTE_R | PTE_X | PTE_G;

if (physical 属于 rodata)
    return PTE_R | PTE_G;

return PTE_R | PTE_W | PTE_G;
```

`G` 表示全局映射属性；当前先关注 `R/W/X`。

每页的虚拟地址仍按我们熟悉的规则生成：

```c
virtual_address = KERNEL_DIRECT_BASE + physical;
```

所以高地址直接映射的关系保持不变，但权限变精细了：

```text
同样访问代码页：
启动页表允许写入
正式页表禁止写入
```

guard page 则通过不创建映射来保护：

```c
if (virtual_address == (uint64_t)(uintptr_t)__boot_stack_guard)
    continue;
```

所有映射准备好后，才调用：

```c
vm_activate(&kernel_page_table);
```

内部是：

```c
uint64_t satp = SATP_SV39 | (table->root >> 12);

__asm__ volatile(
    "csrw satp, %0\n"
    "sfence.vma zero, zero"
    :
    : "r"(satp)
    : "memory"
);
```

这次不需要重新跳到高地址：**CPU 本来就在高地址运行，新页表也把当前代码和栈映射到原来的物理位置。** 切换的是翻译规则所在的根页表。

接下来内核会主动写代码页、访问 guard page，验证保护是否生效。

先确认权限差别：**一页物理内存即使仍然存在于 RAM 中，如果当前页表没有给某个虚拟地址建立映射，通过这个虚拟地址访问它会成功吗？**

## User

不会

## Assistant

对，物理内存存在不等于某个虚拟地址可访问。接下来用项目的“映射 → 写入 → 解除映射 → 触发页故障”测试，把分配器和页表操作串起来。

## Activity

    $ sed -n '53,104p' src/main.c && sed -n '123,175p' src/mm/vm.c && sed -n '141,175p' src/arch/riscv/trap.c
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
        return true;
    }
    
    static bool table_empty(const uint64_t *entries)
    {
        for (size_t i = 0; i < 512; ++i) {
            if ((entries[i] & PTE_V) != 0)
                return false;
        }
        return true;
    }
    
    bool vm_unmap(struct page_table *table, uint64_t address)
    {
        uint64_t *levels[3];
        size_t indices[3];
    
        if (!table || !table->root || (address & PAGE_MASK) != 0 ||
            !canonical(address))
            return false;
        levels[2] = root_pointer(table);
        for (unsigned level = 2; level > 0; --level) {
            uint64_t entry;
    
            indices[level] = vpn_index(address, level);
            entry = levels[level][indices[level]];
            if ((entry & PTE_V) == 0 ||
                (entry & (PTE_R | PTE_W | PTE_X)) != 0)
                return false;
            levels[level - 1] = phys_to_virt(pte_physical(entry));
        }
        indices[0] = vpn_index(address, 0);
        if ((levels[0][indices[0]] & (PTE_V | PTE_R | PTE_X)) == PTE_V)
            return false;
        if ((levels[0][indices[0]] & PTE_V) == 0)
            return false;
        levels[0][indices[0]] = 0;
        for (unsigned level = 0; level < 2; ++level) {
            if (!table_empty(levels[level]))
                break;
            page_free(virt_to_phys(levels[level]));
            levels[level + 1][indices[level + 1]] = 0;
        }
        vm_flush_all();
        return true;
    }
    
    bool vm_protect(struct page_table *table, uint64_t address, uint64_t flags)
    {
        uint64_t *entry;
    
        if ((address & PAGE_MASK) != 0 || !valid_leaf_flags(flags))
            return false;
            page_fault_seen = true;
            frame->sepc += trapped_instruction_size(frame->sepc);
            return;
        }
    
        print_trap(exception_name(code), frame);
        if (test_running && exception_index <
                sizeof(expected_exceptions) / sizeof(expected_exceptions[0]) &&
            code == expected_exceptions[exception_index]) {
            ++exception_index;
            frame->sepc += trapped_instruction_size(frame->sepc);
            return;
        }
    
        unexpected("unhandled exception", frame);
    }
    
    bool trap_expect_page_fault(volatile uint64_t *address, bool write)
    {
        uint64_t ignored = 0;
    
        expected_page_fault = write ? EXCEPTION_STORE_PAGE_FAULT
                                    : EXCEPTION_LOAD_PAGE_FAULT;
        expected_fault_address = (uintptr_t)address;
        page_fault_seen = false;
        page_fault_test_running = true;
        if (write)
            *address = UINT64_C(0xfeedfacecafebeef);
        else
            ignored = *address;
        page_fault_test_running = false;
        __asm__ volatile("" : : "r"(ignored) : "memory");
        return page_fault_seen;
    }
    
    ✓ • 0ms

## Assistant

对。项目接下来会故意制造这种情况，确认页表保护确实生效。

`run_vm_tests` 分两部分：先检查一份临时页表的数据操作，再在**正在使用的内核页表**上执行真实内存访问。我们先看后者，把流程串起来。

测试使用这个虚拟地址：

```c
const uint64_t test_virtual = 0xffffffd000000000;
```

**第一步：申请物理页，再建立映射。**

```c
page = page_alloc();

vm_map(&kernel_page_table, test_virtual, page,
       PTE_R | PTE_W);
```

注意两项操作各自负责什么：

```text
page_alloc → 得到一页可使用的物理内存
vm_map     → 让 test_virtual 指向这一页，允许读写
```

**第二步：真的通过虚拟地址写入、读回。**

```c
*(volatile uint64_t *)(uintptr_t)test_virtual =
    0x123456789abcdef0;
```

这行代码把地址转换成指向 64 位数的指针，再写入八字节。CPU 会使用当前页表翻译 `test_virtual`，把数值写进刚分配的物理页。

随后读取同一地址，确认结果相同。这里的 `volatile` 要求保留实际访问，不能让编译器直接拿已知常量代替读回。

**第三步：解除映射。**

```c
vm_unmap(&kernel_page_table, test_virtual);
```

这个函数找到对应的 L0 表项，清零：

```c
levels[0][indices[0]] = 0;
```

然后检查页表页是否已经空了：

```c
if (!table_empty(levels[level]))
    break;

page_free(virt_to_phys(levels[level]));
levels[level + 1][indices[level + 1]] = 0;
```

可以这样理解：

```text
删除一个 L0 表项
    ↓
这张 L0 表还有其他有效表项吗？
    ├─ 有：保留整张表
    └─ 没有：释放 L0 表页，清除父表中的指针
                    ↓
              再检查 L1 表是否为空
```

根页表仍然保留。最后调用 `vm_flush_all()`，执行我们学过的 `sfence.vma`，避免继续使用旧翻译。

这里释放的是**不再需要的页表页**，不是映射所指向的那张数据页。数据页此时仍由测试代码持有。

**第四步：再次写入，期待页故障。**

```c
trap_expect_page_fault(
    (volatile uint64_t *)(uintptr_t)test_virtual, true);
```

这个辅助函数会先记录：

```text
预期类型：写入页故障
预期地址：test_virtual
测试状态：正在等待这次故障
```

然后真的写：

```c
*address = 0xfeedfacecafebeef;
```

由于映射已经删除，CPU 触发页故障。`trap_dispatch` 检查原因和地址是否符合预期：

```c
if (page_fault_test_running &&
    code == expected_page_fault &&
    frame->stval == expected_fault_address) {

    page_fault_seen = true;
    frame->sepc += trapped_instruction_size(frame->sepc);
    return;
}
```

匹配后，记录“确实看到故障”，跳过这条故意失败的写指令，返回测试代码。

如果访问意外成功，就不会设置 `page_fault_seen`，测试反而返回失败。**这里发生预期故障才是测试通过。**

最后，测试归还数据页：

```c
page_free(page);
```

完整生命周期是：

```text
申请物理页
  → 建立映射
  → 写入并读回
  → 解除映射，回收空页表
  → 验证旧虚拟地址触发页故障
  → 归还数据页
```

确认一个资源管理问题：**如果完成 `vm_unmap` 后忘记最后的 `page_free(page)`，这张数据页会自动回到空闲链表吗？**

## User

不会

## Assistant

对。数据页会继续被占用，形成内存泄漏。**解除映射和归还物理页是两件独立的事。**

现在回头看 `run_vm_tests` 的第一部分：它创建一份临时页表，检查查询、权限修改和资源回收。**这份页表没有被写入 `satp`，CPU 不会用它执行实际地址翻译。**

代码先记录空闲页数，再申请数据页和根页表：

```c
size_t baseline = page_free_count();
uint64_t page = page_alloc();

vm_create(&temporary);
vm_map(&temporary, test_virtual, page, PTE_R | PTE_W);
```

`baseline` 是测试前的基准。测试结束后，空闲页数应该回到这个值。

接下来查询：

```c
vm_query(&temporary, test_virtual + 17, &physical, &flags);
```

为什么特意加 `17`？为了验证查询函数不仅能找到页的起点，还能正确处理**页内偏移**。

假设映射为：

```text
虚拟页起点 test_virtual → 物理页起点 page
```

那么：

```text
test_virtual + 17 → page + 17
```

实际实现是：

```c
bool vm_query(const struct page_table *table, uint64_t address,
              uint64_t *physical, uint64_t *flags)
{
    uint64_t *entry =
        walk((struct page_table *)table, address, false);

    if (!entry || (*entry & PTE_V) == 0 ||
        (*entry & (PTE_R | PTE_X)) == 0)
        return false;

    if (physical)
        *physical = pte_physical(*entry) | (address & PAGE_MASK);
    if (flags)
        *flags = *entry & 0x3ffU;
    return true;
}
```

这里有三个关键点：

- `walk(..., false)`：只查已有页表，不创建缺失的下级页表。
- `pte_physical(*entry)`：从 PTE 提取物理页起点。
- `address & PAGE_MASK`：提取虚拟地址的低 12 位，也就是页内偏移。

页起点低 12 位全零，所以按位或可以把偏移拼进去。`physical`、`flags` 是可选输出指针，传空指针就不写对应结果。

**这是软件读取页表数据，不是通过这个虚拟地址读取目标内存。** 因此，即使临时页表没有启用，也能查询。

然后修改权限：

```c
vm_protect(&temporary, test_virtual, PTE_R);
```

它找到已有叶子表项，保留物理地址，重新生成权限：

```c
*entry = leaf_pte(pte_physical(*entry), flags);
vm_flush_all();
```

也就是：

```text
修改前：虚拟页 → 同一物理页，可读写
修改后：虚拟页 → 同一物理页，只读
```

这个测试检查了调用成功；由于临时页表没有启用，这一步本身没有通过一次真实写入来验证只读保护。实际写入保护由后面的内核代码页测试检查。

最后收尾：

```c
vm_unmap(&temporary, test_virtual);
vm_destroy(&temporary);
page_free(page);

if (page_free_count() != baseline)
    return false;
```

资源分别由谁处理：

| 资源 | 回收位置 |
|---|---|
| 空的 L0、L1 页表页 | `vm_unmap` |
| 临时根页表，以及仍存在的下级表页 | `vm_destroy` |
| 被映射的数据页 | 显式 `page_free(page)` |

`vm_destroy` 递归释放页表结构，**不会把叶子映射指向的数据页一并释放**。这是它的接口约定，否则对于共享内存、设备映射等情况，盲目释放目标页会出错。

这组测试最终要确认：操作结束后，页表页和数据页都归还，空闲页数恢复。

确认一下：**`vm_query` 返回的 `physical` 是目标位置存放的数据，还是目标位置的物理地址？**

## User

后者

## Assistant

对，返回的是物理地址。我们把 M2 最后两个保护测试讲完，再看内核堆如何在物理页分配器之上提供小块内存。

## Activity

    $ sed -n '1,235p' src/mm/heap.c && sed -n '1,110p' src/mm/heap.h && sed -n '85,105p' src/main.c
    #include "heap.h"
    
    #include "layout.h"
    #include "page_alloc.h"
    #include "../kernel/list.h"
    #include "../kernel/sync.h"
    #include "../runtime.h"
    
    #define SLAB_MAGIC 0x534c4142U
    #define CLASS_COUNT 8U
    
    struct free_object {
        struct free_object *next;
    };
    
    struct slab {
        uint32_t magic;
        uint16_t class_index;
        uint16_t used;
        struct free_object *free;
        struct list_node link;
    };
    
    static const size_t class_sizes[CLASS_COUNT] = {
        16, 32, 64, 128, 256, 512, 1024, 2048
    };
    static struct list_node classes[CLASS_COUNT];
    static struct spinlock heap_lock;
    
    void heap_init(void)
    {
        for (size_t i = 0; i < CLASS_COUNT; ++i)
            list_init(&classes[i]);
        spinlock_init(&heap_lock);
    }
    
    static struct slab *new_slab(size_t class_index)
    {
        uint64_t physical = page_alloc();
        struct slab *slab;
        size_t size = class_sizes[class_index];
        size_t offset = (sizeof(*slab) + size - 1) & ~(size - 1);
    
        if (!physical)
            return 0;
        slab = phys_to_virt(physical);
        memset(slab, 0, sizeof(*slab));
        slab->magic = SLAB_MAGIC;
        slab->class_index = (uint16_t)class_index;
        list_init(&slab->link);
        for (; offset + size <= PAGE_SIZE; offset += size) {
            struct free_object *object = (void *)((char *)slab + offset);
    
            object->next = slab->free;
            slab->free = object;
        }
        list_push_back(&classes[class_index], &slab->link);
        return slab;
    }
    
    void *kmalloc(size_t size)
    {
        struct slab *slab = 0;
        void *result;
        uint64_t flags;
        size_t class_index;
    
        if (size == 0)
            size = 1;
        for (class_index = 0; class_index < CLASS_COUNT; ++class_index) {
            if (size <= class_sizes[class_index])
                break;
        }
        if (class_index == CLASS_COUNT)
            return 0;
        flags = irq_save();
        spin_lock(&heap_lock);
        for (struct list_node *node = classes[class_index].next;
             node != &classes[class_index]; node = node->next) {
            struct slab *candidate = container_of(node, struct slab, link);
    
            if (candidate->free) {
                slab = candidate;
                break;
            }
        }
        if (!slab)
            slab = new_slab(class_index);
        if (!slab) {
            spin_unlock(&heap_lock);
            irq_restore(flags);
            return 0;
        }
        result = slab->free;
        slab->free = slab->free->next;
        ++slab->used;
        spin_unlock(&heap_lock);
        irq_restore(flags);
        memset(result, 0, class_sizes[class_index]);
        return result;
    }
    
    void kfree(void *pointer)
    {
        uintptr_t base;
        struct slab *slab;
        struct free_object *object = pointer;
        uint64_t flags;
    
        if (!pointer)
            return;
        base = (uintptr_t)pointer & ~(uintptr_t)PAGE_MASK;
        slab = (struct slab *)base;
        flags = irq_save();
        spin_lock(&heap_lock);
        if (slab->magic != SLAB_MAGIC || slab->class_index >= CLASS_COUNT ||
            slab->used == 0) {
            spin_unlock(&heap_lock);
            irq_restore(flags);
            return;
        }
        object->next = slab->free;
        slab->free = object;
        --slab->used;
        if (slab->used == 0) {
            list_remove(&slab->link);
            slab->magic = 0;
            spin_unlock(&heap_lock);
            irq_restore(flags);
            page_free(virt_to_phys(slab));
            return;
        }
        spin_unlock(&heap_lock);
        irq_restore(flags);
    }
    #ifndef MINICORE_MM_HEAP_H
    #define MINICORE_MM_HEAP_H
    
    #include <stddef.h>
    
    void heap_init(void);
    void *kmalloc(size_t size);
    void kfree(void *pointer);
    
    #endif
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
    ✓ • 0ms

## Assistant

对，返回的是物理地址，不会读取那个位置的数据。

M2 最后还会检查两种保护：

```c
trap_expect_page_fault(
    (volatile uint64_t *)__text_start, true);

trap_expect_page_fault(
    (volatile uint64_t *)__boot_stack_guard, false);
```

第二个参数表示是否执行写入，所以它们分别测试：

| 操作 | 为什么应该失败 |
|---|---|
| 写入内核代码页 | 有映射，但没有写权限 |
| 读取栈下方 guard page | 没有映射 |

它们都应该产生页故障，但原因不同。通过测试后，内核打印 `M2 PASS`，再执行：

```c
heap_init();
```

现在进入**内核堆**。它要解决一个实际问题：物理页分配器每次给出 4096 字节，但一个小对象可能只需要几十字节。

例如，申请一个 40 字节的对象，如果直接给它一整页，大部分空间都会浪费。MiniCore 的堆把一页拆成多个固定大小的小块，供多个对象使用。

对外接口是：

```c
void *kmalloc(size_t size);
void kfree(void *pointer);
```

这次返回的是**可以直接读写的内核虚拟指针**，与返回物理地址的 `page_alloc` 不同。

项目支持八种块大小：

```c
static const size_t class_sizes[] = {
    16, 32, 64, 128, 256, 512, 1024, 2048
};
```

`kmalloc` 选择能容纳请求的最小档位：

```text
申请  12 字节 → 使用 16 字节块
申请  40 字节 → 使用 64 字节块
申请 100 字节 → 使用 128 字节块
```

当前实现对超过 2048 字节的请求返回空指针；大小为零的请求则按 1 字节处理，最终进入 16 字节档位。

一张用于存放这些小块的物理页，在这里叫一个 **slab**。同一页只服务一个大小档位：

```text
一页 4096 字节，服务 64 字节档位
┌──────────────────────────┐
│ slab 管理信息及对齐空间  │
├──────────────────────────┤
│ 64 字节对象块            │
├──────────────────────────┤
│ 64 字节对象块            │
├──────────────────────────┤
│ ……                       │
└──────────────────────────┘
```

管理信息的结构是：

```c
struct slab {
    uint32_t magic;
    uint16_t class_index;
    uint16_t used;
    struct free_object *free;
    struct list_node link;
};
```

每个字段都有具体用途：

- `magic`：识别这是一页 slab 的标记。
- `class_index`：这一页属于哪个大小档位。
- `used`：当前已经分配出去多少个块。
- `free`：这一页内部的空闲块链表。
- `link`：把这一页接入对应档位的 slab 链表。

所以这里有**两层链表**：

```text
64 字节档位
    → slab A → slab B → …

slab A 内部
    → 空闲块 1 → 空闲块 2 → …
```

这样，申请 40 字节时，堆先找 64 字节档位，再从其中某一页取出一个空闲块。没有合适的页时，才调用 `page_alloc` 申请新页并切分。

`heap_init` 本身只初始化各档位的链表和锁：

```c
void heap_init(void)
{
    for (size_t i = 0; i < CLASS_COUNT; ++i)
        list_init(&classes[i]);

    spinlock_init(&heap_lock);
}
```

**它没有提前申请八张物理页，物理页是在实际需要时申请的。**

下一步我们会用一次 `kmalloc(40)` 走完申请过程。先确认大小选择：**申请 65 字节，会进入 64 字节档位，还是 128 字节档位？**

## User

128

## Assistant

对，64 字节放不下 65 字节，所以选 128 字节档位。接下来用首次 `kmalloc(40)` 走一遍：申请物理页、切成小块，再取出一块。

## Activity

    $ sed -n '1,130p' src/kernel/list.h && sed -n '1,120p' src/kernel/sync.c && sed -n '1,112p' src/mm/heap.c
    #ifndef MINICORE_KERNEL_LIST_H
    #define MINICORE_KERNEL_LIST_H
    
    #include <stdbool.h>
    #include <stddef.h>
    
    struct list_node {
        struct list_node *prev;
        struct list_node *next;
    };
    
    #define LIST_HEAD(name) struct list_node name = { &(name), &(name) }
    #define container_of(pointer, type, member) \
        ((type *)((char *)(pointer) - offsetof(type, member)))
    
    static inline void list_init(struct list_node *head)
    {
        head->prev = head;
        head->next = head;
    }
    
    static inline bool list_empty(const struct list_node *head)
    {
        return head->next == head;
    }
    
    static inline void list_insert_before(struct list_node *position,
                                          struct list_node *node)
    {
        node->prev = position->prev;
        node->next = position;
        position->prev->next = node;
        position->prev = node;
    }
    
    static inline void list_push_back(struct list_node *head,
                                      struct list_node *node)
    {
        list_insert_before(head, node);
    }
    
    static inline void list_remove(struct list_node *node)
    {
        node->prev->next = node->next;
        node->next->prev = node->prev;
        list_init(node);
    }
    
    #endif
    #include "sync.h"
    
    #include "scheduler.h"
    #include "../arch/riscv/csr.h"
    
    uint64_t irq_save(void)
    {
        uint64_t status = csr_read_sstatus();
    
        csr_clear_sstatus(SSTATUS_SIE);
        return status;
    }
    
    void irq_restore(uint64_t flags)
    {
        if ((flags & SSTATUS_SIE) != 0)
            csr_set_sstatus(SSTATUS_SIE);
        else
            csr_clear_sstatus(SSTATUS_SIE);
    }
    
    bool irq_enabled(void)
    {
        return (csr_read_sstatus() & SSTATUS_SIE) != 0;
    }
    
    void spinlock_init(struct spinlock *lock)
    {
        lock->locked = 0;
    }
    
    void spin_lock(struct spinlock *lock)
    {
        uint32_t value;
    
        do {
            __asm__ volatile("amoswap.w.aq %0, %2, (%1)"
                             : "=r"(value)
                             : "r"(&lock->locked), "r"(1U)
                             : "memory");
        } while (value != 0);
    }
    
    void spin_unlock(struct spinlock *lock)
    {
        __asm__ volatile("fence rw, w" : : : "memory");
        lock->locked = 0;
    }
    
    void wait_queue_init(struct wait_queue *queue)
    {
        list_init(&queue->sleepers);
    }
    
    void wait_queue_sleep(struct wait_queue *queue, struct spinlock *lock)
    {
        scheduler_block_current(&queue->sleepers);
        spin_unlock(lock);
        scheduler_switch();
        spin_lock(lock);
    }
    
    bool wait_queue_wake_one(struct wait_queue *queue)
    {
        if (list_empty(&queue->sleepers))
            return false;
        scheduler_wake_node(queue->sleepers.next);
        return true;
    }
    
    void wait_queue_wake_all(struct wait_queue *queue)
    {
        while (wait_queue_wake_one(queue))
            ;
    }
    #include "heap.h"
    
    #include "layout.h"
    #include "page_alloc.h"
    #include "../kernel/list.h"
    #include "../kernel/sync.h"
    #include "../runtime.h"
    
    #define SLAB_MAGIC 0x534c4142U
    #define CLASS_COUNT 8U
    
    struct free_object {
        struct free_object *next;
    };
    
    struct slab {
        uint32_t magic;
        uint16_t class_index;
        uint16_t used;
        struct free_object *free;
        struct list_node link;
    };
    
    static const size_t class_sizes[CLASS_COUNT] = {
        16, 32, 64, 128, 256, 512, 1024, 2048
    };
    static struct list_node classes[CLASS_COUNT];
    static struct spinlock heap_lock;
    
    void heap_init(void)
    {
        for (size_t i = 0; i < CLASS_COUNT; ++i)
            list_init(&classes[i]);
        spinlock_init(&heap_lock);
    }
    
    static struct slab *new_slab(size_t class_index)
    {
        uint64_t physical = page_alloc();
        struct slab *slab;
        size_t size = class_sizes[class_index];
        size_t offset = (sizeof(*slab) + size - 1) & ~(size - 1);
    
        if (!physical)
            return 0;
        slab = phys_to_virt(physical);
        memset(slab, 0, sizeof(*slab));
        slab->magic = SLAB_MAGIC;
        slab->class_index = (uint16_t)class_index;
        list_init(&slab->link);
        for (; offset + size <= PAGE_SIZE; offset += size) {
            struct free_object *object = (void *)((char *)slab + offset);
    
            object->next = slab->free;
            slab->free = object;
        }
        list_push_back(&classes[class_index], &slab->link);
        return slab;
    }
    
    void *kmalloc(size_t size)
    {
        struct slab *slab = 0;
        void *result;
        uint64_t flags;
        size_t class_index;
    
        if (size == 0)
            size = 1;
        for (class_index = 0; class_index < CLASS_COUNT; ++class_index) {
            if (size <= class_sizes[class_index])
                break;
        }
        if (class_index == CLASS_COUNT)
            return 0;
        flags = irq_save();
        spin_lock(&heap_lock);
        for (struct list_node *node = classes[class_index].next;
             node != &classes[class_index]; node = node->next) {
            struct slab *candidate = container_of(node, struct slab, link);
    
            if (candidate->free) {
                slab = candidate;
                break;
            }
        }
        if (!slab)
            slab = new_slab(class_index);
        if (!slab) {
            spin_unlock(&heap_lock);
            irq_restore(flags);
            return 0;
        }
        result = slab->free;
        slab->free = slab->free->next;
        ++slab->used;
        spin_unlock(&heap_lock);
        irq_restore(flags);
        memset(result, 0, class_sizes[class_index]);
        return result;
    }
    
    void kfree(void *pointer)
    {
        uintptr_t base;
        struct slab *slab;
        struct free_object *object = pointer;
        uint64_t flags;
    
        if (!pointer)
            return;
        base = (uintptr_t)pointer & ~(uintptr_t)PAGE_MASK;
    ✓ • 0ms

## Assistant

对，选择能容纳请求的最小档位：65 字节需要 128 字节块。

现在走一次**首次申请 40 字节**：

```c
void *p = kmalloc(40);
```

`kmalloc` 先找到 64 字节档位，其下标是 `2`：

```text
下标： 0   1   2   3 …
大小：16  32  64  128 …
```

随后保存中断状态、关闭本地中断，再取得堆锁：

```c
flags = irq_save();
spin_lock(&heap_lock);
```

这段先理解为保护堆的共享管理数据，避免修改链表时被其他执行路径同时操作。锁的具体实现我们稍后展开。

它先搜索这个档位中有没有尚有空闲块的 slab。首次申请时没有，于是：

```c
slab = new_slab(class_index);
```

**`new_slab` 首先申请一张物理页。**

```c
uint64_t physical = page_alloc();

if (!physical)
    return 0;

slab = phys_to_virt(physical);
```

页头用于存放 `struct slab` 管理信息。项目当前的 RV64 布局下，这个结构占 32 字节：

```text
magic + class_index + used：8 字节
free 指针：                 8 字节
link 的两个指针：          16 字节
合计：                     32 字节
```

但对象块不能直接从偏移 32 开始，因为这里还要求对象块按其档位大小对齐。代码计算：

```c
size_t offset =
    (sizeof(*slab) + size - 1) & ~(size - 1);
```

代入 `sizeof(*slab)=32`、`size=64`，得到 `offset=64`。也就是把管理信息之后的位置向上对齐到 64 字节边界。

这一页的布局是：

```text
页内偏移
   0 ┌──────────────────────┐
     │ slab 管理信息，32字节│
  32 ├──────────────────────┤
     │ 对齐空隙，32字节     │
  64 ├──────────────────────┤
     │ 第一个 64 字节块     │
 128 ├──────────────────────┤
     │ 第二个 64 字节块     │
     │ ……                   │
4032 ├──────────────────────┤
     │ 最后一个 64 字节块   │
4096 └──────────────────────┘
```

所以这一页能容纳：

```text
(4096 - 64) / 64 = 63 个对象块
```

**接着，把这 63 个块串成空闲链表。**

```c
for (; offset + size <= PAGE_SIZE; offset += size) {
    struct free_object *object =
        (void *)((char *)slab + offset);

    object->next = slab->free;
    slab->free = object;
}
```

又是熟悉的办法：空闲块的开头暂时存放 `next`，不额外申请节点。

循环按地址递增，但使用头插法，所以最终链表顺序是：

```text
slab->free → 偏移4032 → 偏移3968 → … → 偏移64 → NULL
```

然后将这张 slab 加入 64 字节档位的链表，返回 `kmalloc`。

**`kmalloc` 从中取走一个块：**

```c
result = slab->free;
slab->free = slab->free->next;
++slab->used;
```

于是第一次申请取到页内偏移 4032 的块，`used` 从 0 变成 1。

最后释放锁、恢复原来的中断状态，并清零这个块：

```c
spin_unlock(&heap_lock);
irq_restore(flags);

memset(result, 0, class_sizes[class_index]);
return result;
```

这里清零的是整个 **64 字节块**。与 `page_alloc` 不同，项目里的 `kmalloc` 会清零返回的内存。

整次申请可以概括为：

```text
申请40字节
  → 选择64字节档位
  → 没有可用slab，申请一张物理页
  → 留出页头，将剩余空间切成63块
  → 取出一块，used变为1
  → 清零，返回内核虚拟指针
```

确认一下：**此时再申请一次 `kmalloc(40)`，需要再申请一张物理页吗？**

## User

不用，在已有的64slab中取一块就行

## Assistant

对，这个 64 字节档位的 slab 还剩 62 个空闲块，直接取一块即可。

接下来反过来：**`kfree(p)` 只有一个指针，怎么知道它属于哪张 slab、该放回哪个档位？**

项目利用了一个布局约定：**每张 slab 恰好占一页，管理信息放在页首。**

因此，只要把对象地址向下对齐到 4 KiB，就能找到管理信息：

```c
base = (uintptr_t)pointer & ~(uintptr_t)PAGE_MASK;
slab = (struct slab *)base;
```

假设页起点是 `0xffffffc080400000`，第一次返回的对象在偏移 `4032 = 0xfc0`：

```text
对象地址：0xffffffc080400fc0
清除低12位
页首地址：0xffffffc080400000
```

这里不是扫描链表，也不是查页表，只是通过对齐规则找回页首。

随后，`kfree` 关闭本地中断并加锁，检查页头：

```c
if (slab->magic != SLAB_MAGIC ||
    slab->class_index >= CLASS_COUNT ||
    slab->used == 0) {
    spin_unlock(&heap_lock);
    irq_restore(flags);
    return;
}
```

这些检查能发现部分不合法情况，但不是完整的指针验证。调用者仍须传入 `kmalloc` 返回的、尚未释放的原始指针，不能传对象中间的地址，也不能重复释放。`kfree(NULL)` 则直接返回。

检查通过，就把对象放回这一页的空闲链表：

```c
object->next = slab->free;
slab->free = object;
--slab->used;
```

这与物理页分配器的释放方法很像：**归还后，对象开头会被写成链表指针，调用者必须停止使用它。**

然后看这一页是否还有正在使用的对象：

```c
if (slab->used == 0) {
    list_remove(&slab->link);
    slab->magic = 0;

    spin_unlock(&heap_lock);
    irq_restore(flags);

    page_free(virt_to_phys(slab));
    return;
}
```

当 `used` 变成零，说明整页都空闲了：

1. 从对应档位的 slab 链表中移除这一页。
2. 清除标记。
3. 释放堆锁、恢复中断状态。
4. 把整张物理页归还给页分配器。

所以这里有两种不同的回收结果：

```text
kfree(对象)
    ↓
这一页还有其他对象在使用？
    ├─ 有 → 只把对象放回页内空闲链表
    └─ 无 → 移除 slab，归还整张物理页
```

用两次申请举例：

```c
void *a = kmalloc(40);
void *b = kmalloc(40);
```

假设它们来自同一张 slab：

```text
申请后：used = 2
释放 a：used = 1，保留这张 slab
释放 b：used = 0，归还整页
```

这就把两层分配器连接起来了：

```text
page_alloc / page_free：管理整张物理页
kmalloc / kfree：把页切成小块，管理内核对象
```

下一步我们看 `classes` 使用的双向循环链表，以及 `container_of` 怎样从链表节点找回所属的 slab。

先确认一个生命周期问题：**释放 `a` 后，即使整张 slab 因为 `b` 仍在使用而没有归还，代码还能继续通过 `a` 读写原来的对象吗？**

## User

不能

## Assistant

对。`a` 已经归还，可能被改写成链表节点，也可能马上分配给别人；整页是否归还不影响这一点。

接下来看堆如何把多张 slab 串起来。这里用的是**带头节点的双向循环链表**：

```c
struct list_node {
    struct list_node *prev;
    struct list_node *next;
};
```

每个节点记录前后两个节点。项目为每种大小档位准备一个头节点：

```c
static struct list_node classes[8];
```

例如 `classes[2]` 是 64 字节档位的链表头。**头节点只负责组织链表，本身不是一张 slab。**

初始化函数是：

```c
static inline void list_init(struct list_node *head)
{
    head->prev = head;
    head->next = head;
}
```

空链表没有使用 `NULL`，而是让头节点指向自己：

```text
head.next = head
head.prev = head
```

因此判断为空只需：

```c
return head->next == head;
```

加入两张 slab 后，关系是：

```text
向后：head → A → B → head
向前：head → B → A → head
```

遍历遇到 `head` 就结束。

关键是：A、B 在链表里并不是整个 slab，而是 slab 内部的 `link` 成员：

```c
struct slab {
    uint32_t magic;
    uint16_t class_index;
    uint16_t used;
    struct free_object *free;
    struct list_node link;       // 链表节点嵌在对象里面
};
```

因此，准确的连接关系是：

```text
classes[2] → slab_A.link → slab_B.link → classes[2]
```

这种设计不需要额外分配一个节点来包装 slab。

但它带来一个问题：遍历时拿到的是 `struct list_node *`，怎么读取所在 slab 的 `free`、`used` 等字段？

项目用：

```c
struct slab *candidate =
    container_of(node, struct slab, link);
```

宏的定义是：

```c
#define container_of(pointer, type, member) \
    ((type *)((char *)(pointer) - offsetof(type, member)))
```

它的含义就是：

```text
对象起始地址 = 成员地址 - 成员在对象中的偏移
```

在当前 RV64 布局下，`link` 位于 slab 起点后 16 字节：

```text
slab 起点 + 0  ：magic、class_index、used
slab 起点 + 8  ：free
slab 起点 + 16 ：link ← 遍历时拿到这个地址
```

所以已知 `&slab->link`，向前退 16 字节，就找到了 `slab` 起点。`offsetof` 由编译器计算偏移，不需要在代码中写死 `16`。

转换成 `char *`，是为了按字节减去偏移；如果直接对 `struct list_node *` 做减法，步长会变成整个节点的大小。

现在再看 `kmalloc` 中的搜索循环：

```c
for (struct list_node *node = classes[class_index].next;
     node != &classes[class_index];
     node = node->next) {

    struct slab *candidate =
        container_of(node, struct slab, link);

    if (candidate->free) {
        slab = candidate;
        break;
    }
}
```

就能直接读成：

```text
从该档位第一张 slab 开始
    → 从 link 找回 slab
    → 检查它有没有空闲对象
    → 有就选中，没有就继续下一张
    → 遇到头节点，说明遍历结束
```

这里循环条件还有一个重要作用：**确保不会把链表头当成 slab 的 `link` 去执行 `container_of`。** `container_of` 只做地址运算，不会帮你检查传入指针是否真的属于那种对象。

下一步再看插入和删除如何修改前后指针。先确认：**`container_of` 会申请或复制一份 slab 吗，还是仅仅算出原来那份 slab 的地址？**

## User

后者

## Assistant

对，它只算出原来那份 slab 的地址，没有申请或复制内存。

接下来看看双向链表怎样插入、删除节点。我们固定用这个例子：

```text
向后：head → A → B → head
向前：head → B → A → head
```

现在要把 C 插到链表末尾，也就是 B 和 head 之间。

项目的 `list_push_back` 是：

```c
static inline void list_push_back(struct list_node *head,
                                  struct list_node *node)
{
    list_insert_before(head, node);
}
```

因为链表是循环的，**插到 head 前面，就是插到末尾**。

真正修改指针的是：

```c
static inline void list_insert_before(struct list_node *position,
                                      struct list_node *node)
{
    node->prev = position->prev;
    node->next = position;
    position->prev->next = node;
    position->prev = node;
}
```

代入 `position=head`、`node=C`，逐条看：

```c
C->prev = head->prev;   // C 的前驱设为 B
C->next = head;         // C 的后继设为 head
head->prev->next = C;   // 此时 head->prev 仍是 B，所以设置 B->next
head->prev = C;         // 最后让 head 的前驱变为 C
```

结果：

```text
向后：head → A → B → C → head
向前：head → C → B → A → head
```

第三、四行的顺序值得注意：第三行还需要通过旧的 `head->prev` 找到 B。如果先把它改成 C，再执行第三行，就会误改 `C->next`。

这套写法也适用于空链表。空时 `head->prev=head`，所以插入第一个节点后，自然形成：

```text
head ⇄ C
```

再看删除：

```c
static inline void list_remove(struct list_node *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
    list_init(node);
}
```

例如从链表中删除 B：

```text
删除前：A ⇄ B ⇄ C
```

前两句把 B 的邻居直接连起来：

```c
A->next = C;
C->prev = A;
```

然后：

```c
list_init(B);
```

让 B 的前后指针都指向自己，表示它已经脱离原链表：

```text
原链表：A ⇄ C
B 自身：prev=B，next=B
```

**删除链表节点不等于释放对象内存。** `list_remove` 只修改指针。之前 `kfree` 的整页回收流程里，是先调用：

```c
list_remove(&slab->link);
```

把 slab 从档位链表摘除，再调用：

```c
page_free(virt_to_phys(slab));
```

归还物理页。否则档位链表可能还指着一张已经分配给别人的页。

这些链表函数也不自带锁，调用者需要保证修改期间没有其他执行路径同时操作它。堆代码中的关中断和 `heap_lock`，就是为此服务的。

确认一下删除的必要步骤：**如果删除 B 时只改 `A->next=C`，却忘了改 `C->prev=A`，向前遍历到 C 时会走向哪里？**

## User

以前B的地址

## Assistant

对，仍会沿着 `C->prev` 走到 B。这样正向遍历看似正常，反向遍历却还经过已删除的节点，所以两个方向都要更新。

接下来解释堆操作外围这几行：

```c
flags = irq_save();
spin_lock(&heap_lock);

/* 修改 slab 和空闲链表 */

spin_unlock(&heap_lock);
irq_restore(flags);
```

它们要保护的是**修改进行到一半的状态**。例如刚才插入 C 需要改四个指针，如果执行到第二步就被打断，另一个执行路径看到的链表可能还不完整。

先看 `irq_save`：

```c
uint64_t irq_save(void)
{
    uint64_t status = csr_read_sstatus();

    csr_clear_sstatus(SSTATUS_SIE);
    return status;
}
```

它做两件事：

```text
记住原来的中断状态
    ↓
关闭当前 hart 的 S-mode 中断总开关
```

然后才取得堆锁。为什么既有锁，还要先关中断？

考虑一个危险场景：

```text
内核代码取得 heap_lock
    ↓ 被中断
中断处理路径也尝试取得 heap_lock
    ↓
发现锁被占用，一直等待
```

但锁的持有者就是被打断的代码。中断处理不结束，它就没有机会继续执行并释放锁，于是卡住。

因此，需要禁止可能在本核抢占当前执行、又操作相同共享状态的中断路径。后面接上定时器抢占调度后，这一点也很重要。

这里先把自旋锁理解为：**锁被占用时，不睡眠，而是循环尝试取得它。** 原子指令如何实现锁，我们下一步再拆。

操作完成后，顺序是：

```c
spin_unlock(&heap_lock);
irq_restore(flags);
```

先释放锁，再恢复中断，避免刚恢复中断就发生上述重入问题。

`irq_restore` 的实现是：

```c
void irq_restore(uint64_t flags)
{
    if ((flags & SSTATUS_SIE) != 0)
        csr_set_sstatus(SSTATUS_SIE);
    else
        csr_clear_sstatus(SSTATUS_SIE);
}
```

注意，它虽然接收之前保存的整个 `sstatus` 值，但这里只恢复 **SIE 这一位**，不会把整个寄存器覆盖回去。

为什么不简单地无条件开启中断？因为进入 `kmalloc` 之前，中断可能本来就是关闭的：

```text
调用前中断开启 → 临时关闭 → 完成后重新开启
调用前中断关闭 → 保持关闭 → 完成后仍然关闭
```

函数不能擅自改变调用者原来的中断状态。

还有个边界：这里关的是**本 hart 的中断**，不会阻止其他 hart 同时访问内存。当前项目按单 hart 运行；多核场景还需要正确的跨核同步。

确认一个嵌套场景：**外层代码已经关闭中断，再调用 `kmalloc`，那么 `kmalloc` 结束时应该开启中断，还是保持关闭？**

## User

后者

## Assistant

对，保持关闭。`kmalloc` 只恢复进入时的状态，不替外层代码决定何时开启中断。

现在看自旋锁本身。核心状态是一个整数：

```text
locked = 0 → 没人持有锁
locked = 1 → 锁已被持有
```

能不能这样加锁？

```c
while (lock->locked != 0) {
}
lock->locked = 1;
```

问题是，“检查”和“写入”是两个独立动作。假设有两个执行者：

```text
A 读到 locked = 0
B 也读到 locked = 0
A 写入 1，认为取得锁
B 写入 1，也认为取得锁
```

于是两者都进入了临界区。锁没有实现互斥。

项目用一条**原子交换指令**解决这个问题：

```c
void spin_lock(struct spinlock *lock)
{
    uint32_t value;

    do {
        __asm__ volatile("amoswap.w.aq %0, %2, (%1)"
                         : "=r"(value)
                         : "r"(&lock->locked), "r"(1U)
                         : "memory");
    } while (value != 0);
}
```

先看指令的核心含义：

```text
原子地完成：
    取出 locked 的旧值
    将 locked 写成 1
    把旧值交给 value
```

其他执行者不能插进这个交换动作的中间。

因此：

| 交换得到的旧值 | 表示什么 | 下一步 |
|---|---|---|
| `0` | 原来没人持锁，本次成功占有 | 离开循环 |
| `1` | 别人已经持锁 | 继续尝试 |

假设 A、B 同时尝试，原子交换会让它们有先后：第一个把 `0` 换成 `1`，得到旧值 `0`；后一个只能得到旧值 `1`。

再拆一下指令名字和参数：

```asm
amoswap.w.aq %0, %2, (%1)
```

- `amoswap`：原子交换。
- `.w`：操作 32 位的锁变量。
- `.aq`：获取语义，约束后面的相关内存访问，不能越过取得锁这个边界。
- `%0`：接收旧值，对应 `value`。
- `%1`：锁变量的地址。
- `%2`：准备写入的 `1`。

这里“原子”和“访问顺序”解决的是两个不同问题：

```text
原子交换 → 谁取得锁？
获取语义 → 取得锁后，怎样正确访问被保护的数据？
```

解锁代码是：

```c
void spin_unlock(struct spinlock *lock)
{
    __asm__ volatile("fence rw, w" : : : "memory");
    lock->locked = 0;
}
```

`fence rw, w` 给前面的读写与后面的写操作建立顺序，使临界区内的操作排在释放锁的写入之前；随后写零，允许其他执行者取得锁。

例如修改 slab 链表时，我们需要保证：

```text
先完成链表数据更新
    ↓
再释放锁
    ↓
下一个持锁者查看更新后的链表
```

所以，对堆来说，加锁不仅要避免两个执行者同时改指针，还要保证交接时的数据访问顺序。

最后确认一下原子交换的判断：**`amoswap` 把锁写成 1 后，返回的旧值也是 1，这次调用者取得锁了吗？**

## User

没有，旧值是0才取得

## Assistant

对，只有读到旧值 0 的那个执行者取得锁。堆和锁的主线已经接上了；接下来看看 `kernel_main` 中的 `log_init`，用日志缓冲区再巩固一次锁的实际用途。

## Activity

    $ sed -n '1,200p' src/kernel/log.c && sed -n '1,120p' src/kernel/log.h && sed -n '1,160p' src/kernel/refcount.h && rg -n 'log_|refcount' src/kernel/m4_test.c
    #include "log.h"
    
    #include "sync.h"
    
    #define LOG_CAPACITY 1024U
    
    static char buffer[LOG_CAPACITY];
    static size_t head;
    static size_t count;
    static struct spinlock lock;
    
    void log_init(void)
    {
        head = 0;
        count = 0;
        spinlock_init(&lock);
    }
    
    void log_write(const char *data, size_t size)
    {
        uint64_t flags = irq_save();
    
        spin_lock(&lock);
        for (size_t i = 0; i < size; ++i) {
            buffer[head] = data[i];
            head = (head + 1) % LOG_CAPACITY;
            if (count < LOG_CAPACITY)
                ++count;
        }
        spin_unlock(&lock);
        irq_restore(flags);
    }
    
    size_t log_read(char *data, size_t capacity)
    {
        uint64_t flags = irq_save();
        size_t amount;
        size_t start;
    
        spin_lock(&lock);
        amount = count < capacity ? count : capacity;
        start = (head + LOG_CAPACITY - count) % LOG_CAPACITY;
        for (size_t i = 0; i < amount; ++i)
            data[i] = buffer[(start + i) % LOG_CAPACITY];
        spin_unlock(&lock);
        irq_restore(flags);
        return amount;
    }
    #ifndef MINICORE_KERNEL_LOG_H
    #define MINICORE_KERNEL_LOG_H
    
    #include <stddef.h>
    
    void log_init(void);
    void log_write(const char *data, size_t size);
    size_t log_read(char *data, size_t capacity);
    
    #endif
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
    5:#include "refcount.h"
    114:    struct refcount reference;
    123:    refcount_init(&reference);
    124:    refcount_get(&reference);
    125:    if (refcount_read(&reference) != 2 || refcount_put(&reference) ||
    126:        !refcount_put(&reference))
    148:    log_write(message, sizeof(message));
    149:    return log_read(copy, sizeof(copy)) == sizeof(copy) &&
    ✓ • 0ms

## Assistant

对，**返回旧值 0 才表示取得锁**；旧值 1 表示锁已经被占用，继续尝试。

接下来，`kernel_main` 在初始化堆之后调用：

```c
log_init();
```

这个日志模块很小，正好能把刚学过的锁用起来。它维护一个 **1024 字节的环形缓冲区**：

```c
static char buffer[1024];
static size_t head;
static size_t count;
static struct spinlock lock;
```

其中：

```text
head  = 下一个字节要写入的位置
count = 当前保留了多少字节
```

`log_init` 把两者清零，并初始化锁。这里的缓冲区是静态数组，不需要 `kmalloc`。

先看写入的核心：

```c
for (size_t i = 0; i < size; ++i) {
    buffer[head] = data[i];
    head = (head + 1) % LOG_CAPACITY;

    if (count < LOG_CAPACITY)
        ++count;
}
```

`% LOG_CAPACITY` 让写入位置到达末尾后绕回开头。缓冲区满了以后，`count` 不再增加，后续写入会覆盖最旧的数据。

为了看清楚，假设容量只有 **4 字节**，依次写入 `A B C D`：

```text
下标：    0   1   2   3
内容：    A   B   C   D
head = 0，count = 4
```

再写入 `E`：

```text
下标：    0   1   2   3
内容：    E   B   C   D
head = 1，count = 4
```

此时按时间顺序保留的是：

```text
B → C → D → E
```

所以读取时不能总从 `buffer[0]` 开始。项目这样计算最旧数据的位置：

```c
start = (head + LOG_CAPACITY - count) % LOG_CAPACITY;
```

代入刚才的例子：

```text
start = (1 + 4 - 4) % 4 = 1
```

然后从下标 1 开始，绕一圈读取：

```c
for (size_t i = 0; i < amount; ++i)
    data[i] = buffer[(start + i) % LOG_CAPACITY];
```

就得到 `B C D E`。

读取数量由两者中较小的决定：

```c
amount = count < capacity ? count : capacity;
```

也就是不能超过已有数据量，也不能超过调用者提供的目标缓冲区容量。

这里有两个接口细节：

- `log_read` **不删除日志**，没有修改 `head` 或 `count`。没有新写入时，重复读取会得到相同内容。
- 它按字节复制，不自动补 `'\0'`，返回值告诉调用者实际读了多少字节。

读和写都使用同样的保护结构：

```c
flags = irq_save();
spin_lock(&lock);

/* 读取或修改 buffer、head、count */

spin_unlock(&lock);
irq_restore(flags);
```

读取也要加锁，是因为它需要看到一致的缓冲区内容和位置。如果算出 `start` 后，写入者同时覆盖数据、推进 `head`，读到的内容就可能混杂。

另外，这个模块只是把日志保存在内存里；`log_write` 不会自动调用串口输出函数。

确认一下：**在容量为 4、已经写入 `A B C D E` 的例子里，如果调用者只提供 2 字节空间，`log_read` 会读出 `BC`，还是 `DE`？**

## User

BC

## Assistant

对，它从当前保留的最旧数据开始读，所以是 `BC`。接下来进入调度器：先看线程保存哪些信息，再走一次创建过程。

## Activity

    $ sed -n '1,255p' src/kernel/scheduler.c && sed -n '1,100p' src/arch/riscv/context.S
    #include "scheduler.h"
    
    #include "sync.h"
    #include "../arch/riscv/csr.h"
    #include "../arch/riscv/sbi.h"
    #include "../mm/heap.h"
    #include "../mm/layout.h"
    #include "../mm/page_alloc.h"
    #include "../runtime.h"
    
    enum thread_state {
        THREAD_RUNNING,
        THREAD_RUNNABLE,
        THREAD_SLEEPING,
        THREAD_DEAD,
    };
    
    struct context {
        uint64_t ra;
        uint64_t sp;
        uint64_t saved[12];
    };
    
    struct thread {
        struct context context;
        struct list_node all_link;
        struct list_node wait_link;
        enum thread_state state;
        thread_entry entry;
        void *argument;
        uint64_t stack_page;
        uint64_t wake_tick;
        const char *name;
        bool idle;
    };
    
    extern void context_switch(struct context *old, struct context *next);
    
    static LIST_HEAD(all_threads);
    static struct thread bootstrap;
    static struct thread *current;
    static struct thread *idle_thread;
    static uint64_t tick_count;
    static uint64_t preemption_count;
    static uint64_t timer_interval;
    static bool timer_running;
    
    static _Noreturn void thread_exit(void);
    
    static void thread_trampoline(void)
    {
        csr_set_sstatus(SSTATUS_SIE);
        current->entry(current->argument);
        thread_exit();
    }
    
    static void idle_main(void *argument)
    {
        (void)argument;
        for (;;) {
            __asm__ volatile("wfi");
            scheduler_yield();
        }
    }
    
    static struct thread *pick_next(void)
    {
        struct list_node *start = current->all_link.next;
        struct list_node *node = start;
    
        do {
            if (node == &all_threads)
                node = node->next;
            if (node != &all_threads) {
                struct thread *thread = container_of(node, struct thread,
                                                      all_link);
    
                if (thread->state == THREAD_RUNNABLE && !thread->idle)
                    return thread;
                node = node->next;
            }
        } while (node != start);
        if (current->state == THREAD_RUNNABLE && !current->idle)
            return current;
        return idle_thread;
    }
    
    void scheduler_switch(void)
    {
        struct thread *previous = current;
        struct thread *next = pick_next();
    
        if (!next || next == previous) {
            if (previous->state == THREAD_RUNNABLE)
                previous->state = THREAD_RUNNING;
            return;
        }
        next->state = THREAD_RUNNING;
        current = next;
        context_switch(&previous->context, &next->context);
    }
    
    static bool create_thread(const char *name, thread_entry entry,
                              void *argument, bool idle)
    {
        struct thread *thread = kmalloc(sizeof(*thread));
        uint64_t stack;
    
        if (!thread)
            return false;
        stack = page_alloc();
        if (!stack) {
            kfree(thread);
            return false;
        }
        memset(thread, 0, sizeof(*thread));
        list_init(&thread->all_link);
        list_init(&thread->wait_link);
        thread->state = THREAD_RUNNABLE;
        thread->entry = entry;
        thread->argument = argument;
        thread->stack_page = stack;
        thread->name = name;
        thread->idle = idle;
        thread->context.ra = (uintptr_t)thread_trampoline;
        thread->context.sp = (uintptr_t)phys_to_virt(stack) + PAGE_SIZE;
        list_push_back(&all_threads, &thread->all_link);
        if (idle)
            idle_thread = thread;
        return true;
    }
    
    bool scheduler_init(uint64_t timebase_frequency)
    {
        memset(&bootstrap, 0, sizeof(bootstrap));
        list_init(&all_threads);
        list_init(&bootstrap.all_link);
        list_init(&bootstrap.wait_link);
        bootstrap.state = THREAD_RUNNING;
        bootstrap.name = "bootstrap";
        current = &bootstrap;
        list_push_back(&all_threads, &bootstrap.all_link);
        tick_count = 0;
        preemption_count = 0;
        timer_interval = timebase_frequency / 200;
        if (timer_interval == 0)
            timer_interval = 1;
        if (!create_thread("idle", idle_main, 0, true))
            return false;
        timer_running = true;
        sbi_set_timer(csr_read_time() + timer_interval);
        csr_set_sie(SIE_STIE);
        csr_set_sstatus(SSTATUS_SIE);
        return true;
    }
    
    bool thread_create(const char *name, thread_entry entry, void *argument)
    {
        uint64_t flags;
        bool result;
    
        if (!entry)
            return false;
        flags = irq_save();
        result = create_thread(name, entry, argument, false);
        irq_restore(flags);
        return result;
    }
    
    void scheduler_yield(void)
    {
        uint64_t flags = irq_save();
    
        if (current->state == THREAD_RUNNING)
            current->state = THREAD_RUNNABLE;
        scheduler_switch();
        irq_restore(flags);
    }
    
    void scheduler_sleep(uint64_t ticks)
    {
        uint64_t flags;
    
        if (ticks == 0) {
            scheduler_yield();
            return;
        }
        flags = irq_save();
        current->wake_tick = tick_count + ticks;
        current->state = THREAD_SLEEPING;
        scheduler_switch();
        irq_restore(flags);
    }
    
    void scheduler_block_current(struct list_node *queue)
    {
        current->state = THREAD_SLEEPING;
        list_push_back(queue, &current->wait_link);
    }
    
    void scheduler_wake_node(struct list_node *node)
    {
        struct thread *thread = container_of(node, struct thread, wait_link);
    
        list_remove(node);
        if (thread->state == THREAD_SLEEPING)
            thread->state = THREAD_RUNNABLE;
    }
    
    void scheduler_tick(void)
    {
        ++tick_count;
        for (struct list_node *node = all_threads.next; node != &all_threads;
             node = node->next) {
            struct thread *thread = container_of(node, struct thread, all_link);
    
            if (thread->state == THREAD_SLEEPING && thread->wake_tick != 0 &&
                thread->wake_tick <= tick_count) {
                thread->wake_tick = 0;
                thread->state = THREAD_RUNNABLE;
            }
        }
        if (timer_running)
            sbi_set_timer(csr_read_time() + timer_interval);
        if (current->state == THREAD_RUNNING) {
            current->state = THREAD_RUNNABLE;
            ++preemption_count;
        }
        scheduler_switch();
    }
    
    bool scheduler_timer_active(void)
    {
        return timer_running;
    }
    
    uint64_t scheduler_ticks(void)
    {
        return tick_count;
    }
    
    uint64_t scheduler_preemptions(void)
    {
        return preemption_count;
    }
    
    void scheduler_stop_timer(void)
    {
        uint64_t flags = irq_save();
    
        timer_running = false;
        csr_clear_sie(SIE_STIE);
        sbi_set_timer(UINT64_MAX);
        irq_restore(flags);
    }
    #define CTX_RA 0
    #define CTX_SP 8
    #define CTX_S0 16
    
        .section .text, "ax", @progbits
        .align 2
        .globl context_switch
        .type context_switch, @function
    context_switch:
        sd ra, CTX_RA(a0)
        sd sp, CTX_SP(a0)
        sd s0, CTX_S0 + 0*8(a0)
        sd s1, CTX_S0 + 1*8(a0)
        sd s2, CTX_S0 + 2*8(a0)
        sd s3, CTX_S0 + 3*8(a0)
        sd s4, CTX_S0 + 4*8(a0)
        sd s5, CTX_S0 + 5*8(a0)
        sd s6, CTX_S0 + 6*8(a0)
        sd s7, CTX_S0 + 7*8(a0)
        sd s8, CTX_S0 + 8*8(a0)
        sd s9, CTX_S0 + 9*8(a0)
        sd s10, CTX_S0 + 10*8(a0)
        sd s11, CTX_S0 + 11*8(a0)
    
        ld ra, CTX_RA(a1)
        ld sp, CTX_SP(a1)
        ld s0, CTX_S0 + 0*8(a1)
        ld s1, CTX_S0 + 1*8(a1)
        ld s2, CTX_S0 + 2*8(a1)
        ld s3, CTX_S0 + 3*8(a1)
        ld s4, CTX_S0 + 4*8(a1)
        ld s5, CTX_S0 + 5*8(a1)
        ld s6, CTX_S0 + 6*8(a1)
        ld s7, CTX_S0 + 7*8(a1)
        ld s8, CTX_S0 + 8*8(a1)
        ld s9, CTX_S0 + 9*8(a1)
        ld s10, CTX_S0 + 10*8(a1)
        ld s11, CTX_S0 + 11*8(a1)
        ret
        .size context_switch, . - context_switch
    
        .globl scheduler_register_probe
        .type scheduler_register_probe, @function
    scheduler_register_probe:
        addi sp, sp, -112
        sd ra, 0(sp)
        sd s0, 8(sp)
        sd s1, 16(sp)
        sd s2, 24(sp)
        sd s3, 32(sp)
        sd s4, 40(sp)
        sd s5, 48(sp)
        sd s6, 56(sp)
        sd s7, 64(sp)
        sd s8, 72(sp)
        sd s9, 80(sp)
        sd s10, 88(sp)
        sd s11, 96(sp)
        sd a0, 104(sp)
        li s0, 0x20
        li s1, 0x21
        li s2, 0x22
        li s3, 0x23
        li s4, 0x24
        li s5, 0x25
        li s6, 0x26
        li s7, 0x27
        li s8, 0x28
        li s9, 0x29
        li s10, 0x2a
        li s11, 0x2b
    .Lprobe_loop:
        call scheduler_yield
        ld t0, 104(sp)
        addi t0, t0, -1
        sd t0, 104(sp)
        bnez t0, .Lprobe_loop
        li a0, 0
        li t0, 0x20
        bne s0, t0, .Lprobe_done
        li t0, 0x21
        bne s1, t0, .Lprobe_done
        li t0, 0x22
        bne s2, t0, .Lprobe_done
        li t0, 0x23
        bne s3, t0, .Lprobe_done
        li t0, 0x24
        bne s4, t0, .Lprobe_done
        li t0, 0x25
        bne s5, t0, .Lprobe_done
        li t0, 0x26
        bne s6, t0, .Lprobe_done
        li t0, 0x27
        bne s7, t0, .Lprobe_done
        li t0, 0x28
        bne s8, t0, .Lprobe_done
        li t0, 0x29
        bne s9, t0, .Lprobe_done
        li t0, 0x2a
        bne s10, t0, .Lprobe_done
    ✓ • 0ms

## Assistant

对，读出的是 `BC`，而且读取后它们仍保留在日志里。

接下来进入 `scheduler_init`。我们先解决一个核心问题：**线程暂停以后，内核需要记住什么，才能让它继续运行？**

想象两个内核线程：

```c
void worker_a(void *arg) { /* 执行任务 A */ }
void worker_b(void *arg) { /* 执行任务 B */ }
```

单核同一时刻只能执行其中一个。切换线程时，至少需要保存：

```text
继续执行的位置
当前栈的位置
恢复执行所需的寄存器值
```

项目用 `context` 表示这部分状态：

```c
struct context {
    uint64_t ra;
    uint64_t sp;
    uint64_t saved[12];  // s0～s11
};
```

你可能会发现：它比之前的 `trap_frame` 小得多。两者用途不同：

| 结构 | 用途 |
|---|---|
| `trap_frame` | 保存被异常或中断打断的现场 |
| `context` | 在调度器的函数调用边界切换线程 |

先记住这个区别。等读切换汇编时，我们会解释为什么这里不保存 `a0～a7` 等所有寄存器，以及抢占时它们保存在哪里。

`context` 又嵌在完整的线程对象中：

```c
struct thread {
    struct context context;
    struct list_node all_link;
    struct list_node wait_link;
    enum thread_state state;
    thread_entry entry;
    void *argument;
    uint64_t stack_page;
    uint64_t wake_tick;
    const char *name;
    bool idle;
};
```

把它按用途读，就容易理解：

```text
context          → 如何恢复运行
entry、argument  → 首次运行时执行哪个函数、传什么参数
stack_page       → 线程栈所在的物理页
state、wake_tick → 当前状态、何时唤醒
all_link         → 接入所有线程的链表
wait_link        → 接入等待队列
name、idle       → 名称、是否为空闲线程
```

同一个线程放了两个链表节点，是因为它可以同时存在于“所有线程链表”和“某个等待队列”中。两条链表需要各自独立的前后指针。

线程状态有四种：

```c
THREAD_RUNNING   // 正在执行
THREAD_RUNNABLE  // 可以执行，等待调度
THREAD_SLEEPING  // 暂时不能执行，等待时间或事件
THREAD_DEAD      // 已退出，等待回收
```

初始化时，内核先把**当前正在执行启动代码的这条执行流**登记成线程：

```c
bootstrap.state = THREAD_RUNNING;
bootstrap.name = "bootstrap";
current = &bootstrap;
list_push_back(&all_threads, &bootstrap.all_link);
```

它叫 `bootstrap`，继续使用我们之前准备的启动栈，不需要在这里新分配一张栈。

然后创建 `idle` 线程。它负责在没有普通线程可运行时接手 CPU，主要循环是：

```c
for (;;) {
    __asm__ volatile("wfi");
    scheduler_yield();
}
```

它等待事件，然后再次尝试让出 CPU。

创建新线程时，内核需要两种分配：

```c
struct thread *thread = kmalloc(sizeof(*thread));
uint64_t stack = page_alloc();
```

- 堆分配线程管理对象。
- 页分配器提供一整页，作为这个线程自己的栈。

如果栈分配失败，代码会 `kfree(thread)`，归还已经申请的管理对象。

随后给新线程准备初始上下文：

```c
thread->state = THREAD_RUNNABLE;
thread->entry = entry;
thread->argument = argument;

thread->context.ra = (uintptr_t)thread_trampoline;
thread->context.sp = (uintptr_t)phys_to_virt(stack) + PAGE_SIZE;
```

这里有个关键设计：**新线程从没运行过，所以没有可以恢复的旧现场。内核人为准备一份初始现场，让第一次切换进去时从 `thread_trampoline` 开始执行。**

它的 `sp` 指向新栈顶；`ra` 则预先指向启动包装函数。下一步的切换汇编会把这两个值装进 CPU。

确认一下：**新线程被创建并加入链表，状态是 `RUNNABLE`，这是否意味着它的入口函数已经开始执行了？**

## User

no,只是可被调度

## Assistant

对，`RUNNABLE` 只是“准备好，可以被调度”，还没有执行入口函数。

现在看一次真实切换：**线程 A 主动让出 CPU，让新线程 B 运行。**

A 调用：

```c
scheduler_yield();
```

它的实现是：

```c
void scheduler_yield(void)
{
    uint64_t flags = irq_save();

    if (current->state == THREAD_RUNNING)
        current->state = THREAD_RUNNABLE;

    scheduler_switch();
    irq_restore(flags);
}
```

先关本地中断，避免切换过程被中断打断；再把 A 从“正在运行”改成“可以运行”。A 只是让出这次机会，没有睡眠，后面仍可再次被选中。

接着进入：

```c
void scheduler_switch(void)
{
    struct thread *previous = current;
    struct thread *next = pick_next();

    if (!next || next == previous) {
        if (previous->state == THREAD_RUNNABLE)
            previous->state = THREAD_RUNNING;
        return;
    }

    next->state = THREAD_RUNNING;
    current = next;
    context_switch(&previous->context, &next->context);
}
```

假设 `pick_next()` 选中了 B：

```text
previous = A
next = B
B.state = RUNNING
current = B
```

注意：**修改 `current` 只是修改软件记录，CPU 此时还在 A 的栈上执行。** 真正的切换发生在下一句：

```c
context_switch(&A->context, &B->context);
```

按照调用约定：

```text
a0 = A 的 context 地址
a1 = B 的 context 地址
```

汇编先保存 A：

```asm
sd ra, 0(a0)
sd sp, 8(a0)
sd s0, 16(a0)
sd s1, 24(a0)
...
```

这里保存的 `ra`，就是本次 `call context_switch` 后面的继续执行位置。

然后装入 B：

```asm
ld ra, 0(a1)
ld sp, 8(a1)
ld s0, 16(a1)
ld s1, 24(a1)
...
ret
```

**最后这条 `ret` 使用的是刚装入的 B 的 `ra`，而不是 A 的返回地址。**

对于第一次运行的 B，我们预先设置了：

```text
B.context.ra = thread_trampoline
B.context.sp = B 的栈顶
```

所以 `ret` 直接进入：

```c
static void thread_trampoline(void)
{
    csr_set_sstatus(SSTATUS_SIE);
    current->entry(current->argument);
    thread_exit();
}
```

此时 `current` 已经指向 B，因此它会：

1. 开启中断。
2. 调用 B 的入口函数，传入 B 的参数。
3. 入口函数返回后，调用 `thread_exit` 处理退出。

这就是新线程第一次运行的全过程。

再解释为什么 `context_switch` 只保存 `s0～s11`、`sp` 和 `ra`。

它发生在**普通函数调用边界**。按照调用约定，调用者不能指望 `a0～a7`、`t0～t6` 在函数调用后保持不变；需要跨调用保留的值，编译器会放到栈上或需要保留的寄存器中。因此，这套切换代码保存相应寄存器，再保留每个线程自己的栈，就能恢复这次调用。

而抢占可以发生在任意指令之间，没有这个前提。所以抢占先经过 `trap_entry`，将完整整数现场保存在当前线程的内核栈上，再进入调度器。后面我们会把这两层连起来。

现在 A 暂停在哪里？

```text
A 的 scheduler_yield
    → A 的 scheduler_switch
        → context_switch 保存 A
            → 切到 B
```

A 的调用栈还在原来的内存中，没有被清空。等未来切回 A，恢复它的 `sp` 和 `ra`，就能从这次 `context_switch` 调用之后继续。

确认一下：**A 调用 `scheduler_yield()` 后面的代码，是 B 一开始运行就立刻由 A 执行，还是要等调度器以后再次选中 A，才会继续？**

## User

没看懂你的问题

## Assistant

我刚才把调用层次讲得太绕了。我们只看 A 的代码：

```c
void worker_a(void *arg)
{
    console_puts("A1\n");
    scheduler_yield();
    console_puts("A2\n");
}
```

假设执行 `scheduler_yield()` 时，调度器选中了 B，那么顺序是：

```text
A 打印 A1
    ↓
A 调用 scheduler_yield，切换到 B
    ↓
B 开始运行，A 暂停
    ↓
以后调度器再次选中 A
    ↓
A 从暂停处继续，完成 scheduler_yield 的返回
    ↓
A 打印 A2
```

**切换到 B 以后，A 不会同时往下执行。** 当前是单核，CPU 正在执行 B 的代码。

我原本想问的是：“A2 什么时候打印？”答案就是：**等 A 再次获得 CPU，且 `scheduler_yield()` 返回之后。**

从 A 自己的视角看，它只是调用了一个函数，然后这个函数过了一会儿才返回；中间那段时间，CPU 去运行其他线程了。

## User

继续

## Assistant

我们接着看：**B 怎样把 CPU 交还给 A，而 A 又怎样找到原来暂停的位置。**

假设 A、B 都会主动让出 CPU：

```c
// 线程 A
console_puts("A1\n");
scheduler_yield();
console_puts("A2\n");

// 线程 B
console_puts("B1\n");
scheduler_yield();
console_puts("B2\n");
```

为了看清过程，假设这两次调度分别选中 B、A。

**第一次切换：A → B。**

A 调用到：

```c
context_switch(&A->context, &B->context);
```

汇编把 A 的状态保存进 `A->context`：

```text
A.context.sp = A 此时的栈位置
A.context.ra = A 这次 context_switch 调用后的继续位置
A.context.saved = A 的 s0～s11
```

然后装入 B 的状态。B 开始运行，打印 `B1`。

A 的栈仍然留在内存里，包括尚未返回的这些调用：

```text
worker_a
  → scheduler_yield
    → scheduler_switch
```

**第二次切换：B → A。**

B 也调用 `scheduler_yield()`，调度器这次选中 A，执行：

```c
context_switch(&B->context, &A->context);
```

汇编先保存 B，再读取先前保存的 A：

```asm
ld ra, 0(a1)       # A 的继续位置
ld sp, 8(a1)       # A 的栈位置
...
ret
```

这一条 `ret` 让 A 从它上一次 `context_switch` 调用之后继续执行。

之后按普通函数返回顺序，一层层回来：

```text
A 的 context_switch 返回
    ↓
A 的 scheduler_switch 返回
    ↓
A 的 scheduler_yield 恢复原来的中断状态，然后返回
    ↓
A 的 worker_a 继续，打印 A2
```

这里最值得理解的是：**同一份函数代码，可以对应不同线程各自的调用现场。**

A 和 B 都调用 `scheduler_yield`，但各自的局部变量、返回地址等在各自的栈里。例如：

```c
uint64_t flags = irq_save();
```

A 恢复时使用的是 A 那次调用保存的 `flags`，不会变成 B 的。

现在再看选择下一个线程的 `pick_next()`。它从：

```c
current->all_link.next
```

开始，沿所有线程的链表向后寻找：

```c
if (thread->state == THREAD_RUNNABLE && !thread->idle)
    return thread;
```

即选择下一个“可运行、且不是 idle”的线程；遇到链表头就绕过去继续找。这是一种简单的轮转选择。

如果没有其他合适的普通线程，而当前线程自己仍可运行，就可以继续选它。`scheduler_switch` 会发现：

```c
next == previous
```

于是把状态恢复成 `RUNNING`，直接返回，不执行寄存器切换。

因此，**调用 `scheduler_yield()` 不保证一定切到别的线程，只是给调度器一次重新选择的机会。**

如果没有普通线程可运行，才选择 idle 线程等待事件。

我们用一个小场景确认：**A 调用 `scheduler_yield()` 时，B 正在睡眠，而且没有其他可运行的普通线程，那么 A 可以被再次选中、继续运行吗？**

## User

可以

## Assistant

对，A 仍然可运行，调度器可以继续选择它。

接下来把 `yield` 和 **sleep** 区分开。项目的睡眠函数是：

```c
void scheduler_sleep(uint64_t ticks)
{
    uint64_t flags;

    if (ticks == 0) {
        scheduler_yield();
        return;
    }

    flags = irq_save();
    current->wake_tick = tick_count + ticks;
    current->state = THREAD_SLEEPING;
    scheduler_switch();
    irq_restore(flags);
}
```

假设 A 调用：

```c
scheduler_sleep(3);
```

此时全局计数是：

```text
tick_count = 10
```

函数先记录：

```text
A.wake_tick = 10 + 3 = 13
A.state = THREAD_SLEEPING
```

然后调用 `scheduler_switch()`。

这次和 `yield` 不同：**A 已经不是 `RUNNABLE`，所以调度器不能马上重新选中它。** 必须等待其他代码把它变回可运行状态。

如果 B 可运行，就切换到 B；如果没有普通线程可运行，就切换到 idle。

你可能会问：A 已经停下来了，谁负责让它醒过来？

答案是**定时器中断处理路径**。调度器启动后，之前的 `handle_timer` 会走这个分支：

```c
if (scheduler_timer_active()) {
    scheduler_tick();
    return;
}
```

`scheduler_tick` 先增加计数：

```c
++tick_count;
```

然后遍历线程，检查：

```c
if (thread->state == THREAD_SLEEPING &&
    thread->wake_tick != 0 &&
    thread->wake_tick <= tick_count) {

    thread->wake_tick = 0;
    thread->state = THREAD_RUNNABLE;
}
```

代入 A 的例子：

| 当前 `tick_count` | A 的状态 |
|---:|---|
| 10 | 调用 sleep，设置在 13 唤醒 |
| 11 | 仍是 `SLEEPING` |
| 12 | 仍是 `SLEEPING` |
| 13 | 改为 `RUNNABLE`，清除 `wake_tick` |

注意：**唤醒只表示可以被调度，不等于这一刻一定已经开始执行。** A 还要等调度器实际选中它。

等 A 被选中，它像前面一样从暂停的 `scheduler_switch` 调用之后恢复，接着：

```c
irq_restore(flags);
```

然后 `scheduler_sleep(3)` 返回，A 继续执行下一行。

整个过程是：

```text
A 设置唤醒时刻，进入 SLEEPING
    ↓
切走，其他线程或 idle 运行
    ↓
定时器计数到达目标，A 变成 RUNNABLE
    ↓
调度器选中 A
    ↓
A 的 scheduler_sleep 返回
```

因此，它不是让 A 自己循环查询时间。A 睡眠期间不占着 CPU 做等待检查。

这里的 `ticks` 是调度器的中断计数单位，不是直接传入毫秒。项目设置的调度定时器间隔约为 5 ms，但按计数唤醒不等于精确的墙钟时间保证。

两种操作的区别可以记成：

```text
yield：我还可以运行，只是让出一次选择机会。
sleep：条件到来前，不要选择我运行。
```

确认一下：**如果 A 和 B 都在睡眠，CPU 去执行 idle，之后定时器中断还能把到期的 A 改成 `RUNNABLE` 吗？**

## User

可以

## Assistant

对。idle 只是当前运行的线程，定时器中断仍然可以到来，唤醒到期的线程并触发调度。

现在进一步：**如果 A 一直计算，既不调用 `yield`，也不调用 `sleep`，B 还能获得 CPU 吗？**

可以，靠的是**定时器抢占**。

之前只看了 `scheduler_tick` 的唤醒部分，它后面还有：

```c
if (timer_running)
    sbi_set_timer(csr_read_time() + timer_interval);

if (current->state == THREAD_RUNNING) {
    current->state = THREAD_RUNNABLE;
    ++preemption_count;
}

scheduler_switch();
```

它先预约下一次中断，再把当前线程改为 `RUNNABLE`，然后重新选择线程。假设 A 被打断，调度器选中了 B，就发生了：

```text
A 正在计算
    ↓ 定时器中断
trap_entry 保存 A 的现场
    ↓
trap_dispatch → handle_timer → scheduler_tick
    ↓
A 改为 RUNNABLE，调度器选中 B
    ↓
context_switch：保存 A 的调度上下文，装入 B
    ↓
B 开始或恢复运行
```

这里出现了两次“保存”，必须区分清楚。

**第一次：保存 A 被中断那一刻的现场。**

由 `trap_entry` 完成，保存在 **A 的内核栈**上：

```text
trap_frame：
    A 原来的 a0、t0、ra 等整数寄存器
    A 被打断的位置 sepc
    异常和状态信息
```

**第二次：保存“处理中断途中”的函数调用现场。**

由 `context_switch` 完成，保存在 **A 的线程对象**里：

```text
A.context：
    此刻的 sp
    context_switch 调用后的继续位置 ra
    此刻的 s0～s11
```

第二次保存的 `ra` 不是 A 原先计算代码的位置，而是“以后回来继续完成调度和中断处理”的位置。

A 暂停时，其栈上仍保留着这些内容：

```text
高地址
┌─────────────────────────┐
│ A 原先执行计算的调用栈  │
├─────────────────────────┤
│ trap_frame：被打断现场  │
├─────────────────────────┤
│ 中断处理与调度调用的栈帧│
└─────────────────────────┘
低地址
           ↑
      A.context.sp
```

等以后切回 A，恢复顺序也是两层：

```text
装入 A.context
    ↓
从 A 的 context_switch 返回
    ↓
继续返回 scheduler_tick、handle_timer、trap_dispatch
    ↓
回到 trap_entry 的汇编恢复部分
    ↓
从 A 栈上的 trap_frame 恢复完整现场
    ↓
sret
    ↓
A 回到被定时器打断的计算位置
```

这就解答了前面留下的问题：**`context_switch` 没保存 `a0`、`t0` 等寄存器，为什么抢占也不会丢失它们？**

因为抢占发生时，这些寄存器已经由更外层的 `trap_entry` 保存到了 A 的栈上。调度切换只需要让我们将来能回到这条尚未完成的中断处理路径。

从 A 原来的计算代码看，它没有主动让出 CPU，也不需要为这次暂停专门写保存代码。

最后注意：`preemption_count` 在这里记录的是定时器处理中的这次调度动作，不保证一定切换到了另一个线程；如果只有 A 可运行，仍然可能继续选择 A。

确认这两层的位置：**A 被打断时原来的 `t0`，保存在 A 的 `context` 里，还是 A 栈上的 `trap_frame` 里？**

## User

后者

## Assistant

对，原来的 `t0` 保存在 A 栈上的 `trap_frame` 中。接下来把线程生命周期补齐：入口函数执行完后怎样退出，以及为什么线程不能立刻释放自己的栈。

## Activity

    $ sed -n '250,360p' src/kernel/scheduler.c && rg -n 'scheduler_reap|thread_exit|thread_create' src/kernel/m4_test.c src/kernel/user.c
    
        timer_running = false;
        csr_clear_sie(SIE_STIE);
        sbi_set_timer(UINT64_MAX);
        irq_restore(flags);
    }
    
    static _Noreturn void thread_exit(void)
    {
        irq_save();
        current->state = THREAD_DEAD;
        scheduler_switch();
        for (;;)
            __asm__ volatile("wfi");
    }
    
    void scheduler_reap(void)
    {
        uint64_t flags = irq_save();
        struct list_node *node = all_threads.next;
    
        while (node != &all_threads) {
            struct thread *thread = container_of(node, struct thread, all_link);
    
            node = node->next;
            if (thread != current && !thread->idle &&
                thread->state == THREAD_DEAD) {
                list_remove(&thread->all_link);
                page_free(thread->stack_page);
                kfree(thread);
            }
        }
        irq_restore(flags);
    }
    src/kernel/m4_test.c:162:    if (!thread_create("register-a", register_worker, (void *)0) ||
    src/kernel/m4_test.c:163:        !thread_create("register-b", register_worker, (void *)1))
    src/kernel/m4_test.c:167:    scheduler_reap();
    src/kernel/m4_test.c:173:    if (!thread_create("preempt-a", preempt_worker_a, 0) ||
    src/kernel/m4_test.c:174:        !thread_create("preempt-b", preempt_worker_b, 0))
    src/kernel/m4_test.c:180:    scheduler_reap();
    src/kernel/m4_test.c:185:    if (!thread_create("sleeper", sleep_worker, 0))
    src/kernel/m4_test.c:191:    scheduler_reap();
    src/kernel/m4_test.c:198:    if (!thread_create("producer", producer, &pipe_test) ||
    src/kernel/m4_test.c:199:        !thread_create("consumer", consumer, &pipe_test))
    src/kernel/m4_test.c:203:    scheduler_reap();
    ✓ • 0ms

## Assistant

对，保存在 A 栈上的 `trap_frame` 中。调度器恢复 A 后，最终由 trap 返回代码把它恢复到 CPU。

现在把线程生命周期的最后一段补上：**线程函数执行完后，怎样退出并归还内存？**

还记得新线程的入口包装函数吗？

```c
static void thread_trampoline(void)
{
    csr_set_sstatus(SSTATUS_SIE);
    current->entry(current->argument);
    thread_exit();
}
```

线程实际入口 `entry` 返回后，就进入 `thread_exit`：

```c
static _Noreturn void thread_exit(void)
{
    irq_save();
    current->state = THREAD_DEAD;
    scheduler_switch();

    for (;;)
        __asm__ volatile("wfi");
}
```

它做三件事：

```text
关闭本地中断
    ↓
把自己标记为 DEAD
    ↓
切换到另一个线程
```

调度器不会再选择 `DEAD` 线程，所以正常情况下，它不会从这次 `scheduler_switch` 返回。后面的无限循环只是兜底。

这里没有恢复原来的中断状态，因为这个线程不打算恢复执行。接手的线程会通过自己的恢复路径或初次入口处理相应的中断状态。

你会发现：**`thread_exit` 没有调用 `page_free` 或 `kfree`。**

为什么不能直接释放自己的栈？

因为它此时还在用这张栈：

```text
thread_exit 正在栈上运行
    → 调用 scheduler_switch
    → 调用 context_switch
```

如果提前把栈归还，页分配器会把它视为空闲页，后续分配可能覆盖其中仍在使用的调用现场。线程对象也不能提前释放，因为切换还要把上下文写进它的 `context`。

因此项目分成两步：

```text
退出：标记 DEAD，切走
回收：由另一个线程释放它的资源
```

负责回收的是：

```c
void scheduler_reap(void)
{
    uint64_t flags = irq_save();
    struct list_node *node = all_threads.next;

    while (node != &all_threads) {
        struct thread *thread =
            container_of(node, struct thread, all_link);

        node = node->next;

        if (thread != current && !thread->idle &&
            thread->state == THREAD_DEAD) {
            list_remove(&thread->all_link);
            page_free(thread->stack_page);
            kfree(thread);
        }
    }

    irq_restore(flags);
}
```

它只回收满足这些条件的线程：

- 不是当前线程。
- 不是 idle 线程。
- 已经处于 `DEAD` 状态。

实际回收顺序是：

```text
从线程链表摘除
    → 归还线程的物理栈页
    → 归还 thread 管理对象
```

在当前项目中，测试代码会显式调用 `scheduler_reap()`。因此，标记 `DEAD` 不意味着内存立即归还。

还有一个很实用的遍历细节：

```c
node = node->next;
```

这句放在删除之前。它先记住下一个节点，之后才能安全删除、释放当前对象。否则释放 `thread` 后再读取它里面的 `all_link.next`，就是访问已经归还的内存。

现在一条线程的主要生命周期完整了：

```text
创建 → RUNNABLE → RUNNING
          ↑          │
          ├─ yield ──┤
          ├─ 唤醒 ← SLEEPING
                     │
                  执行结束
                     ↓
                    DEAD
                     ↓
              scheduler_reap 回收
```

确认一下：**一个线程已经是 `DEAD`，但还没有执行 `scheduler_reap`，它的栈页是否已经回到空闲页链表？**

## User

否

## Assistant

对，`DEAD` 只是不能再运行，栈页要等回收函数处理。接下来看看线程如何等待“某个条件成立”，用项目里的生产者和消费者测试作例子。

## Activity

    $ sed -n '1,115p' src/kernel/m4_test.c && sed -n '50,120p' src/kernel/sync.c && sed -n '1,110p' src/kernel/sync.h
    #include "m4_test.h"
    
    #include "list.h"
    #include "log.h"
    #include "refcount.h"
    #include "scheduler.h"
    #include "sync.h"
    #include "../mm/heap.h"
    #include "../mm/page_alloc.h"
    #include "../runtime.h"
    #include "../platform/platform.h"
    
    static volatile bool register_results[2];
    static volatile uint64_t preempt_a;
    static volatile uint64_t preempt_b;
    static volatile bool sleep_done;
    static volatile uint64_t sleep_elapsed;
    
    struct pipe_test {
        struct spinlock lock;
        struct wait_queue readable;
        struct wait_queue writable;
        unsigned data[4];
        size_t read;
        size_t write;
        size_t count;
        unsigned produced_sum;
        unsigned consumed_sum;
        volatile bool producer_done;
        volatile bool consumer_done;
    };
    
    static struct pipe_test pipe_test;
    
    static void register_worker(void *argument)
    {
        uintptr_t index = (uintptr_t)argument;
    
        register_results[index] = scheduler_register_probe(32);
    }
    
    static void preempt_worker_a(void *argument)
    {
        (void)argument;
        while (preempt_b == 0)
            ++preempt_a;
    }
    
    static void preempt_worker_b(void *argument)
    {
        (void)argument;
        while (preempt_a == 0)
            ;
        ++preempt_b;
    }
    
    static void sleep_worker(void *argument)
    {
        uint64_t before = scheduler_ticks();
    
        (void)argument;
        scheduler_sleep(4);
        sleep_elapsed = scheduler_ticks() - before;
        sleep_done = true;
    }
    
    static void producer(void *argument)
    {
        struct pipe_test *pipe = argument;
    
        for (unsigned value = 1; value <= 64; ++value) {
            uint64_t flags = irq_save();
    
            spin_lock(&pipe->lock);
            while (pipe->count == 4)
                wait_queue_sleep(&pipe->writable, &pipe->lock);
            pipe->data[pipe->write] = value;
            pipe->write = (pipe->write + 1) % 4;
            ++pipe->count;
            pipe->produced_sum += value;
            wait_queue_wake_one(&pipe->readable);
            spin_unlock(&pipe->lock);
            irq_restore(flags);
            if ((value & 3U) == 0)
                scheduler_yield();
        }
        pipe->producer_done = true;
    }
    
    static void consumer(void *argument)
    {
        struct pipe_test *pipe = argument;
    
        for (unsigned i = 0; i < 64; ++i) {
            uint64_t flags = irq_save();
            unsigned value;
    
            spin_lock(&pipe->lock);
            while (pipe->count == 0)
                wait_queue_sleep(&pipe->readable, &pipe->lock);
            value = pipe->data[pipe->read];
            pipe->read = (pipe->read + 1) % 4;
            --pipe->count;
            pipe->consumed_sum += value;
            wait_queue_wake_one(&pipe->writable);
            spin_unlock(&pipe->lock);
            irq_restore(flags);
        }
        pipe->consumer_done = true;
    }
    
    static bool object_tests(void)
    {
        struct refcount reference;
        LIST_HEAD(head);
    void wait_queue_init(struct wait_queue *queue)
    {
        list_init(&queue->sleepers);
    }
    
    void wait_queue_sleep(struct wait_queue *queue, struct spinlock *lock)
    {
        scheduler_block_current(&queue->sleepers);
        spin_unlock(lock);
        scheduler_switch();
        spin_lock(lock);
    }
    
    bool wait_queue_wake_one(struct wait_queue *queue)
    {
        if (list_empty(&queue->sleepers))
            return false;
        scheduler_wake_node(queue->sleepers.next);
        return true;
    }
    
    void wait_queue_wake_all(struct wait_queue *queue)
    {
        while (wait_queue_wake_one(queue))
            ;
    }
    #ifndef MINICORE_KERNEL_SYNC_H
    #define MINICORE_KERNEL_SYNC_H
    
    #include <stdbool.h>
    #include <stdint.h>
    
    #include "list.h"
    
    struct spinlock {
        volatile uint32_t locked;
    };
    
    struct wait_queue {
        struct list_node sleepers;
    };
    
    #define SPINLOCK_INITIALIZER { 0 }
    
    uint64_t irq_save(void);
    void irq_restore(uint64_t flags);
    bool irq_enabled(void);
    void spinlock_init(struct spinlock *lock);
    void spin_lock(struct spinlock *lock);
    void spin_unlock(struct spinlock *lock);
    void wait_queue_init(struct wait_queue *queue);
    void wait_queue_sleep(struct wait_queue *queue, struct spinlock *lock);
    bool wait_queue_wake_one(struct wait_queue *queue);
    void wait_queue_wake_all(struct wait_queue *queue);
    
    #endif
    ✓ • 0ms

## Assistant

对，资源还没归还，只是线程已经退出。

接下来学另一种暂停：**等待事件，而不是等待时间。**

之前的 `scheduler_sleep(3)` 表示等三个 tick。现在换成一个实际场景：

> 消费者要读取数据，但缓冲区是空的；它应该暂停，等生产者放入数据后再继续。

项目用一个只能存 **4 个整数**的缓冲区做测试：

```c
struct pipe_test {
    struct spinlock lock;
    struct wait_queue readable;
    struct wait_queue writable;
    unsigned data[4];
    size_t read;
    size_t write;
    size_t count;
    /* 另有测试统计字段 */
};
```

其中：

```text
readable：等待“有数据可读”的线程
writable：等待“有空间可写”的线程
```

等待队列保存的是**线程**，不是数据。整数放在 `data[4]` 中。

先看消费者的核心流程：

```c
uint64_t flags = irq_save();
spin_lock(&pipe->lock);

while (pipe->count == 0)
    wait_queue_sleep(&pipe->readable, &pipe->lock);

value = pipe->data[pipe->read];
pipe->read = (pipe->read + 1) % 4;
--pipe->count;

wait_queue_wake_one(&pipe->writable);
spin_unlock(&pipe->lock);
irq_restore(flags);
```

这次缓冲区和之前的日志不同：**读取会取走数据，使 `count` 减一**，从而给生产者腾出空间。

重点是这一句：

```c
wait_queue_sleep(&pipe->readable, &pipe->lock);
```

它的实现只有四行：

```c
void wait_queue_sleep(struct wait_queue *queue, struct spinlock *lock)
{
    scheduler_block_current(&queue->sleepers);
    spin_unlock(lock);
    scheduler_switch();
    spin_lock(lock);
}
```

我们假设消费者 C 发现 `count == 0`，走一遍。

**第一步：先登记“我在等数据”。**

```c
scheduler_block_current(&queue->sleepers);
```

内部执行：

```c
current->state = THREAD_SLEEPING;
list_push_back(queue, &current->wait_link);
```

C 变成睡眠状态，并加入 `readable` 等待队列。它仍然保留在所有线程链表中，所以这里使用的是另一组链表节点 `wait_link`。

**第二步：释放缓冲区的锁。**

```c
spin_unlock(lock);
```

必须释放。否则生产者无法取得锁，也就不能写入数据；C 等待的条件永远无法成立。

**第三步：切换到其他线程。**

```c
scheduler_switch();
```

C 暂停。假设生产者 P 开始运行，它取得同一把锁，写入一个整数，使 `count` 从 0 变成 1，然后：

```c
wait_queue_wake_one(&pipe->readable);
```

唤醒操作将 C 从等待队列摘下，并把状态从 `SLEEPING` 改为 `RUNNABLE`。此时 C 只是可以运行，还需要等调度器选中它。

**第四步：C 恢复后，重新取得锁。**

C 被选中时，从自己的 `scheduler_switch()` 调用之后继续：

```c
spin_lock(lock);
```

随后 `wait_queue_sleep` 返回，消费者回到：

```c
while (pipe->count == 0)
```

重新检查缓冲区。确认有数据后，才真正读取。

所以这个函数的调用约定是：

```text
调用前：调用者已关闭本地中断，并持有条件锁
等待时：登记自己，释放锁，切走
返回时：重新持有同一把锁
```

“先登记等待，再释放锁”的顺序也很重要：在当前单 hart、调用者已关闭中断的约定下，它避免出现“生产者已经发出唤醒，消费者却还没登记，之后睡过去”的丢失唤醒问题。

我们先抓住锁的交接。假设 C 在睡眠时一直持有 `pipe->lock`，生产者 P 又必须取得它才能写数据：**P 能不能完成写入并唤醒 C？**

## User

不能

## Assistant

对。生产者拿不到锁就无法放入数据，消费者又在等数据，双方会卡住。所以消费者必须**登记等待后释放锁，再切走**。

接下来解释两个容易写错的地方。

第一个是：**为什么醒来后要重新检查条件，使用 `while` 而不是 `if`？**

```c
while (pipe->count == 0)
    wait_queue_sleep(&pipe->readable, &pipe->lock);
```

因为唤醒只表示：

> 条件可能已经变化，你可以重新检查。

它没有把某个数据项直接交给这个线程。

例如，把场景扩展为多个消费者：

```text
生产者放入一个数据，唤醒消费者 C1
    ↓
C1 变成 RUNNABLE，但还没有执行
    ↓
另一个消费者 C2 先取得锁，取走数据
    ↓
C1 后来取得锁，发现 count 又是 0
```

C1 必须继续等待。如果用 `if`，醒来后就直接往下读，可能读到无效内容，还会错误地减少 `count`。

当前测试只有一个生产者和一个消费者，但使用 `while` 仍然表达了正确规则：**取得锁后，以实际条件为准。**

第二个是：**唤醒函数具体做了什么？**

项目的 `wait_queue_wake_one` 是：

```c
bool wait_queue_wake_one(struct wait_queue *queue)
{
    if (list_empty(&queue->sleepers))
        return false;

    scheduler_wake_node(queue->sleepers.next);
    return true;
}
```

如果有人等待，就选等待队列中的第一个节点，交给：

```c
void scheduler_wake_node(struct list_node *node)
{
    struct thread *thread =
        container_of(node, struct thread, wait_link);

    list_remove(node);

    if (thread->state == THREAD_SLEEPING)
        thread->state = THREAD_RUNNABLE;
}
```

这里用 `container_of` 从 `wait_link` 找回线程对象，再摘掉等待节点，修改状态。它**没有直接切换到被唤醒线程**。

因此，生产者唤醒消费者后，仍会继续自己的代码，释放锁、恢复中断状态。消费者之后才可能获得 CPU。

反过来，缓冲区满时，生产者也要睡眠：

```c
while (pipe->count == 4)
    wait_queue_sleep(&pipe->writable, &pipe->lock);
```

消费者取走一个数据以后：

```c
--pipe->count;
wait_queue_wake_one(&pipe->writable);
```

这样就形成对称关系：

| 当前条件 | 谁需要等待 | 等在哪个队列 | 谁改变条件并唤醒 |
|---|---|---|---|
| 缓冲区空 | 消费者 | `readable` | 生产者写入数据 |
| 缓冲区满 | 生产者 | `writable` | 消费者取走数据 |

这就是项目生产者/消费者同步的核心：**锁保护缓冲区状态，等待队列让条件不满足的线程暂停，唤醒让它重新参与调度。**

确认一下：**如果消费者取走一个数据后，误唤醒了 `readable`，而真正睡着的是等待空间的生产者，它会因此自动醒来吗？**

## User

不会

## Assistant

对，生产者在 `writable` 队列里，唤醒另一个队列不会找到它。接下来看看测试如何判断这套同步机制是否按预期完成，而不只是“线程似乎都运行了”。

## Activity

    $ sed -n '110,240p' src/kernel/m4_test.c && sed -n '1,95p' src/kernel/refcount.h
    }
    
    static bool object_tests(void)
    {
        struct refcount reference;
        LIST_HEAD(head);
        struct list_node a;
        struct list_node b;
        static const char message[] = "m4-log";
        char copy[sizeof(message)];
        void *objects[8];
        size_t baseline = page_free_count();
    
        refcount_init(&reference);
        refcount_get(&reference);
        if (refcount_read(&reference) != 2 || refcount_put(&reference) ||
            !refcount_put(&reference))
            return false;
        list_init(&a);
        list_init(&b);
        list_push_back(&head, &a);
        list_push_back(&head, &b);
        if (head.next != &a || head.prev != &b)
            return false;
        list_remove(&a);
        list_remove(&b);
        if (!list_empty(&head))
            return false;
        for (size_t i = 0; i < 8; ++i) {
            objects[i] = kmalloc((size_t)1 << (i + 4));
            if (!objects[i])
                return false;
            memset(objects[i], (int)i, (size_t)1 << (i + 4));
        }
        for (size_t i = 0; i < 8; ++i)
            kfree(objects[i]);
        if (page_free_count() != baseline)
            return false;
        log_write(message, sizeof(message));
        return log_read(copy, sizeof(copy)) == sizeof(copy) &&
               memcmp(message, copy, sizeof(copy)) == 0;
    }
    
    bool run_m4_tests(void)
    {
        uint64_t preemptions;
    
        if (!object_tests())
            return false;
        console_puts("M4 OBJECT PASS\n");
        register_results[0] = false;
        register_results[1] = false;
        if (!thread_create("register-a", register_worker, (void *)0) ||
            !thread_create("register-b", register_worker, (void *)1))
            return false;
        while (!register_results[0] || !register_results[1])
            scheduler_yield();
        scheduler_reap();
        console_puts("M4 SWITCH PASS\n");
    
        preempt_a = 0;
        preempt_b = 0;
        preemptions = scheduler_preemptions();
        if (!thread_create("preempt-a", preempt_worker_a, 0) ||
            !thread_create("preempt-b", preempt_worker_b, 0))
            return false;
        scheduler_sleep(6);
        if (preempt_a == 0 || preempt_b == 0 ||
            scheduler_preemptions() <= preemptions)
            return false;
        scheduler_reap();
        console_puts("M4 TIMER PASS\n");
    
        sleep_done = false;
        sleep_elapsed = 0;
        if (!thread_create("sleeper", sleep_worker, 0))
            return false;
        while (!sleep_done)
            scheduler_yield();
        if (sleep_elapsed < 4)
            return false;
        scheduler_reap();
        console_puts("M4 SLEEP PASS\n");
    
        memset(&pipe_test, 0, sizeof(pipe_test));
        spinlock_init(&pipe_test.lock);
        wait_queue_init(&pipe_test.readable);
        wait_queue_init(&pipe_test.writable);
        if (!thread_create("producer", producer, &pipe_test) ||
            !thread_create("consumer", consumer, &pipe_test))
            return false;
        while (!pipe_test.producer_done || !pipe_test.consumer_done)
            scheduler_yield();
        scheduler_reap();
        console_puts("M4 QUEUE PASS\n");
        return pipe_test.count == 0 &&
               pipe_test.produced_sum == pipe_test.consumed_sum &&
               pipe_test.produced_sum == (64U * 65U) / 2U;
    }
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
    ✓ • 0ms

## Assistant

对，它不会自动醒来。等待队列只管理登记在自己这里的线程，不会根据缓冲区状态自动寻找其他队列。

现在看项目怎样验证生产者和消费者完成了工作。

生产者依次放入整数 `1～64`：

```c
for (unsigned value = 1; value <= 64; ++value) {
    /* 等待可写，放入 value */
    pipe->produced_sum += value;
}
pipe->producer_done = true;
```

消费者执行 64 次读取，累加读到的值：

```c
for (unsigned i = 0; i < 64; ++i) {
    /* 等待可读，取出 value */
    pipe->consumed_sum += value;
}
pipe->consumer_done = true;
```

主测试线程等待两者完成：

```c
while (!pipe_test.producer_done || !pipe_test.consumer_done)
    scheduler_yield();

scheduler_reap();
```

这里主线程不断让出调度机会，让两个工作线程推进。最后检查：

```c
return pipe_test.count == 0 &&
       pipe_test.produced_sum == pipe_test.consumed_sum &&
       pipe_test.produced_sum == (64U * 65U) / 2U;
```

含义是：

```text
缓冲区最终为空
生产与消费的总和相同
总和等于 1 + 2 + … + 64 = 2080
```

这是有用的验收，但要准确理解它的范围：**它没有逐项检查消费者是否严格按 `1、2、3……64` 的顺序读到数据。** 顺序颠倒，甚至某些总和恰好抵消的错误，都可能逃过总和检查。

还有一个观察：代码先打印 `M4 QUEUE PASS`，再执行最后的返回条件。所以单独看到这行日志，还不能代替检查整个测试是否成功返回。

至此，我们已理解 M4 的堆、链表、日志、线程、抢占、睡眠和等待队列。它还包含一个很小的工具：**引用计数**，之前没有展开，现在补上。

项目实现是：

```c
struct refcount {
    size_t value;
};
```

初始化：

```c
reference->value = 1;
```

增加一份持有关系：

```c
++reference->value;
```

释放一份持有关系：

```c
return --reference->value == 0;
```

它要解决的是：一个对象由多个使用者共同持有时，谁能决定最终释放？

例如：

```text
A 创建对象：计数 1
B 也持有它：计数 2
A 不再使用：计数 1，不能释放
B 不再使用：计数 0，可以最终释放
```

项目的 `refcount_put` 只返回“是否降到零”，**不会自己调用 `kfree`**。释放什么资源，仍由对象的管理代码决定。

这个实现也没有原子操作、锁或下溢检查，所以不能把它当成可以随意并发调用的引用计数。调用者需要保证同步正确，并且每次 `put` 都对应自己持有的一份引用。

当前的对象测试验证了最基本的变化：

```text
init → 1
get  → 2
put  → 1，返回 false
put  → 0，返回 true
```

先确认它和普通指针复制的区别：**如果只是写 `b = a`，复制了一个指针，但没有调用 `refcount_get`，计数会自动从 1 变成 2 吗？**

## User

no

## Assistant

对，指针赋值不会自动更新引用计数，持有关系需要代码显式维护。接下来补上之前尚未展开的用户态部分：内核怎样建立用户地址空间，并第一次进入 U-mode。

## Activity

    $ sed -n '1,220p' src/kernel/user.c && sed -n '236,335p' src/arch/riscv/trap.S && sed -n '1,90p' user/user.S
    #include "user.h"
    
    #include "../arch/riscv/csr.h"
    #include "../mm/layout.h"
    #include "../mm/page_alloc.h"
    #include "../platform/platform.h"
    #include "../runtime.h"
    #include "scheduler.h"
    
    #define USER_CODE UINT64_C(0x10000)
    #define USER_STACK_TOP UINT64_C(0x400000)
    #define USER_STACK_PAGES 2U
    #define SYSCALL_WRITE UINT64_C(1)
    #define SYSCALL_EXIT UINT64_C(2)
    #define SYSCALL_YIELD UINT64_C(3)
    #define EXCEPTION_USER_ECALL UINT64_C(8)
    #define EXCEPTION_LOAD_PAGE_FAULT UINT64_C(13)
    #define EXCEPTION_STORE_PAGE_FAULT UINT64_C(15)
    
    struct file;
    
    struct file_ops {
        int64_t (*write)(struct file *file, uint64_t buffer, size_t size);
    };
    
    struct file {
        const struct file_ops *ops;
    };
    
    struct user_process {
        struct page_table page_table;
        uint64_t *pages;
        size_t page_count;
        struct file *fds[3];
        int64_t exit_status;
        uint64_t fault_code;
        bool finished;
    };
    
    extern const unsigned char _binary_build_user_bin_start[];
    extern const unsigned char _binary_build_user_bin_end[];
    extern void user_enter(uint64_t entry, uint64_t stack, uint64_t argument);
    extern void user_resume_kernel(void);
    
    uint64_t user_kernel_sp;
    static struct user_process *current;
    
    static bool copy_user(void *kernel_buffer, uint64_t user_address, size_t size,
                          bool to_user)
    {
        unsigned char *buffer = kernel_buffer;
    
        if (!current || user_address > UINT64_MAX - size)
            return false;
        while (size != 0) {
            uint64_t physical;
            uint64_t flags;
            size_t chunk = PAGE_SIZE - (user_address & PAGE_MASK);
    
            if (chunk > size)
                chunk = size;
            if (!vm_query(&current->page_table, user_address, &physical, &flags) ||
                (flags & (PTE_U | PTE_R)) != (PTE_U | PTE_R) ||
                (to_user && (flags & PTE_W) == 0))
                return false;
            if (to_user)
                memcpy(phys_to_virt(physical), buffer, chunk);
            else
                memcpy(buffer, phys_to_virt(physical), chunk);
            buffer += chunk;
            user_address += chunk;
            size -= chunk;
        }
        return true;
    }
    
    bool copy_from_user(void *destination, uint64_t source, size_t size)
    {
        return size == 0 ||
               (destination && copy_user(destination, source, size, false));
    }
    
    bool copy_to_user(uint64_t destination, const void *source, size_t size)
    {
        return size == 0 ||
               (source && copy_user((void *)source, destination, size, true));
    }
    
    static int64_t console_write(struct file *file, uint64_t address, size_t size)
    {
        char buffer[64];
        size_t done = 0;
    
        (void)file;
        if (address > UINT64_MAX - size)
            return -USER_EFAULT;
        while (done < size) {
            size_t chunk = size - done;
    
            if (chunk > sizeof(buffer))
                chunk = sizeof(buffer);
            if (!copy_from_user(buffer, address + done, chunk))
                return -USER_EFAULT;
            for (size_t i = 0; i < chunk; ++i)
                console_putc(buffer[i]);
            done += chunk;
        }
        return (int64_t)done;
    }
    
    static const struct file_ops console_ops = { .write = console_write };
    static struct file console_file = { .ops = &console_ops };
    
    static void finish_user(struct trap_frame *frame, int64_t status,
                            uint64_t fault)
    {
        current->finished = true;
        current->exit_status = status;
        current->fault_code = fault;
        frame->x[1] = (uintptr_t)user_resume_kernel;
        frame->x[2] = user_kernel_sp;
        frame->sepc = (uintptr_t)user_resume_kernel;
        /* Return to the scheduler's supervisor context with interrupts enabled.
         * sret copies SPIE into SIE, so setting SPIE here is intentional. */
        frame->sstatus |= SSTATUS_SPP | SSTATUS_SPIE;
        frame->sstatus &= ~SSTATUS_SIE;
    }
    
    static int64_t dispatch_syscall(struct trap_frame *frame)
    {
        switch (frame->x[17]) {
        case SYSCALL_WRITE: {
            uint64_t fd = frame->x[10];
            struct file *file;
    
            if (fd >= sizeof(current->fds) / sizeof(current->fds[0]) ||
                !(file = current->fds[fd]) || !file->ops || !file->ops->write)
                return -9;
            return file->ops->write(file, frame->x[11], (size_t)frame->x[12]);
        }
        case SYSCALL_EXIT:
            finish_user(frame, (int64_t)frame->x[10], 0);
            return 0;
        case SYSCALL_YIELD:
            scheduler_yield();
            return 0;
        default:
            return -38;
        }
    }
    
    bool user_handle_trap(struct trap_frame *frame, uint64_t code)
    {
        if (!current || (frame->sstatus & SSTATUS_SPP) != 0)
            return false;
        if (code == EXCEPTION_USER_ECALL) {
            frame->sepc += 4;
            frame->x[10] = (uint64_t)dispatch_syscall(frame);
            return true;
        }
        finish_user(frame, -1, code);
        return true;
    }
    
    static void release_process(struct user_process *process)
    {
        for (size_t i = 0; i < process->page_count; ++i) {
            if (process->pages[i])
                page_free(process->pages[i]);
        }
        vm_destroy_user(&process->page_table);
    }
    
    static bool prepare_process(struct user_process *process,
                                const struct page_table *kernel_table,
                                uint64_t *pages, size_t page_capacity)
    {
        size_t image_size = (size_t)(_binary_build_user_bin_end -
                                     _binary_build_user_bin_start);
        size_t code_pages = (image_size + PAGE_MASK) / PAGE_SIZE;
    
        memset(process, 0, sizeof(*process));
        process->pages = pages;
        if (code_pages + USER_STACK_PAGES > page_capacity ||
            !vm_create_user(&process->page_table, kernel_table))
            return false;
        for (size_t i = 0; i < code_pages; ++i) {
            size_t offset = i * PAGE_SIZE;
            size_t chunk = image_size - offset;
            uint64_t page = page_alloc();
    
            if (chunk > PAGE_SIZE)
                chunk = PAGE_SIZE;
            if (!page)
                goto fail;
            pages[process->page_count++] = page;
            memset(phys_to_virt(page), 0, PAGE_SIZE);
            memcpy(phys_to_virt(page), _binary_build_user_bin_start + offset,
                   chunk);
            if (!vm_map(&process->page_table, USER_CODE + offset, page,
                        PTE_U | PTE_R | PTE_X))
                goto fail;
        }
        for (size_t i = 0; i < USER_STACK_PAGES; ++i) {
            uint64_t page = page_alloc();
            uint64_t address = USER_STACK_TOP -
                               (USER_STACK_PAGES - i) * PAGE_SIZE;
    
            if (!page)
                goto fail;
            pages[process->page_count++] = page;
            memset(phys_to_virt(page), 0, PAGE_SIZE);
            if (!vm_map(&process->page_table, address, page,
                        PTE_U | PTE_R | PTE_W))
                goto fail;
        }
        process->fds[1] = &console_file;
        process->fds[2] = &console_file;
        return true;
    fail:
        sd s3, 48(sp)
        sd s4, 56(sp)
        sd s5, 64(sp)
        sd s6, 72(sp)
        sd s7, 80(sp)
        sd s8, 88(sp)
        sd s9, 96(sp)
        sd s10, 104(sp)
        sd s11, 112(sp)
        .option push
        .option norelax
        la t0, user_kernel_sp
        .option pop
        sd sp, 0(t0)
    
        csrw sepc, a0
        csrr t0, sstatus
        li t1, ((1 << 8) | (1 << 1))
        not t1, t1
        and t0, t0, t1
        ori t0, t0, (1 << 5)
        csrw sstatus, t0
        csrw sscratch, sp
        mv sp, a1
        mv a0, a2
        sret
    
        .globl user_resume_kernel
    user_resume_kernel:
        ld ra, 0(sp)
        ld gp, 8(sp)
        ld tp, 16(sp)
        ld s0, 24(sp)
        ld s1, 32(sp)
        ld s2, 40(sp)
        ld s3, 48(sp)
        ld s4, 56(sp)
        ld s5, 64(sp)
        ld s6, 72(sp)
        ld s7, 80(sp)
        ld s8, 88(sp)
        ld s9, 96(sp)
        ld s10, 104(sp)
        ld s11, 112(sp)
        addi sp, sp, 128
        ret
        .size user_enter, . - user_enter
    
        .section .note.GNU-stack, "", @progbits
        .section .text, "ax", @progbits
        .globl _start
    _start:
        beqz a0, .Lnormal
        li t0, 1
        beq a0, t0, .Lkernel_access
    
        /* The program text is U+RX, never writable. */
        la t0, _start
        sd zero, 0(t0)
        j .Lfailed
    
    .Lkernel_access:
        /* The kernel direct map exists in this page table but lacks PTE_U. */
        li t0, 0xffffffc080200000
        ld t1, 0(t0)
        j .Lfailed
    
    .Lnormal:
        li a7, 3
        ecall
        bnez a0, .Lfailed
    
        /* Put eight bytes across the boundary between the two stack pages. */
        li t0, 0x3feffc
        li t1, 'M'
        sb t1, 0(t0)
        li t1, '3'
        sb t1, 1(t0)
        li t1, ' '
        sb t1, 2(t0)
        li t1, 'U'
        sb t1, 3(t0)
        li t1, 'S'
        sb t1, 4(t0)
        li t1, 'E'
        sb t1, 5(t0)
        li t1, 'R'
        sb t1, 6(t0)
        li t1, '\n'
        sb t1, 7(t0)
        li a0, 1
        mv a1, t0
        li a2, 8
        li a7, 1
        ecall
        li t0, 8
        bne a0, t0, .Lfailed
    
        /* An unmapped pointer must become -EFAULT, not a supervisor fault. */
        li a0, 1
        li a1, 0x500000
        li a2, 1
        li a7, 1
        ecall
        li t0, -14
        bne a0, t0, .Lfailed
    
        li a0, 0
        li a7, 2
        ecall
    
    .Lfailed:
        li a0, 1
        li a7, 2
        ecall
    .Lhang:
        j .Lhang
    ✓ • 0ms

## Assistant

对，指针复制不会自动增加引用计数。如果 B 要独立持有对象，就需要按约定增加引用；只是临时借用指针，则不一定需要增加。

接下来补上**用户态程序**这条主线。我们刚才先集中学了调度器；实际 `kernel_main` 中，是初始化调度器后先跑用户态测试，再跑 M4 线程测试。

先明确一个区别：**前面创建的 A、B 都是内核线程，运行在 S-mode。创建线程并不会自动进入 U-mode。**

运行用户程序，还需要准备受限制的地址空间，并明确切换特权级。

MiniCore 的用户测试程序来自：

```text
user/user.S
    → 构建成二进制
    → 嵌入内核镜像
    → 运行时复制到用户程序专用的物理页
```

目前不是从文件系统加载程序。

我们先看用户地址空间的布局：

```text
高地址：共享内核映射，用户态不可访问
             …

0x400000  ← 用户栈顶
┌──────────────────────┐
│ 两页用户栈，共 8 KiB │ 可读写、不可执行
└──────────────────────┘
0x3fe000
             …

0x10000   ← 用户代码起点，可读、可执行、不可写
```

创建它的第一步是：

```c
vm_create_user(&process->page_table, kernel_table);
```

这个函数先申请新的根页表，再复制内核根页表的上半部分：

```c
root = root_pointer(table);
kernel_root = root_pointer(kernel_table);

memcpy(root + 256, kernel_root + 256, 256 * sizeof(*root));
```

得到：

```text
新根页表
├─ [0～255]：初始为空，之后填用户映射
└─ [256～511]：复制内核根表项，共享内核下级页表
```

注意：复制的是**根表项**，没有复制整份内核代码，也没有复制所有下级页表。

为什么用户页表里还放内核映射？在本项目中，用户程序发生 trap 后，CPU 要执行内核入口、访问内核栈。保留内核映射，能让这些地址在当前页表下继续有效。

但映射存在不意味着用户可以访问：内核叶子表项没有 `PTE_U`。

接下来，`prepare_process` 申请用户代码页、复制程序内容，再建立映射：

```c
vm_map(&process->page_table,
       USER_CODE + offset,
       page,
       PTE_U | PTE_R | PTE_X);
```

这里的权限是：

```text
U：允许用户态访问
R：允许读取
X：允许执行
没有 W：禁止写入代码
```

用户栈则使用：

```c
vm_map(&process->page_table,
       address,
       page,
       PTE_U | PTE_R | PTE_W);
```

允许用户读写，但没有执行权限。

这正好把之前学过的两个模块连起来：

```text
page_alloc：提供实际物理页
vm_map：决定用户在哪个虚拟地址、以什么权限访问它
```

准备完成后，内核才会启用这套页表，再通过 `user_enter` 进入 U-mode。**切换页表和切换特权级是两个动作，不能混为一谈。**

先确认权限边界：**用户页表里能查到某个内核地址的映射，但对应叶子表项没有 `PTE_U`，U-mode 程序能读取那里吗？**

## User

no

## Assistant

对，没有 `PTE_U` 就不允许用户态访问。接下来走 `user_enter`：它怎样准备用户栈、入口地址和返回内核时要用的状态。

## Activity

    $ sed -n '220,335p' src/kernel/user.c && sed -n '224,290p' src/arch/riscv/trap.S
    fail:
        release_process(process);
        return false;
    }
    
    static bool run_case(const struct page_table *kernel_table, uint64_t mode,
                         int64_t expected_status, uint64_t expected_fault)
    {
        struct user_process process;
        uint64_t pages[18] = {0};
    
        if (!prepare_process(&process, kernel_table, pages,
                             sizeof(pages) / sizeof(pages[0])))
            return false;
        current = &process;
        vm_activate(&process.page_table);
        user_enter(USER_CODE, USER_STACK_TOP - 16, mode);
        vm_activate(kernel_table);
        current = 0;
        bool passed = process.finished && process.exit_status == expected_status &&
                      process.fault_code == expected_fault;
        release_process(&process);
        return passed;
    }
    
    static bool run_copy_test(const struct page_table *kernel_table)
    {
        static const char source[] = "copytest";
        struct user_process process;
        uint64_t pages[18] = {0};
        char result[sizeof(source)];
        bool passed;
    
        if (!prepare_process(&process, kernel_table, pages,
                             sizeof(pages) / sizeof(pages[0])))
            return false;
        current = &process;
        passed = copy_to_user(UINT64_C(0x3feffc), source, sizeof(source)) &&
                 copy_from_user(result, UINT64_C(0x3feffc), sizeof(result)) &&
                 memcmp(source, result, sizeof(source)) == 0 &&
                 !copy_to_user(USER_CODE, source, 1) &&
                 !copy_from_user(result, UINT64_MAX - 2, sizeof(result));
        current = 0;
        release_process(&process);
        return passed;
    }
    
    bool user_run_m3_tests(const struct page_table *kernel_table)
    {
        size_t baseline = page_free_count();
    
        /* SUM deliberately remains clear: copies walk and validate PTEs and then
         * use the kernel direct map.  A bad U pointer therefore cannot fault the
         * kernel, and the permission check is identical across page boundaries. */
        csr_clear_sstatus(SSTATUS_SUM);
        if (!run_copy_test(kernel_table) ||
            !run_case(kernel_table, 0, 0, 0) ||
            !run_case(kernel_table, 1, -1, EXCEPTION_LOAD_PAGE_FAULT) ||
            !run_case(kernel_table, 2, -1, EXCEPTION_STORE_PAGE_FAULT) ||
            page_free_count() != baseline)
            return false;
        return true;
    }
    
        .align 2
        .globl user_enter
        .type user_enter, @function
    user_enter:
        addi sp, sp, -128
        sd ra, 0(sp)
        sd gp, 8(sp)
        sd tp, 16(sp)
        sd s0, 24(sp)
        sd s1, 32(sp)
        sd s2, 40(sp)
        sd s3, 48(sp)
        sd s4, 56(sp)
        sd s5, 64(sp)
        sd s6, 72(sp)
        sd s7, 80(sp)
        sd s8, 88(sp)
        sd s9, 96(sp)
        sd s10, 104(sp)
        sd s11, 112(sp)
        .option push
        .option norelax
        la t0, user_kernel_sp
        .option pop
        sd sp, 0(t0)
    
        csrw sepc, a0
        csrr t0, sstatus
        li t1, ((1 << 8) | (1 << 1))
        not t1, t1
        and t0, t0, t1
        ori t0, t0, (1 << 5)
        csrw sstatus, t0
        csrw sscratch, sp
        mv sp, a1
        mv a0, a2
        sret
    
        .globl user_resume_kernel
    user_resume_kernel:
        ld ra, 0(sp)
        ld gp, 8(sp)
        ld tp, 16(sp)
        ld s0, 24(sp)
        ld s1, 32(sp)
        ld s2, 40(sp)
        ld s3, 48(sp)
        ld s4, 56(sp)
        ld s5, 64(sp)
        ld s6, 72(sp)
        ld s7, 80(sp)
        ld s8, 88(sp)
        ld s9, 96(sp)
        ld s10, 104(sp)
        ld s11, 112(sp)
        addi sp, sp, 128
        ret
        .size user_enter, . - user_enter
    
        .section .note.GNU-stack, "", @progbits
    ✓ • 0ms

## Assistant

对，用户态访问会被权限检查挡住。

现在准备第一次进入 U-mode。内核先切换页表，再调用：

```c
vm_activate(&process.page_table);
user_enter(USER_CODE, USER_STACK_TOP - 16, mode);
```

代入项目里的地址：

```text
a0 = 0x10000   → 用户代码入口
a1 = 0x3ffff0  → 用户初始栈指针
a2 = mode     → 传给用户测试程序的模式参数
```

**此时虽然已经启用用户页表，CPU 仍在 S-mode。** 页表决定地址怎么翻译，特权级决定以什么权限执行。

`user_enter` 先在当前内核栈上留出 128 字节，保存返回地址和需要保留的寄存器：

```asm
addi sp, sp, -128
sd ra, 0(sp)
sd gp, 8(sp)
sd tp, 16(sp)
sd s0, 24(sp)
...
```

这些信息用于用户测试结束后，恢复内核原来的调用流程。随后把当前内核栈位置保存到全局变量：

```asm
la t0, user_kernel_sp
sd sp, 0(t0)
```

接下来是进入用户态的核心准备。

**第一步：设置即将执行的位置。**

```asm
csrw sepc, a0
```

现在：

```text
sepc = 0x10000
```

你之前见过 `sepc` 用来记录异常返回位置。这里内核主动填写它，把 `sret` 的目的地设成用户程序入口。

**第二步：设置 `sret` 要恢复的特权级和中断状态。**

相关代码合起来的效果是：

```text
sstatus.SPP  = 0
sstatus.SIE  = 0
sstatus.SPIE = 1
```

分别表示：

- `SPP=0`：接下来的 `sret` 进入 U-mode。
- `SIE=0`：关闭当前 S-mode 中断，保护最后的切栈过程。
- `SPIE=1`：为 `sret` 恢复中断状态作准备，执行时会复制到 `SIE`。

这里不是等一次真实异常再返回，而是**人为准备返回状态，利用 `sret` 完成第一次进入用户态**。

**第三步：准备用户栈，并留下内核栈的位置。**

```asm
csrw sscratch, sp
mv sp, a1
```

假设保存现场后的内核栈指针是 K：

```text
执行前：
sp       = K

执行后：
sscratch = K          ← 下次 trap 时取回内核栈
sp       = 0x3ffff0    ← 用户栈
```

还记得 `trap_entry` 的第一条指令吗？

```asm
csrrw sp, sscratch, sp
```

它就是为这个约定服务的。将来用户态发生 trap，这次交换会让：

```text
sp       = 内核栈指针
sscratch = 被打断时的用户栈指针
```

这样入口就能在内核栈上保存现场，而不依赖用户栈。

**第四步：传入用户参数，进入 U-mode。**

```asm
mv a0, a2
sret
```

用户程序启动时，通过 `a0` 得到 `mode`。`sret` 则让 CPU：

```text
PC → sepc 中的 0x10000
特权级 → U-mode
sp → 保持为刚准备的用户栈指针
```

注意，**用户 `sp` 是前面的 `mv` 设置的，不是 `sret` 自动帮忙切换的。**

完整过程是：

```text
保存内核调用现场
  → sepc 设置用户入口
  → SPP 设置为 U-mode
  → sscratch 保存内核栈位置
  → sp 切到用户栈
  → sret，开始执行用户程序
```

确认一下这两个栈指针的用途：**用户程序运行时，`sp` 指向用户栈，那么保存在 `sscratch` 中的内核栈指针，是供用户程序直接使用，还是供下一次 trap 入口切栈使用？**

## User

后者

## Assistant

对，`sscratch` 中的内核栈指针，是供下一次 trap 入口切栈使用的。

现在用户程序已经开始运行。正常测试路径的第一件事是：

```asm
li a7, 3
ecall
bnez a0, .Lfailed
```

它请求的不是打印，而是 **`yield`：主动让出一次调度机会**。

这里的 `3` 来自 MiniCore 自己定义的系统调用编号：

| `a7` 的值 | 请求 |
|---:|---|
| 1 | `write` |
| 2 | `exit` |
| 3 | `yield` |

这些编号是软件约定。**CPU 不知道“3 表示 yield”，它只负责执行 `ecall` 并触发异常。**

这次与此前调用 OpenSBI 的区别是：

```text
S-mode 执行 ecall → 本项目中进入 M-mode OpenSBI

U-mode 执行 ecall → 本项目中进入 S-mode 内核
```

用户程序这次触发的异常编号是 `8`，即来自 U-mode 的环境调用。

进入 `trap_entry` 时，假设：

```text
sp       = U，用户当前栈指针
sscratch = K，预留的内核栈指针
```

第一条指令交换它们：

```asm
csrrw sp, sscratch, sp
```

得到：

```text
sp       = K
sscratch = U
```

因为此时 `sp` 非零，入口直接跳到保存现场的位置，不再执行第二次交换。之后就在内核栈上预留 288 字节并保存寄存器。

其中：

```text
frame->x[17] = 用户的 a7 = 3
```

处理流程来到：

```c
if (user_handle_trap(frame, code))
    return;
```

`user_handle_trap` 先确认确实来自用户态：

```c
if (!current || (frame->sstatus & SSTATUS_SPP) != 0)
    return false;
```

这里的 `current` 是 `user.c` 中记录当前用户进程的指针；它与 `scheduler.c` 中的当前线程指针是各自文件里的不同变量。

然后处理用户 `ecall`：

```c
if (code == EXCEPTION_USER_ECALL) {
    frame->sepc += 4;
    frame->x[10] = (uint64_t)dispatch_syscall(frame);
    return true;
}
```

这两行各有作用。

```c
frame->sepc += 4;
```

跳过已经处理的 `ecall`。它是四字节指令；否则返回用户态后会再次执行同一个调用。

```c
frame->x[10] = dispatch_syscall(frame);
```

把系统调用结果写进保存的 `a0` 槽位。之后恢复寄存器时，用户就能从 `a0` 读到结果。

分发函数查看保存的 `a7`：

```c
switch (frame->x[17]) {
    ...
    case SYSCALL_YIELD:
        scheduler_yield();
        return 0;
}
```

这就连接到了我们已经学过的调度器。期间可能切换到别的线程；等这条执行流恢复，`scheduler_yield` 返回，系统调用返回值为零。

最后，trap 汇编恢复用户现场，切回用户栈，执行 `sret`。用户程序从 `ecall` 后面继续：

```asm
bnez a0, .Lfailed
```

因为 `a0=0`，不跳转失败分支，继续后面的测试。

整条路径是：

```text
用户：a7=3，ecall
  → 内核切栈、保存现场
  → 根据 a7 分发 yield
  → 调度，之后恢复
  → 把返回值 0 写进保存的 a0
  → 恢复用户现场，sret
  → 用户从 a0 读取结果
```

确认一个关键区别：**`frame->sepc += 4` 跳过的是用户的 `ecall` 指令，还是跳过 `ecall` 后面的 `bnez` 指令？**

## User

前者

## Assistant

对，跳过的是 `ecall`，返回后仍要执行那条 `bnez`。

用户程序接下来测试 `write`，先在用户栈上放入八个字符：

```text
M3 USER\n
```

它特意从地址 `0x3feffc` 开始写：

```text
第一页末尾                 第二页开头
0x3feffc                    0x3ff000
┌───┬───┬───┬───┐         ┌───┬───┬───┬────┐
│ M │ 3 │空格│ U │         │ S │ E │ R │ \n │
└───┴───┴───┴───┘         └───┴───┴───┴────┘
```

这八字节跨过两个 4 KiB 页。这样一次打印，也能测试内核是否正确处理跨页用户缓冲区。

然后准备系统调用参数：

```asm
li a0, 1          # 文件描述符：1
mv a1, t0        # 缓冲区地址：0x3feffc
li a2, 8         # 长度：8 字节
li a7, 1         # 系统调用编号：write
ecall
```

这里两个 `1` 含义不同：

```text
a7 = 1 → 我要执行 write
a0 = 1 → 我要写入编号为 1 的文件描述符
```

内核保存现场后，从这些槽位取参数：

```text
frame->x[17] → a7，调用编号
frame->x[10] → a0，文件描述符
frame->x[11] → a1，用户缓冲区地址
frame->x[12] → a2，字节数
```

`dispatch_syscall` 的 `write` 分支是：

```c
uint64_t fd = frame->x[10];
struct file *file;

if (fd >= sizeof(current->fds) / sizeof(current->fds[0]) ||
    !(file = current->fds[fd]) ||
    !file->ops || !file->ops->write)
    return -9;

return file->ops->write(file,
                        frame->x[11],
                        (size_t)frame->x[12]);
```

它先检查描述符是否越界、对应对象是否存在、对象是否支持写入，再调用写函数。

这里暂时没有完整文件系统，只是一个很小的接口：

```c
struct file_ops {
    int64_t (*write)(struct file *file,
                     uint64_t buffer, size_t size);
};

struct file {
    const struct file_ops *ops;
};
```

准备用户进程时，代码把描述符 1 和 2 都接到控制台对象：

```c
process->fds[1] = &console_file;
process->fds[2] = &console_file;
```

所以这次实际走的是：

```text
fd = 1
  → current->fds[1]
  → console_file
  → console_ops.write
  → console_write
```

**文件描述符是用户交给内核的一个小整数，不是允许用户直接指定内核函数的指针。** 内核通过自己的表决定最终调用什么。

`console_write` 没有直接把用户指针传给 `console_puts`。它先准备一个内核缓冲区：

```c
char buffer[64];
```

再分批执行：

```c
copy_from_user(buffer, address + done, chunk);
```

复制成功后，逐字节调用：

```c
console_putc(buffer[i]);
```

全部成功时返回写入的字节数。这次应该返回 `8`，最终恢复到用户的 `a0`，用户程序会检查它是否等于 `8`。

接下来最重要的是 `copy_from_user`：用户提供的地址可能未映射、权限不对，或者跨页后落入无效区域，内核不能直接相信它。

先想一下这个跨页例子：**即使前四字节所在的页合法，能否据此认为后四字节所在的下一页也一定合法？**

## User

不能

## Assistant

对，两页各有自己的映射和权限，必须分别检查。

项目的 `copy_from_user` 最终调用同一个内部函数：

```c
copy_user(destination, source, size, false);
```

其中 `false` 表示“从用户内存复制到内核”。我们继续用刚才的例子：

```text
用户起点：0x3feffc
复制长度：8 字节
```

核心循环是：

```c
while (size != 0) {
    uint64_t physical;
    uint64_t flags;
    size_t chunk = PAGE_SIZE - (user_address & PAGE_MASK);

    if (chunk > size)
        chunk = size;

    if (!vm_query(&current->page_table, user_address,
                  &physical, &flags) ||
        (flags & (PTE_U | PTE_R)) != (PTE_U | PTE_R) ||
        (to_user && (flags & PTE_W) == 0))
        return false;

    if (to_user)
        memcpy(phys_to_virt(physical), buffer, chunk);
    else
        memcpy(buffer, phys_to_virt(physical), chunk);

    buffer += chunk;
    user_address += chunk;
    size -= chunk;
}
```

先别急着读完整段，我们按两轮执行来看。

**第一轮：只处理第一页剩余的四字节。**

```c
chunk = PAGE_SIZE - (user_address & PAGE_MASK);
```

`0x3feffc` 的页内偏移是 `0xffc`，所以：

```text
chunk = 0x1000 - 0xffc = 4
```

意思是：从当前地址开始，再读四字节就到页边界了。

然后：

```c
vm_query(&current->page_table, user_address,
         &physical, &flags);
```

软件查用户页表，得到**这个具体地址对应的物理地址**，以及权限。

检查要求：

```text
映射存在
PTE_U = 1，确实是用户可访问页
PTE_R = 1，允许读取
```

检查通过，才复制这四字节。

这里的访问路径很值得注意：

```text
用户虚拟地址
    ↓ 软件查询用户页表并检查权限
物理地址
    ↓ phys_to_virt
内核高地址直接映射
    ↓ memcpy
内核缓冲区
```

**内核没有直接解引用用户虚拟地址。** 它先确认用户是否有权访问，再通过内核的直接映射读取同一块物理内存。

项目也明确保持 `sstatus.SUM` 为零，没有依赖开启 SUM 来直接访问用户页。

第一轮完成后：

```text
内核目标指针：前进 4 字节
用户地址：    0x3feffc + 4 = 0x3ff000
剩余长度：    8 - 4 = 4
```

**第二轮：重新查询下一页。**

现在地址恰好位于页首：

```text
当前页剩余空间：4096 字节
实际剩余任务：  4 字节
因此 chunk = 4
```

重新执行 `vm_query` 和权限检查，通过后才复制后四字节。

两轮完成，内核缓冲区得到：

```text
M3 USER\n
```

这样即使两张用户页映射到**不相邻的物理页**，也能正确复制。代码没有假定“第一页的物理地址加四，就一定是第二页的数据”。

同一个内部函数还支持 `copy_to_user`。这时 `to_user=true`，除了用户和读权限，还额外要求：

```c
(flags & PTE_W) != 0
```

复制方向也反过来，从内核缓冲区写入用户对应的物理页。

函数开始前还检查地址区间加法是否溢出：

```c
if (!current || user_address > UINT64_MAX - size)
    return false;
```

不过，这个实现有一个边界要记住：**失败不会回滚此前已经复制的部分。** 如果第一页合法、第二页非法，第一轮复制已经发生，第二轮才返回 `false`。

确认一下：**用户两页虚拟地址连续，但映射到的物理页不连续，这个逐页复制函数还能正确工作吗？**

## User

能

## Assistant

对。它每跨过一页都会重新查映射，所以不要求物理页连续。

用户程序完成打印后，还会故意用一个未映射地址调用 `write`：

```asm
li a0, 1          # fd
li a1, 0x500000   # 未映射的用户地址
li a2, 1         # 请求写一个字节
li a7, 1         # write
ecall
```

`copy_from_user` 查询失败，`console_write` 返回：

```c
return -USER_EFAULT;   // 本项目中是 -14
```

这个错误通过保存的 `a0` 返回用户态。内核不会直接解引用坏地址，因此这个测试不需要靠内核页故障来收场。

然后用户请求正常退出：

```asm
li a0, 0          # 退出状态为 0
li a7, 2          # exit
ecall
```

这次系统调用的目标不同：**用户已经结束，不应该再回到 `ecall` 后面继续运行。**

`dispatch_syscall` 会调用：

```c
finish_user(frame, (int64_t)frame->x[10], 0);
```

其中第二个参数是退出状态，第三个参数为零表示没有记录异常原因。

`finish_user` 先记录结果：

```c
current->finished = true;
current->exit_status = status;
current->fault_code = fault;
```

接着修改保存的 trap 现场：

```c
frame->x[1] = (uintptr_t)user_resume_kernel;
frame->x[2] = user_kernel_sp;
frame->sepc = (uintptr_t)user_resume_kernel;

frame->sstatus |= SSTATUS_SPP | SSTATUS_SPIE;
frame->sstatus &= ~SSTATUS_SIE;
```

把它翻译成执行目标就是：

```text
返回位置 → user_resume_kernel
返回栈   → 之前保存的内核栈
返回模式 → S-mode，因为 SPP=1
```

因此，这次 `sret` 虽然仍然执行“恢复现场并返回”，**恢复的已经是内核主动改造过的现场**，不会回到用户程序。

这就是我们之前说“C 处理函数可以修改 `trap_frame`，影响恢复行为”的具体应用。

接下来，`user_resume_kernel` 恢复最初进入用户态前保存的寄存器：

```asm
user_resume_kernel:
    ld ra, 0(sp)
    ld gp, 8(sp)
    ld tp, 16(sp)
    ld s0, 24(sp)
    ...
    addi sp, sp, 128
    ret
```

这里的 `ret` 又恢复使用原来的内核返回地址，让那次：

```c
user_enter(USER_CODE, USER_STACK_TOP - 16, mode);
```

终于返回到 C 代码。

于是 `run_case` 能继续执行：

```c
vm_activate(kernel_table);
current = 0;

bool passed =
    process.finished &&
    process.exit_status == expected_status &&
    process.fault_code == expected_fault;

release_process(&process);
```

顺序很重要：**先切回内核页表，再释放用户进程的页表和物理页。** 不能还让 CPU 使用一张已经归还给分配器的根页表。

整个退出路径可以串成：

```text
用户 ecall，请求 exit
    → 内核记录退出结果
    → 修改 trap_frame，指定返回内核
    → sret 到 user_resume_kernel
    → 恢复进入用户态前的内核调用现场
    → ret，令 user_enter 返回
    → 切回内核页表
    → 检查结果、回收用户资源
```

当前这是一个嵌入式用户程序的测试执行流程，还不是完整的多进程生命周期管理。

确认一下两个返回动作：**从 trap 汇编进入 `user_resume_kernel` 使用的是 `sret`；从 `user_resume_kernel` 回到调用 `user_enter` 的 C 代码，使用的是 `ret`，对吗？**

## User

对

## Assistant

对。这两次返回分别跨过“特权级边界”和“普通函数调用边界”。

用户态这一节还剩下两个保护测试，以及资源回收。走完它们，M3 的主线就完整了。

项目会运行同一份用户程序三次，用传入的 `mode` 选择不同路径：

```c
run_case(kernel_table, 0,  0, 0);
run_case(kernel_table, 1, -1, EXCEPTION_LOAD_PAGE_FAULT);
run_case(kernel_table, 2, -1, EXCEPTION_STORE_PAGE_FAULT);
```

参数中的后两项是预期退出状态和预期异常原因：

| 模式 | 用户程序做什么 | 内核期待什么 |
|---|---|---|
| 0 | `yield`、打印、坏指针检查、正常退出 | 状态 0，无异常 |
| 1 | 读取内核地址 | 读取页故障，状态 -1 |
| 2 | 写入自己的代码页 | 写入页故障，状态 -1 |

模式 1 执行：

```asm
li t0, 0xffffffc080200000
ld t1, 0(t0)
```

这个地址属于内核映射，但没有用户访问权限，所以 U-mode 读取会触发页故障。

模式 2 执行：

```asm
la t0, _start
sd zero, 0(t0)
```

用户代码页有 `U/R/X`，没有 `W`，因此不能修改自己的代码。

这两次不是系统调用。`user_handle_trap` 发现来自 U-mode，但原因不是用户 `ecall`，便执行：

```c
finish_user(frame, -1, code);
return true;
```

它记录失败和异常编号，然后使用刚才学过的路径回到内核。**不会跳过出错指令，让这个用户测试继续运行。**

所以，预期的用户程序错误不会终止整个内核测试；内核收回控制权，核对发生的异常是否正确。

接下来回收资源：

```c
static void release_process(struct user_process *process)
{
    for (size_t i = 0; i < process->page_count; ++i) {
        if (process->pages[i])
            page_free(process->pages[i]);
    }

    vm_destroy_user(&process->page_table);
}
```

`pages` 记录了创建过程中申请的用户代码页和用户栈页，第一段循环负责归还它们。

然后 `vm_destroy_user` 回收用户页表。这里不能直接照搬普通 `vm_destroy`，因为用户根页表的上半部分共享着内核下级页表：

```text
用户根页表
├─ 下半部分 → 用户自己的下级页表
└─ 上半部分 → 内核共享的下级页表
```

所以它只遍历根表的前 256 项：

```c
for (size_t i = 0; i < 256; ++i) {
    uint64_t entry = root[i];

    if (表项有效且指向下一级页表)
        destroy_level(pte_physical(entry), 1);
}

page_free(table->root);
table->root = 0;
```

释放用户自己的下级页表，再释放用户根页表，**不沿上半部分递归释放内核共享页表**。

这样，数据页和页表页的责任仍然分开：

```text
release_process 的循环 → 释放用户代码页、栈页
vm_destroy_user        → 释放用户自己的页表结构
内核共享映射           → 保留
```

最后，`user_run_m3_tests` 对比测试前后的空闲页数：

```c
page_free_count() == baseline
```

确保这些正常退出、异常退出的测试没有遗留已分配页。

至此，我们已经串起用户程序的完整测试生命周期：

```text
准备代码、栈和页表
    → sret 进入 U-mode
    → 系统调用或用户异常
    → 返回内核
    → 检查结果
    → 回收用户资源
```

确认共享资源的边界：**如果 `vm_destroy_user` 也递归释放根表后 256 项指向的页表，会只影响这个用户程序，还是可能破坏内核正在使用的映射？**

## User

后者

## Assistant

对，会破坏内核映射。因为用户根页表里的这些表项，指向的是**同一批内核下级页表**，并没有各自独立的副本。

到这里，启动、内存、用户态、调度和同步的主线已经连起来了。接下来我们补上测试代码中的关键设计：**怎样证明线程真的被抢占了，而不是靠主动 `yield` 才轮流运行？**

项目用了两个非常小的线程。

线程 A：

```c
static void preempt_worker_a(void *argument)
{
    (void)argument;

    while (preempt_b == 0)
        ++preempt_a;
}
```

线程 B：

```c
static void preempt_worker_b(void *argument)
{
    (void)argument;

    while (preempt_a == 0)
        ;

    ++preempt_b;
}
```

测试开始时：

```text
preempt_a = 0
preempt_b = 0
```

注意：**这两个线程的循环都没有调用 `scheduler_yield()`。**

假设 A 先运行：

```text
A 发现 preempt_b == 0
    ↓
不断增加 preempt_a
    ↓
只有 B 能把 preempt_b 改成非零
```

如果没有抢占，A 就会一直占着 CPU，B 没机会运行，A 的循环也无法结束。

有定时器抢占时：

```text
A 循环运行，preempt_a 已经大于 0
    ↓ 定时器中断，切到 B
B 看到 preempt_a 已非零
    ↓
B 增加 preempt_b，然后退出
    ↓ 以后切回 A
A 看到 preempt_b 已非零，退出循环
```

如果 B 先运行也一样：它会在空循环里等 A，必须靠抢占让 A 获得执行机会。

主测试线程这样启动它们：

```c
preempt_a = 0;
preempt_b = 0;
preemptions = scheduler_preemptions();

thread_create("preempt-a", preempt_worker_a, 0);
thread_create("preempt-b", preempt_worker_b, 0);

scheduler_sleep(6);
```

主线程先睡六个 tick，给工作线程运行的机会。醒来后检查：

```c
if (preempt_a == 0 || preempt_b == 0 ||
    scheduler_preemptions() <= preemptions)
    return false;
```

它同时要求：

```text
A 确实执行过
B 确实执行过
定时器调度计数有所增加
```

这比只检查“抢占计数增加了”更有说服力，因为我们之前讲过，计数增加不一定意味着切到了另一个线程。

变量被声明为：

```c
static volatile uint64_t preempt_a;
static volatile uint64_t preempt_b;
```

`volatile` 让循环持续执行相应的读写，避免编译器把条件读取当成不变值。但它不是锁，也不能单靠它构造通用的多核同步协议；这里是当前单 hart 测试中的用法。

最后再看测试的设计意图：**如果为了让循环“更友好”，在 A、B 的循环里各加一句 `scheduler_yield()`，它们仍然可能完成，但还能据此证明定时器抢占有效吗？**

## User

不能

## Assistant

对，加了 `yield` 后，即使抢占失效，也可能靠主动切换完成测试。接下来补上启动流程中略过的 PLIC：它和 SBI 定时器分别负责哪类中断。

## Activity

    $ sed -n '1,220p' src/platform/plic.c && sed -n '1,140p' src/platform/plic.h
    #include "plic.h"
    
    #include <stddef.h>
    
    #include "../mm/layout.h"
    
    /* QEMU virt assigns M/S contexts consecutively for each hart. */
    #define PLIC_ENABLE_BASE UINT64_C(0x2000)
    #define PLIC_ENABLE_STRIDE UINT64_C(0x80)
    #define PLIC_CONTEXT_BASE UINT64_C(0x200000)
    #define PLIC_CONTEXT_STRIDE UINT64_C(0x1000)
    
    static uintptr_t plic_base;
    static uint32_t plic_sources;
    static uint64_t plic_context;
    
    static volatile uint32_t *plic_register(uint64_t offset)
    {
        return (volatile uint32_t *)(plic_base + offset);
    }
    
    bool plic_init(const struct boot_info *info, uint64_t hart_id)
    {
        if (!info || !info->plic_present || info->plic.size < 0x210000 ||
            hart_id > (UINT64_MAX - 1) / 2)
            return false;
        plic_base = (uintptr_t)phys_to_virt(info->plic.base);
        plic_sources = info->plic_source_count;
        plic_context = hart_id * 2 + 1;
        *plic_register(PLIC_CONTEXT_BASE + plic_context * PLIC_CONTEXT_STRIDE) = 0;
        return true;
    }
    
    bool plic_enable(uint32_t source, uint32_t priority)
    {
        volatile uint32_t *enable;
    
        if (plic_base == 0 || source == 0 || source > plic_sources)
            return false;
        *plic_register((uint64_t)source * 4) = priority;
        enable = plic_register(PLIC_ENABLE_BASE + plic_context * PLIC_ENABLE_STRIDE +
                               (source / 32U) * 4U);
        *enable |= UINT32_C(1) << (source % 32U);
        return true;
    }
    
    void plic_disable(uint32_t source)
    {
        volatile uint32_t *enable;
    
        if (plic_base == 0 || source == 0 || source > plic_sources)
            return;
        enable = plic_register(PLIC_ENABLE_BASE + plic_context * PLIC_ENABLE_STRIDE +
                               (source / 32U) * 4U);
        *enable &= ~(UINT32_C(1) << (source % 32U));
    }
    
    uint32_t plic_claim(void)
    {
        if (plic_base == 0)
            return 0;
        return *plic_register(PLIC_CONTEXT_BASE +
                              plic_context * PLIC_CONTEXT_STRIDE + 4);
    }
    
    void plic_complete(uint32_t source)
    {
        if (plic_base != 0 && source != 0)
            *plic_register(PLIC_CONTEXT_BASE +
                           plic_context * PLIC_CONTEXT_STRIDE + 4) = source;
    }
    #ifndef MINICORE_PLATFORM_PLIC_H
    #define MINICORE_PLATFORM_PLIC_H
    
    #include <stdbool.h>
    #include <stdint.h>
    
    #include "dtb.h"
    
    bool plic_init(const struct boot_info *info, uint64_t hart_id);
    bool plic_enable(uint32_t source, uint32_t priority);
    void plic_disable(uint32_t source);
    uint32_t plic_claim(void);
    void plic_complete(uint32_t source);
    
    #endif
    ✓ • 0ms

## Activity

    Searched the web for site.github.com/riscv/riscv-plic-spec priority threshold claim complete pending interrupt

## Assistant

对。加了 `yield`，测试就可能靠主动切换通过，无法证明定时器抢占有效。

接下来补上启动时略过的：

```c
plic_init(&boot_info, hart_id);
```

**PLIC 是外部设备中断控制器。** UART、VirtIO 等设备可以通过它向 CPU 发出中断通知。我们之前使用的定时器中断不走这条路径。

可以先区分：

```text
定时器到期 → 定时器中断
设备有事件 → PLIC → 外部中断
```

PLIC 接收多个设备的请求，根据使能、优先级和目标阈值，决定是否通知相应的 CPU 目标。[PLIC 规范](https://github.com/riscv/riscv-plic-spec/blob/master/riscv-plic.adoc)

项目通过 MMIO 操作 PLIC。辅助函数只是计算寄存器地址：

```c
static volatile uint32_t *plic_register(uint64_t offset)
{
    return (volatile uint32_t *)(plic_base + offset);
}
```

`plic_base` 是内核可访问的虚拟地址；`offset` 是某个 PLIC 寄存器相对设备基址的字节偏移。

初始化的主要动作是：

```c
plic_base = (uintptr_t)phys_to_virt(info->plic.base);
plic_sources = info->plic_source_count;
plic_context = hart_id * 2 + 1;
```

这里的 `context` **不是线程的 `struct context`**，而是 PLIC 的中断目标配置。项目按所用 QEMU `virt` 的布局选择当前 hart 的 S-mode 目标：

```text
hart 0：M-mode context 0，S-mode context 1
hart 1：M-mode context 2，S-mode context 3
```

随后把这个目标的优先级阈值写成零。

但是，**初始化 PLIC 不等于已经开启所有设备中断**。某个中断源还需要单独使能：

```c
plic_enable(source, priority);
```

它先设置源的优先级：

```c
*plic_register((uint64_t)source * 4) = priority;
```

再在目标的使能位图中设置对应位。每个 32 位寄存器管理 32 个源，所以使用：

```text
source / 32 → 找哪个寄存器
source % 32 → 找寄存器中的哪一位
```

例如中断源编号 35：

```text
35 / 32 = 1 → 第二个使能寄存器
35 % 32 = 3 → bit 3
```

禁用则清除同一位：

```c
*enable &= ~(1U << (source % 32U));
```

设备中断真正到来后，处理路径还需要两个动作：

```text
claim：领取一个待处理的中断，得到设备中断源编号
处理：操作对应设备，处理它的事件
complete：通知 PLIC，这次处理已完成
```

项目提供了 `plic_claim()` 和 `plic_complete(source)`。它们读取、写入同一个 claim/complete 寄存器，但读写的语义不同。领取不到待处理中断时，返回编号零。[PLIC 中断领取规则](https://docs.riscv.org/reference/plic/plic-claims.html)

这里要把**已有接口**和**已经接通的运行路径**分清：当前项目初始化了 PLIC，并提供这些操作函数，但 `trap_dispatch` 的中断分支只处理定时器，还没有完整的外部设备中断分发。我们之前的串口输出也采用轮询，没有使用 UART 中断。

因此现在能够打印文字，并不能证明 PLIC 的外部中断处理路径已经工作。

确认一下：**如果 UART 可以通过轮询正常输出，但没有开启 UART 中断源，这两件事矛盾吗？**

## User

不矛盾

## Assistant

对。轮询是 CPU 主动读取设备状态，中断是设备有事件时通知 CPU，两种方式可以独立使用。

现在补上启动时紧接着 PLIC 初始化的一步：

```c
if (!sbi_probe_extension(SBI_EXT_TIME))
    panic("SBI TIME extension is unavailable");
```

它回答的是：**当前 OpenSBI 是否支持我们要使用的定时器服务？**

前面我们学过 `sbi_set_timer` 怎么调用服务；这里是在使用之前先查询能力。

实际实现是：

```c
bool sbi_probe_extension(uint64_t extension_id)
{
    struct sbi_return result =
        sbi_call(SBI_EXT_BASE,
                 SBI_BASE_PROBE_EXTENSION,
                 extension_id, 0, 0);

    return result.error == 0 && result.value != 0;
}
```

它使用 SBI 的基础扩展，而不是直接调用 TIME 扩展：

```text
a7 = 0x10        → 基础扩展 BASE
a6 = 3           → 查询扩展是否存在
a0 = 0x54494d45  → 要查询的扩展：TIME
ecall
```

你可以把它理解成：

> 向 OpenSBI 的基础服务询问：“你有没有 TIME 服务？”

返回的两个字段分工不同：

```c
struct sbi_return {
    long error;
    long value;
};
```

- `error`：这次查询调用是否成功。
- `value`：查询得到的答案，零表示不支持，非零表示支持。

所以函数必须同时判断：

```c
result.error == 0 && result.value != 0
```

这里不是只看 `error == 0` 就够了。**“成功问到了答案”和“答案是支持”是两件事。**

如果不支持，启动代码立刻报告失败，避免继续运行依赖定时器的测试和调度器。

至此，启动阶段这几项的关系就清楚了：

```text
DTB 提供设备与时间计数信息
    ↓
PLIC 初始化外部中断控制器接口
    ↓
SBI 探测确认定时器服务存在
    ↓
trap_init 安装内核异常入口
    ↓
设置定时器、开启中断并运行测试
```

这也解释了为什么内核初始化有顺序要求：设置定时器并允许中断之前，必须先准备好能够处理它的入口。

确认一下：**如果探测返回 `error=0、value=0`，表示查询失败，还是查询成功但不支持 TIME 扩展？**

## User

后者

## Assistant

对，查询成功，但答案是不支持。运行主线已经串起来了，接下来补上它之前的一环：源码怎样构建成 QEMU 能加载的 `kernel.elf`，以及用户程序怎样被装进内核。

## Activity

    $ sed -n '1,36p' Makefile && sed -n '96,150p' Makefile && sed -n '1,100p' user/user.ld
    CROSS ?= riscv64-unknown-elf-
    CC := $(CROSS)gcc
    QEMU ?= qemu-system-riscv64
    GDB ?= gdb-multiarch
    PYTHON ?= python3
    QEMU_MACHINE ?= virt
    QEMU_MEMORY ?= 256M
    
    TARGET := build/kernel.elf
    LINKER_SCRIPT := src/linker/linker.ld
    OBJS := build/main.o build/runtime.o build/dtb.o build/qemu-virt.o \
        build/plic.o build/sbi.o build/trap.o build/page_alloc.o build/vm.o \
        build/heap.o build/sync.o build/log.o build/scheduler.o build/m4-test.o \
        build/user.o build/user-image.o build/context-asm.o build/trap-asm.o build/head.o
    ARCH := -march=rv64imac_zicsr_zicntr -mabi=lp64 -mcmodel=medany
    CFLAGS := $(ARCH) -std=c11 -O2 -g -Wall -Wextra -Werror \
        -ffreestanding -fno-builtin -fno-stack-protector -fno-pie
    
    .PHONY: all run test debug gdb layout clean
    all: $(TARGET)
    
    $(TARGET): $(OBJS) $(LINKER_SCRIPT)
        $(CC) $(ARCH) -nostdlib -static -no-pie -Wl,--build-id=none \
            -T $(LINKER_SCRIPT) $(OBJS) -lgcc -o $@
    
    build/main.o: src/main.c src/arch/riscv/sbi.h src/arch/riscv/trap.h \
            src/kernel/user.h src/platform/dtb.h src/platform/plic.h \
            src/platform/platform.h src/runtime.h
        @mkdir -p build
        $(CC) $(CFLAGS) -c $< -o $@
    
    build/runtime.o: src/runtime.c src/runtime.h
        @mkdir -p build
        $(CC) $(CFLAGS) -c $< -o $@
    
    build/dtb.o: src/platform/dtb.c src/platform/dtb.h src/runtime.h
    
    build/user-program.o: user/user.S
        @mkdir -p build
        $(CC) $(ARCH) -g -c $< -o $@
    
    build/user.elf: build/user-program.o user/user.ld
        $(CC) $(ARCH) -nostdlib -static -no-pie -Wl,--build-id=none \
            -T user/user.ld build/user-program.o -o $@
    
    build/user.bin: build/user.elf
        $(CROSS)objcopy -O binary $< $@
    
    build/user-image.o: build/user.bin
        $(CROSS)objcopy -I binary -O elf64-littleriscv -B riscv \
            --rename-section .data=.rodata.user,alloc,load,readonly,data,contents \
            $< $@
    
    build/trap-asm.o: src/arch/riscv/trap.S
        @mkdir -p build
        $(CC) $(ARCH) -g -c $< -o $@
    
    build/context-asm.o: src/arch/riscv/context.S
        @mkdir -p build
        $(CC) $(ARCH) -g -c $< -o $@
    
    build/head.o: src/arch/riscv/head.S
        @mkdir -p build
        $(CC) $(ARCH) -g -c $< -o $@
    
    run: $(TARGET)
        $(QEMU) -machine virt -m 256M -smp 1 -nographic \
            -bios default -kernel $(TARGET)
    
    test: $(TARGET)
        @set -e; for memory in 256M 32M; do \
            log=build/test-$$memory.log; \
            timeout 20s $(QEMU) -machine virt -m $$memory -smp 1 -nographic \
                -bios default -kernel $(TARGET) >$$log 2>&1; \
            grep -q "M4 PASS" $$log; \
            echo "M4 $$memory PASS"; \
        done
    
    debug: $(TARGET)
        $(QEMU) -machine virt -m 256M -smp 1 -nographic \
            -bios default -kernel $(TARGET) -S -s
    
    gdb: $(TARGET)
        $(GDB) $(TARGET) -ex 'target remote :1234'
    
    layout: $(TARGET)
        $(PYTHON) tools/generate-layout.py \
            --elf $(TARGET) \
            --readelf $(CROSS)readelf \
            --nm $(CROSS)nm \
            --qemu $(QEMU) \
    OUTPUT_ARCH(riscv)
    ENTRY(_start)
    SECTIONS
    {
        . = 0x10000;
        .text : { *(.text .text.*) *(.rodata .rodata.*) }
        /DISCARD/ : { *(.comment) *(.riscv.attributes) }
    }
    ✓ • 0ms

## Assistant

对，查询成功，只是不支持该扩展。

现在补上构建流程。我们之前一直在讲“运行时发生什么”，但这些代码首先需要变成 CPU 能执行的指令。

项目构建内核的主线是：

```text
C 源文件、汇编源文件
    ↓ 编译或汇编
多个 .o 目标文件
    ↓ 按 linker.ld 链接
build/kernel.elf
    ↓ QEMU 加载，OpenSBI 交接
_start
```

例如：

```makefile
$(CC) $(CFLAGS) -c src/main.c -o build/main.o
```

`-c` 表示生成目标文件，暂时不链接。此时 `main.o` 可以记录“这里需要调用 `console_puts`”，但不必已经知道它最终位于哪个地址。

链接阶段把各个目标文件组合起来：

```makefile
$(CC) ... -T src/linker/linker.ld $(OBJS) -lgcc \
    -o build/kernel.elf
```

它根据链接脚本安排代码和数据的位置，并解析跨文件符号引用。例如将 `kernel_main` 对 `console_puts` 的调用，与平台输出文件里的实现连接起来。

内核使用自己的启动代码和基础运行库，而不是普通应用程序的运行环境。这也解释了为什么项目要自己实现 `memcpy`、`memset`，以及为什么入口是 `_start`，不是由系统替它调用 `main`。

**用户程序还有一条额外的构建路径。**

```text
user/user.S
    ↓
build/user-program.o
    ↓ 按 user/user.ld 链接
build/user.elf
    ↓ 提取原始内容
build/user.bin
    ↓ 包装成可参与内核链接的目标文件
build/user-image.o
    ↓
链接进 kernel.elf
```

这里有两个容易混淆的问题。

**第一个：为什么先链接用户程序？**

用户程序也需要确定自己的运行地址。它的链接脚本写着：

```ld
ENTRY(_start)

SECTIONS
{
    . = 0x10000;
    .text : { *(.text .text.*) *(.rodata .rodata.*) }
}
```

这就和内核中的约定对应上了：

```c
#define USER_CODE 0x10000
```

用户程序按 `0x10000` 的地址布局生成；运行时，内核也将代码映射到用户虚拟地址 `0x10000`。

**第二个：为什么又变成 `.bin`，再包装成 `.o`？**

`user.elf` 带有 ELF 头、段信息、符号等元数据。当前内核没有实现用户 ELF 加载器，而是直接复制一段已经准备好的程序内容，所以先通过：

```makefile
objcopy -O binary build/user.elf build/user.bin
```

得到原始二进制内容。

然后再把这段二进制当成**一份数据资产**，包装为链接器能处理的 `user-image.o`，放到内核只读数据区域。

包装过程会提供这些符号，供 C 代码访问：

```c
_binary_build_user_bin_start
_binary_build_user_bin_end
```

于是内核可以计算镜像长度：

```c
image_size = end - start;
```

再申请用户物理页，把这些字节复制过去、建立映射，最后进入 U-mode。

所以同一份用户代码在两个阶段扮演不同角色：

```text
内核镜像中：一段待加载的只读数据
用户地址空间中：映射了执行权限的程序代码
```

把用户二进制链接进内核，并不意味着构建完成后它就自动作为用户程序运行；仍需要我们之前讲过的 `prepare_process → user_enter`。

确认一下：**`user-image.o` 链接进 `kernel.elf` 后，内核是直接把它当普通 C 函数调用，还是复制到用户页、建立用户映射后，通过 `sret` 进入执行？**
