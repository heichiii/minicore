# TODO

1. M1 test详情

# Overview

项目构建阶段和运行阶段都很重要。

# 构建

## 编译选项解释

1. -march=rv64imac_zicsr_zicntr ：

   指定生成代码所允许使用的 RISC-V 指令集：

     - rv64：64 位 RISC-V。
     - i：基础整数指令集。
     - m：整数乘法和除法指令。
     - a：原子操作指令。
     - c：16 位压缩指令。
     - zicsr：CSR 控制状态寄存器访问指令，如 csrr、csrw。
     - zicntr：基础计数器，如 cycle、time、instret。

2. -mabi=lp64 ：

   使用 RISC-V LP64 ABI：

     - long 和指针为 64 位。
     - int 为 32 位。
     - 函数参数和返回值遵循 64 位整数调用约定。
     - 不使用浮点参数寄存器，是软浮点 ABI。

     所有相互链接的目标文件必须采用兼容 ABI，不能混入 lp64d 等目标文件。

3. -mcmodel=medany：

   使用 RISC-V medany 代码模型：

     - 主要通过 PC 相对寻址访问代码和静态数据。
     - 程序可以放在地址空间中的不同区域，不要求位于低地址。
     - 静态符号通常需要位于同一个约 2 GiB 可寻址范围内。

     这对当前位于高半地址的内核很重要；链接脚本把主体映射到 0xffffffc0... 区域。

4. -std=c11 ：按 ISO C11 标准编译 C 源码。相比 gnu11，不会默认开放所有 GNU 语言扩展

5. -O2 

   启用较强、但一般不会显著增加代码体积的优化，包括：

     - 消除无用代码；
     - 常量传播；
     - 函数内联；
     - 循环和分支优化；
     - 寄存器分配优化。

     它不会像 -O3 那样激进。调试时局部变量仍可能被优化掉。

6. -g ：在目标文件和 ELF 中生成调试信息，供 GDB、objdump 等工具使用。  -g 可以和 -O2 同时使用，但优化后的执行顺序可能与源码不完全一致。

7. -Wall ：开启一组常见且价值较高的警告，例如：

     - 未初始化或未使用的部分变量；
     - 可疑控制流；
     - 部分类型和格式问题。

     它并不表示开启 GCC 的全部警告。

8. -Wextra ：在 -Wall 基础上开启额外警告，例如未使用的函数参数、部分有符号/无符号比较问题。

9. -Werror ：把所有警告当作编译错误，使存在警告的代码无法完成构建。

10. -ffreestanding 

    声明程序运行在 freestanding 环境：

      - 不假定存在完整操作系统；
      - 不假定存在标准 C 库；
      - main 不必是普通宿主程序入口；
      - 宏 __STDC_HOSTED__ 会是 0。

      内核、bootloader 和裸机程序通常都需要它。

11. -fno-builtin ：禁止把 memcpy、strlen、printf 等函数自动视为编译器内建函数。  这样编译器会尊重项目自己提供的运行时实现，避免根据标准库语义擅自替换调用。它和 -ffreestanding 有一定重叠，但显式写出更稳妥。

12. -fno-stack-protector ：关闭栈保护插桩。  否则 GCC 可能生成对 __stack_chk_guard、__stack_chk_fail 的引用，而这个小型内核没有普通 libc 提供这些符号。  代价是没有 canary 对栈溢出的自动检测。

13. -fno-pie：编译阶段不生成 PIE（位置无关可执行文件）所需的代码。  内核地址由自己的链接脚本精确控制，不应受宿主工具链默认 PIE 设置影响。

14. -nostdlib 

    不链接工具链默认提供的：

      - C 启动文件，如 crt0.o；
      - 标准 C 库；
      - 默认系统库；
      - 通常也包括自动添加的 libgcc。

      内核自己提供 _start、运行时和内存布局，所以不能使用宿主环境的启动代码。

15. -static ：要求静态链接，不生成对动态链接器或共享库的依赖。  对于裸机内核这是合理的；在已经使用 -nostdlib 的情况下，它更多是显式保证链接结果不依赖动态环境。

16. -no-pie ：链接阶段生成普通 ELF 可执行文件，而不是 PIE。注意它和 -fno-pie 分工不同：

      - -fno-pie：影响每个源文件如何生成机器码。
      - -no-pie：影响最终 ELF 如何链接。

17. -Wl,--build-id=none ：-Wl,选项 表示把选项传给底层 GNU 链接器 ld。

      --build-id=none 禁止自动生成 .note.gnu.build-id 段，避免出现内核不需要、且可能干扰自定义内存布局的额外段。

18. -T $(LINKER_SCRIPT) ：使用项目自己的链接脚本，而不是 GCC 默认链接布局。

19. -lgcc：显式链接 GCC 编译器运行时库 libgcc。  即使不使用 libc，编译器仍可能需要其中的辅助函数，例如某些除法、移位或其他目标架构辅助操作。因为 -nostdlib 取消了默认库，所以这里需要手动添加。

## 链接器脚本

链接器脚本 `src/linker/linker.ld` 决定内核 ELF 的入口、各个节的地址和顺序，以及需要导出给启动代码和 C 代码的边界符号。它与编译器选项中的 `-T src/linker/linker.ld` 对应。

### 基本声明

```ld
OUTPUT_ARCH(riscv)
ENTRY(_start)
```

- `OUTPUT_ARCH(riscv)` 指定输出 ELF 的目标架构是 RISC-V。
- `ENTRY(_start)` 把 `_start` 设为 ELF 入口。QEMU/OpenSBI 加载内核后会从该符号开始执行。`_start` 定义在 `src/arch/riscv/head.S` 中。

### 物理基址和高半内核

```ld
KERNEL_PHYS_BASE = 0x80200000;
KERNEL_DIRECT_BASE = 0xffffffc000000000;
KERNEL_VIRT_BASE = KERNEL_DIRECT_BASE + KERNEL_PHYS_BASE;
```

- `KERNEL_PHYS_BASE` 是内核镜像的物理加载起点。QEMU `virt` 的 RAM 从 `0x80000000` 开始，OpenSBI 通常将 S-mode 内核放在 `0x80200000`。
- `KERNEL_DIRECT_BASE` 是 Sv39 高半直接映射的基址。物理地址 `pa` 对应的高半虚拟地址是 `KERNEL_DIRECT_BASE + pa`。
- `KERNEL_VIRT_BASE` 因此是内核主体的高半虚拟起点，即 `0xffffffc080200000`。

链接器脚本中需要区分两种地址：

- VMA（Virtual Memory Address）：程序运行时使用的地址，由位置计数器 `.` 决定。
- LMA（Load Memory Address）：ELF 内容被加载到的物理地址，在高半节中由 `AT(...)` 指定。

启动初期 MMU 还没有打开，CPU 只能通过物理地址执行；打开页表后，内核主体则使用高半 VMA。

### 低地址启动节

```ld
. = KERNEL_PHYS_BASE;
__kernel_phys_start = .;
.text.boot : ALIGN(4K) { KEEP(*(.text.boot)) }
.bss.boot (NOLOAD) : ALIGN(4K) { *(.bss.boot) }
```

- `. = KERNEL_PHYS_BASE` 把链接器的位置计数器设为 `0x80200000`。
- `__kernel_phys_start` 记录镜像的物理起点。
- `.text.boot` 收集所有输入目标文件中的 `.text.boot` 节，也就是 `_start` 所在的早期启动代码。它的 VMA 和 LMA 相同，因此 MMU 关闭时也能执行。
- `ALIGN(4K)` 使输出节按 4 KiB 页边界对齐。
- `KEEP(...)` 防止链接器在启用节回收时删除入口代码，即使没有普通符号引用它。
- `.bss.boot` 放置启动根页表 `boot_root_page`。
- `NOLOAD` 表示该节需要占用运行时内存，但 ELF 文件不用保存 4 KiB 的零字节。启动代码会在使用前显式清零这张页表。

### 切换到高半虚拟地址

```ld
. = KERNEL_VIRT_BASE + SIZEOF(.text.boot) + SIZEOF(.bss.boot);
. = ALIGN(4K);
__kernel_start = .;
```

- `SIZEOF(...)` 取前面两个启动节的大小，使高半内核的物理存储位置避开它们。
- 第二个 `ALIGN(4K)` 把内核主体起点向上对齐到页边界。
- `__kernel_start` 是内核主体的高半虚拟起点。

`head.S` 创建临时 Sv39 页表后，会从低地址 `.text.boot` 跳到高半 `.text` 继续执行。临时页表同时保留低地址恒等映射和高半直接映射，使这个过渡可以完成。

### 内核主体的节

```ld
.text : AT(ADDR(.text) - KERNEL_DIRECT_BASE) { ... }
.rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_DIRECT_BASE) { ... }
.data ALIGN(4K) : AT(ADDR(.data) - KERNEL_DIRECT_BASE) { ... }
.bss ALIGN(4K) (NOLOAD) : AT(ADDR(.bss) - KERNEL_DIRECT_BASE) { ... }
```

`ADDR(section)` 返回该节的 VMA。对每个高半节减去 `KERNEL_DIRECT_BASE` 得到 LMA，即：

```text
LMA = VMA - KERNEL_DIRECT_BASE
VMA = KERNEL_DIRECT_BASE + LMA
```

因此 ELF 会把字节加载到 RAM 的低物理地址，但其中的符号和指令按高半虚拟地址链接。

- `.text` 收集 `.text` 和 `.text.*`，存放内核可执行代码。`__text_start` 和 `__text_end` 标记边界。
- `.rodata` 收集 `.rodata` 和 `.rodata.*`，存放字符串、`const` 对象及嵌入的只读用户镜像。它单独按页对齐，便于最终页表设为只读。
- `.data` 收集 `.sdata` 和 `.data` 及其子节，存放已初始化的可写全局数据。
- `.bss` 收集 `.sbss`、`.bss` 和 `COMMON` 符号，存放未初始化或零初始化的全局数据。它也是 `NOLOAD` 节，`head.S` 会使用 `__bss_start` 和 `__bss_end` 在启动时将它清零。

`.text`、`.rodata`、`.data` 和 `.bss` 分页对齐，使正式页表能分别设置 RX、R 和 RW 权限，而不需让一个物理页同时承担不同权限的内容。

### `gp` 全局指针

```ld
__global_pointer$ = MIN(__data_start + 0x800,
                        MAX(__data_start + 0x800, __data_end - 0x800));
```

`__global_pointer$` 供 `head.S` 用来初始化 RISC-V 的 `gp` 寄存器。`gp` 相对寻址的有效偏移大致是 -2048 到 +2047 字节，用于紧凑地访问 `.sdata` 和 `.sbss` 中的小数据。按照当前表达式，该符号的值实际为 `__data_start + 0x800`。

`head.S` 在设置 `gp` 时使用 `.option norelax`，防止链接器把“初始化 `gp`”这条指令反过来松弛为依赖尚未初始化的 `gp` 的指令。

### BSS、保护页和启动栈

```ld
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
```

- `COMMON` 兼容尚未分配到普通 `.bss` 节的共通符号。
- BSS 数据后先对齐到页边界。
- `__boot_stack_guard` 标记一个 4 KiB 的栈保护页。链接器脚本只预留这页地址；正式页表在 `vm.c` 中故意不映射它，从而在栈向下越界时触发页故障。
- `__boot_stack_bottom` 和 `__boot_stack_top` 之间是 12 KiB 启动栈。RISC-V 栈向低地址增长，所以 `sp` 初始化为 `__boot_stack_top`。
- 保护页和栈也属于 `NOLOAD` 的 `.bss`，只增加 ELF 的内存大小，不会在文件中存放成千上万个零字节。

### 内核结束符号

```ld
. = ALIGN(4K);
__kernel_end = .;
__kernel_phys_end = __kernel_end - KERNEL_DIRECT_BASE;
```

- `__kernel_end` 是页对齐后的高半内核结束地址。
- `__kernel_phys_end` 是对应的物理结束地址。
- 内存管理器使用这些符号排除内核已占用的物理页，避免将内核自身覆盖。

### 丢弃不需要的节

```ld
/DISCARD/ :
{
    *(.comment)
    *(.eh_frame .eh_frame.*)
    *(.riscv.attributes)
}
```

- `.comment` 通常保存编译器版本信息。
- `.eh_frame` 是异常展开/栈回溯元数据，当前内核不使用。
- `.riscv.attributes` 记录 RISC-V 对象属性，不需要被加载到运行时内存。

`/DISCARD/` 会从最终 ELF 中删除这些输入节。

### 链接时断言

```ld
ASSERT((__boot_stack_top & 15) == 0, "boot stack must be 16-byte aligned")
ASSERT(SIZEOF(.text.boot) <= 4K, "boot trampoline is too large")
ASSERT(SIZEOF(.bss.boot) == 4K, "bootstrap root must occupy one page")
```

- 启动栈顶必须 16 字节对齐，满足 RISC-V psABI 对函数调用时栈指针的要求。
- `.text.boot` 不能超过一页，保持启动跳板的固定布局。
- `.bss.boot` 必须恰好是 4 KiB，与一张 Sv39 根页表的大小相同。

任一条件不满足时，链接会立即失败，比让错误布局留到启动时更容易定位。

### 用户程序链接脚本

`user/user.ld` 是一个更小的链接脚本：

```ld
OUTPUT_ARCH(riscv)
ENTRY(_start)
SECTIONS
{
    . = 0x10000;
    .text : { *(.text .text.*) *(.rodata .rodata.*) }
    /DISCARD/ : { *(.comment) *(.riscv.attributes) }
}
```

- 用户程序从虚拟地址 `0x10000` 开始运行。
- 当前用户程序只需要可执行代码和只读数据，所以二者被放进同一个 `.text` 输出节。
- 用户 ELF 随后被 `objcopy -O binary` 转成原始镜像，再作为 `.rodata.user` 嵌入内核。
- 加载用户程序时，内核会根据这个链接地址创建用户页表映射。

# 运行

## QEMU启动

加载opensbi、DTB、kernel.elf，地址空间布局：

```text
QEMU virt 物理地址空间（-m 256M，从低地址向高地址）

0x0000000000  +--------------------------------------------------+
              | 未映射                                           |
0x0000001000  +--------------------------------------------------+
              | MROM：QEMU reset vector / 启动 ROM               |
0x0000010000  +--------------------------------------------------+
              | 未映射                                           |
0x0000100000  +--------------------------------------------------+
              | QEMU test finisher：测试成功/失败退出          |
0x0000101000  +--------------------------------------------------+
              | Goldfish RTC                                      |
0x0000101024  +--------------------------------------------------+
              | 未映射                                           |
0x0002000000  +--------------------------------------------------+
              | ACLINT SWI：软件中断                           |
0x0002004000  +--------------------------------------------------+
              | ACLINT MTIMER：时钟中断比较器                    |
0x000200c000  +--------------------------------------------------+
              | 未映射                                           |
0x0003000000  +--------------------------------------------------+
              | PCIe I/O port window                              |
0x0003010000  +--------------------------------------------------+
              | 未映射                                           |
0x000c000000  +--------------------------------------------------+
              | PLIC：外部中断控制器，6 MiB                   |
0x000c600000  +--------------------------------------------------+
              | 未映射                                           |
0x0010000000  +--------------------------------------------------+
              | UART0：16550 串口                               |
0x0010000008  +--------------------------------------------------+
              | 未映射/保留                                    |
0x0010001000  +--------------------------------------------------+
              | VirtIO-MMIO 0                                    |
0x0010001200  +--------------------------------------------------+
              | ... 共 8 个窗口，每隔 0x1000 排列 ...            |
0x0010008000  +--------------------------------------------------+
              | VirtIO-MMIO 7                                    |
0x0010008200  +--------------------------------------------------+
              | 未映射                                           |
0x0010100000  +--------------------------------------------------+
              | FW_CFG：QEMU 固件配置数据/ctl/DMA                |
0x0010100018  +--------------------------------------------------+
              | 未映射                                           |
0x0020000000  +--------------------------------------------------+
              | Flash 0（pflash），32 MiB                         |
0x0022000000  +--------------------------------------------------+
              | Flash 1（pflash），32 MiB                         |
0x0024000000  +--------------------------------------------------+
              | 未映射                                           |
0x0030000000  +--------------------------------------------------+
              | PCIe ECAM / MMCONFIG，256 MiB                    |
0x0040000000  +--------------------------------------------------+
              | PCIe 32-bit MMIO window，1 GiB                   |
0x0080000000  +--------------------------------------------------+
              | RAM 起点；OpenSBI 保留区                      |
0x0080060000  +--------------------------------------------------+
              | OpenSBI 与内核之间的空闲 RAM                    |
0x0080200000  +--------------------------------------------------+ <- _start
              | kernel.elf，内部布局在下图展开              |
0x008020c000  +--------------------------------------------------+ <- __kernel_phys_end
              | 可用 RAM                                          |
0x008fe00000  +--------------------------------------------------+
              | DTB：0x8fe00000..0x8fe017a3                  |
0x008fe017a4  +--------------------------------------------------+
              | RAM 顶部剩余区域                                |
0x0090000000  +--------------------------------------------------+ <- 256 MiB RAM 末端（不包含）
              | 未映射                                           |
0x0400000000  +--------------------------------------------------+
              | PCIe 64-bit MMIO window，16 GiB                  |
0x0800000000  +--------------------------------------------------+ <- 当前 QEMU virt 最高的顶层区域末端
```

上图的区间是 QEMU 10.0.13 默认 `virt` 机器在当前启动参数下的顶层物理映射。“未映射”表示 QEMU 没有在该物理区间注册 RAM、ROM 或 MMIO 设备；访问它通常会产生访问错误。

RAM 中的 `kernel.elf` 进一步展开如下：

```text
0x80200000  +--------------------------------------------------+ <- _start / ELF 入口
            | .text.boot：_start 和 MMU 启动跳板              |
0x80201000  +--------------------------------------------------+ <- boot_root_page
            | .bss.boot：临时 Sv39 根页表，4 KiB          |
0x80202000  +--------------------------------------------------+ <- __kernel_start 的物理地址
            | .text：高半内核代码（物理加载区间）             |
0x80205000  +--------------------------------------------------+ <- __rodata_start
            | .rodata / .srodata / 嵌入的用户镜像          |
0x80206000  +--------------------------------------------------+ <- __data_start
            | .data / .sdata：已初始化可写数据              |
0x80207000  +--------------------------------------------------+ <- __bss_start
            | .bss / .sbss：零初始化数据，NOLOAD             |
0x80208000  +--------------------------------------------------+ <- __boot_stack_guard
            | 栈保护页，4 KiB（正式页表中不映射）           |
0x80209000  +--------------------------------------------------+ <- __boot_stack_bottom
            | 启动栈，12 KiB（向低地址增长）                |
0x8020c000  +--------------------------------------------------+ <- __kernel_phys_end
```

`kernel.elf` 内部还要同时从物理加载地址和运行时虚拟地址两个角度理解：

```text
物理地址（LMA）                         运行时虚拟地址（VMA）

0x80200000  .text.boot / _start       0x0000000080200000  MMU 开启前使用低地址
0x80201000  .bss.boot                 0x0000000080201000  临时根页表

0x80202000  .text          <------->  0xffffffc080202000  高半内核代码
0x80205000  .rodata       <------->  0xffffffc080205000  高半只读数据
0x80206000  .data         <------->  0xffffffc080206000  高半可写数据
0x80207000  .bss          <------->  0xffffffc080207000  高半零初始化数据
0x80208000  guard page    <------->  0xffffffc080208000  虚拟页故意不映射
0x80209000  boot stack    <------->  0xffffffc080209000  启动栈底
0x8020c000  kernel end    <------->  0xffffffc08020c000  内核结束

0x8fe00000  DTB           <------->  0xffffffc08fe00000  direct map 中的 DTB
```

QEMU 根据 ELF Program Header 的 `PhysAddr` 将各段放到 `0x80200000` 起的物理 RAM。OpenSBI 在 MMU 关闭的 S-mode 下跳到 `_start`，并传入 `a0 = hart ID`、`a1 = DTB 物理地址`。`_start` 建立临时页表后，再跳到 `0xffffffc080202000` 起的高半 `.text`。

图中内核各节边界来自当前 `build/kernel.elf`；代码或数据大小变化后，除 `0x80200000` 入口和链接脚本明确固定的对齐关系外，其他边界可能随之变化。DTB 位置也由 QEMU 和 RAM 大小决定；例如 `-m 32M` 时，当前 QEMU 将它放在 `0x81e00000`。

## 控制权交到kernel.elf

### 低地址准备

进入head.S的_start:

把hart id和DTB地址存在寄存器。

把临时页表清零。

在临时页表填写大页页表项，用于4GiB空间的恒等映射和高半区映射。

启动MMU,使用新页表。

跳转到高地址入口点。

### 高地址

设置栈指针到启动栈。

清零BSS.

调用kernel_main.

### main.c

S mode下直接操作UART0输出信息。

解析DTB，拿到硬件地址布局信息。

初始化plic：计算高地址用于正式页表访问。填写一些配置。

初始化trap：关闭、禁用S mode中断，设置trap入口。

执行M1测试。

page_allocator_init，根据DTB检查RAM得到可用页池，串成列表。

vm_build_kernel建立内核页表，映射RAM和用到的硬件。

启用新页表。

run_vm_tests测试页表：

  记录 baseline
      │
      ├─ 分配数据页
      ├─ 创建临时页表
      ├─ map
      ├─ query 地址和权限
      ├─ protect 为只读
      ├─ unmap
      ├─ destroy 页表
      ├─ 释放数据页
      └─ 检查 free_pages == baseline
              │
              ├─ 在活动内核页表 map
              ├─ 真实写入和读回
              ├─ unmap
              ├─ 确认写操作触发 page fault
              ├─ 释放数据页
              ├─ 确认 .text 不可写
              ├─ 确认 guard page 不可读
              └─ 再次检查 free_pages == baseline



user_run_m3_tests：

