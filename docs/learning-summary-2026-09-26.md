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
