目标：

- RV64GC，QEMU `virt`
- C11 freestanding，少量 RISC-V 汇编
- OpenSBI 提供 M-mode 运行环境，内核运行在 S-mode
- Sv39 虚拟内存
- Unix-like 进程、文件描述符、VFS
- VirtIO 块设备
- 单核完成后再启用 SMP
- 初期静态链接用户程序，后期再扩展动态链接

QEMU `virt` 提供 UART、CLINT/ACLINT、PLIC、VirtIO-MMIO、设备树等设备，很适合作为唯一首发平台。[QEMU virt 官方文档](https://www.qemu.org/docs/master/system/riscv/virt.html)

## 总体依赖关系

```
构建系统 / ABI / 链接布局
              │
OpenSBI启动 ─ 平台描述(DTB) ─ Early Console
              │
     CSR / Trap / Interrupt / Timer
              │
         物理内存管理
              │
     Sv39页表与内核虚拟地址空间
              │
      内核堆 / Slab / 内核对象
              │
同步原语 ─ 上下文切换 ─ 调度器
              │
       进程 / 用户地址空间
              │
       系统调用 / 文件描述符
              │
设备模型 ─ VirtIO块设备 ─ Buffer Cache
              │
             VFS
              │
       文件系统 / ELF / init
              │
       SMP / 网络 / 高级功能
```

这条路线不会实现之后注定被推翻的“临时架构”。允许存在早期 bump allocator、轮询串口等启动设施，但它们应明确属于 boot/early 子系统，而不是冒充最终实现。

------

## 阶段 0：先冻结系统边界

在写代码前确定以下契约：

- 内核由 OpenSBI 进入 S-mode，不自行实现完整 M-mode firmware。
- SBI 调用封装为独立 `sbi` 层，并在启动时探测扩展，不假设 QEMU 内置 OpenSBI 一定支持最新版 SBI。SBI 使用 `ecall`，v0.2 之后以 `a7/a6` 传递 extension/function ID。[SBI 规范仓库](https://github.com/riscv-non-isa/riscv-sbi-doc)
- 使用 LP64 ABI。
- 用户态 ABI 自己定义，初期只追求内部稳定，不必立即兼容 Linux。
- 页大小固定 4 KiB。
- 内核采用高地址映射，物理内存保留直接映射区。
- 平台信息优先来自 DTB，不把所有地址散落成宏。
- 错误处理统一使用负错误码或明确的 `status + out parameter`。
- 所有内核对象明确生命周期、所有权和锁保护规则。

这一阶段应产出：

```
docs/
  architecture.md
  memory-layout.md
  syscall-abi.md
  locking.md
  boot-protocol.md
```

------

## 阶段 1：构建、链接和启动链

实现：

- `riscv64-unknown-elf` 或等价交叉工具链
- freestanding C runtime
- linker script
- `.text/.rodata/.data/.bss` 布局
- `_start` 汇编入口
- 设置 boot stack、清零 BSS、跳入 `kernel_main`
- 保存 OpenSBI 传入的 `hartid` 和 DTB 地址
- `memcpy/memset/memmove/strlen` 等最小运行库
- panic、assert、符号化前的地址回溯
- early console
- QEMU 自动退出机制，供测试使用

推荐启动方式：

```
qemu-system-riscv64 \
  -machine virt \
  -m 256M \
  -smp 1 \
  -nographic \
  -bios default \
  -kernel build/kernel.elf
```

验收标准：

- 每次启动可稳定进入 `kernel_main`
- 能打印段地址、hart ID、DTB 地址
- panic 后不会继续执行
- QEMU 可用明确的成功/失败退出码结束
- GDB 能在 `_start` 和 `kernel_main` 下断点

------

## 阶段 2：RISC-V 架构核心层

这一层只负责 CPU 和特权架构，不依赖进程、文件系统。

实现模块：

```
arch/riscv/
  entry.S
  csr.h
  trap.S
  trap.c
  context.S
  sbi.c
  irq.c
  timer.c
  cpu.c
```

内容包括：

- CSR 访问封装
- `sstatus/sie/sip/stvec/sepc/scause/stval/satp`
- trap frame 的固定 ABI
- 异常与中断的统一入口/分发
- S-mode timer
- software interrupt/IPI 接口占位
- PLIC 初始化和 claim/complete
- 中断嵌套策略
- 保存/恢复通用寄存器
- FPU 状态策略：暂不支持时明确禁用或 lazy trap，不能默默破坏状态

RISC-V S-mode 和 Sv39 的行为应直接以特权规范为准。[RISC-V Supervisor ISA](https://docs.riscv.org/reference/isa/priv/supervisor.html)

验收标准：

- 能主动触发并识别 illegal instruction、breakpoint、访问异常
- `stval/sepc/scause` 输出正确
- 时钟中断连续触发
- 外部中断能经过 PLIC 完成 claim/complete
- trap 返回后寄存器内容不被破坏

------

## 阶段 3：物理内存管理

先有可靠的物理页管理，之后再构造最终虚拟内存体系。

建议分层：

1. 根据 DTB 获取 RAM 范围。
2. 排除内核、DTB、固件及保留区域。
3. early bump allocator，用于页表和正式分配器初始化。
4. buddy allocator 或“按阶连续页分配器”。
5. 每页元数据 `struct page`。
6. 页引用计数和 flags。
7. 调试模式下的 poisoning、double-free 检查。

核心接口示例：

```
struct page *page_alloc(unsigned order, unsigned flags);
void page_free(struct page *page, unsigned order);

uintptr_t page_to_pa(const struct page *);
struct page *pa_to_page(uintptr_t pa);
```

这里不要直接把物理页分配和 `malloc` 混在一起。

验收标准：

- 随机分配/释放测试后空闲页数量恢复
- 能分配不同 order 的连续页
- 保留区永远不会被分配
- double free、越界 order 能被检测
- 在小内存参数下也能启动，例如 `-m 32M`

------

## 阶段 4：Sv39 虚拟内存

物理页管理稳定后，实现最终页表层：

- Sv39 三层页表
- 页表页分配和回收
- map/unmap/protect/query
- `sfence.vma`
- ASID 接口预留
- 内核高地址映射
- 物理内存 direct map
- MMIO 映射区
- 内核栈 guard page
- 用户/内核权限隔离
- 页错误分类

建议划分：

```
mm/
  page.c       // 物理页
  pagetable.c  // RISC-V PTE操作
  vmap.c       // 内核虚拟地址空间
  vmspace.c    // 用户地址空间
```

不要让驱动直接操作 PTE，也不要在通用 VM 代码里散布 RISC-V 位定义。

验收标准：

- 开启 `satp` 后内核继续执行
- `.text` 不可写，`.rodata` 不可写
- unmapped/guard page 访问必定 trap
- map → write → unmap → fault 测试通过
- 页表销毁后所有页表页均被回收

------

## 阶段 5：内核内存和基础对象

在虚拟内存之上实现：

- slab/size-class allocator
- `kmalloc/kfree`
- 大对象走 page allocator
- intrusive list
- bitmap
- reference counting
- object ID/generation
- per-CPU 数据结构框架
- 内核日志 ring buffer

建议禁止无边界的通用分配习惯。调度器、进程和 VFS 的核心对象最好使用专用 cache：

```
kmem_cache_create("task", sizeof(struct task), ...);
kmem_cache_create("vm_area", sizeof(struct vm_area), ...);
```

验收标准：

- 多尺寸随机分配压力测试
- 分配失败路径不会破坏 allocator
- 泄漏统计可用
- UAF/double-free 在 debug build 中可尽早暴露

------

## 阶段 6：并发、上下文和调度

依赖 trap、timer、内存分配已经完整。

实现顺序：

- interrupt disable/restore
- spinlock
- 原子操作
- wait queue
- completion
- mutex
- kernel thread
- 上下文切换
- scheduler
- sleep/wakeup
- timer queue
- 抢占控制

第一版调度器直接实现完整的优先级 round-robin 或简洁的多级队列，不需要先做“只能手动 yield 的教学调度器”。但 boot idle task 可以作为特殊内核线程存在。

必须提前规定：

- 哪些上下文允许睡眠
- 中断上下文禁止哪些操作
- 锁顺序
- 持有 spinlock 时的中断状态
- 调度点和抢占点
- task 的引用计数与退出回收

验收标准：

- 多内核线程抢占运行
- sleep 不占用 CPU
- producer/consumer 压力测试
- 高频时钟中断下无丢失唤醒
- 锁依赖检查或 lock assertion 可用

------

## 阶段 7：进程与用户地址空间

实现：

- `struct process` 与 `struct thread` 分离
- 独立 Sv39 用户地址空间
- 用户栈
- trap frame
- U-mode 进入与返回
- 用户指针检查
- `copy_from_user/copy_to_user`
- `fork` 的基础语义
- `exec`
- `exit/wait`
- PID 管理
- 父子关系
- signal 暂缓，但接口留出位置

内存区域使用 VMA 表达：

```
struct vm_area {
    uintptr_t start;
    uintptr_t end;
    unsigned prot;
    unsigned flags;
    struct vm_object *object;
};
```

可先实现 eager-copy `fork`，之后添加 COW。这里的 eager-copy 是同一套 VM 架构下的策略替换，不是推翻式重写。

验收标准：

- U-mode 无法访问内核映射
- 用户空指针、越界指针不会导致内核崩溃
- 多进程地址空间互不污染
- `fork/exec/exit/wait` 生命周期闭合
- 进程退出后页表、物理页、内核栈均被回收

------

## 阶段 8：系统调用和文件描述符层

系统调用入口依赖用户态 trap 和进程模型。

第一组 syscall 建议：

```
process: exit, fork, exec, waitpid, getpid, yield
memory:  brk, mmap, munmap
fd:      openat, close, read, write, dup, pipe
fs:      fstat, getdents, mkdirat, unlinkat, chdir
time:    clock_gettime, nanosleep
```

架构上分开：

```
syscall ABI
    ↓
参数验证 / copyin / copyout
    ↓
进程、VM、VFS等内核接口
```

syscall 层不要直接调用具体文件系统或设备驱动。

------

## 阶段 9：设备模型、VirtIO 与块缓存

设备栈的正确依赖关系是：

```
DTB
 ↓
platform bus
 ↓
device / driver
 ↓
virtio-mmio transport
 ↓
virtqueue
 ↓
virtio-blk
 ↓
generic block layer
 ↓
buffer/page cache
```

实现内容：

- 最小 DTB parser
- 设备/驱动匹配
- MMIO 访问屏障
- DMA 内存规则
- VirtIO feature negotiation
- split virtqueue
- descriptor 生命周期
- 中断完成与等待队列
- 通用 block request
- buffer cache
- 写回和同步语义

先写 block layer，再让文件系统依赖它。不要让文件系统直接操作 VirtIO descriptor。

验收标准：

- 设备特性协商失败时安全退出
- 能进行跨 sector、非对齐的上层读写
- 并发 block request 正确完成
- descriptor 不泄漏
- 强制重启后，已 `fsync` 的数据符合预期

------

## 阶段 10：VFS 和文件系统

推荐先建立 VFS 对象模型，再实现具体文件系统：

```
struct vnode
struct file
struct dentry
struct superblock
struct mount
struct file_ops
struct vnode_ops
```

VFS 应支持：

- 路径解析
- `.` 与 `..`
- mount point
- cwd/root
- 文件描述符表
- pipe
- device node
- page/buffer cache 集成
- 并发打开与引用计数

具体文件系统可以选择：

- 自己设计一个小型 inode 文件系统：学习价值高
- FAT32：方便与宿主机交换文件
- ext2：结构清晰，Unix 语义更自然

如果最终目标偏 Unix-like，我会优先选择“只读 initramfs + 自研简单磁盘 FS”，随后再做 FAT32 或 ext2。这样启动文件与持久化文件系统彼此解耦。

------

## 阶段 11：ELF、init 与用户空间

实现：

- ELF64 loader
- program header 权限映射
- argv/envp/auxv 栈布局
- initramfs
- `/init`
- libc-like 用户库
- syscall wrappers
- shell
- 基础命令

用户态建议单独维护：

```
user/
  libc/
  crt/
  init/
  sh/
  bin/
```

验收标准：

- 内核只负责启动 `/init`
- init 可以启动并回收 shell/子进程
- shell 能运行外部 ELF
- 管道和重定向工作
- 用户程序崩溃不会拖垮内核

------

## 阶段 12：SMP

SMP 放在这里不是因为它“不重要”，而是它依赖：

- 稳定的每 CPU 数据
- 中断控制
- 内存分配
- 页表/TLB 刷新协议
- 调度器
- 锁和原子操作
- task 生命周期

实现：

- SBI HSM 启动 secondary hart
- per-hart stack
- per-CPU idle task
- IPI
- remote TLB shootdown
- SMP scheduler
- run queue locking
- cross-CPU wakeup
- RCU 可留到更后面

验收标准：

```
-smp 1
-smp 2
-smp 4
-smp 8
```

全部能通过相同压力测试；不能把“单核关中断”等价于全局互斥。

------

## 阶段 13：可靠性和高级功能

基础系统闭环后再做：

- Copy-on-write fork
- demand paging
- shared memory
- signal
- poll/select/epoll
- tty line discipline
- VirtIO 网络
- TCP/IP
- SMP 性能优化
- 内核符号和 stack unwinder
- core dump
- kernel sanitizer
- fuzz syscall/VFS/文件系统
- POSIX 兼容层
- Linux syscall 子集兼容

## 推荐仓库结构

```
mini-os/
├── Makefile
├── linker/
├── include/
│   ├── kernel/
│   ├── arch/
│   └── uapi/
├── arch/riscv/
├── platform/qemu-virt/
├── kernel/
│   ├── sched/
│   ├── process/
│   ├── syscall/
│   ├── sync/
│   └── time/
├── mm/
├── drivers/
│   ├── irqchip/
│   ├── serial/
│   ├── virtio/
│   └── block/
├── fs/
├── lib/
├── user/
├── tests/
└── docs/
```

依赖方向必须保持单向：

```
arch/platform → mm/kernel core → device/block → VFS/FS → syscall
```

尤其避免：

- `mm` 依赖 VFS
- 调度器依赖具体驱动
- syscall 直接依赖 VirtIO
- 文件系统直接操作 MMIO
- 通用内核代码包含 QEMU 固定地址

## 推荐里程碑

| 里程碑 | 可观察成果                      |
| ------ | ------------------------------- |
| M0     | OpenSBI 启动、串口、panic、GDB  |
| M1     | trap、PLIC、timer、DTB          |
| M2     | 物理页、Sv39、内核堆            |
| M3     | 内核线程、同步、抢占调度        |
| M4     | U-mode、进程、用户内存、syscall |
| M5     | VirtIO block、缓存、VFS         |
| M6     | 文件系统、ELF、init、shell      |
| M7     | SMP 与跨核 TLB                  |
| M8     | COW、signal、网络或兼容性扩展   |

这套路线的第一个“完整系统”出现在 M6：它已经拥有稳定的底层架构，而不是由多个教学阶段拼出来的临时实现。rCore 的章节仍然适合用来理解机制，但你的代码组织和实施顺序应以这里的依赖图为准。[rCore Tutorial V3](https://github.com/rcore-os/rCore-Tutorial-v3)