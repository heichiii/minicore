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
