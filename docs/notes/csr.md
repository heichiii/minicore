总结一下，RISC-V 的 **CSR（Control and Status Register）** 是 CPU 内部用于控制特权级、中断、异常、虚拟内存和性能计数的特殊寄存器。

最核心的可以记成这几组：

| CSR        | 作用                                                |
| ---------- | ------------------------------------------------- |
| `mstatus`  | M 模式全局状态，包含全局中断使能 `MIE`、之前中断状态 `MPIE`、之前特权级 `MPP` |
| `mie`      | 各类中断使能，如软件、定时器、外部中断                               |
| `mip`      | 各类中断是否处于 pending 状态                               |
| `mtvec`    | Trap 处理入口地址                                       |
| `mepc`     | 发生 Trap 时保存相关 PC                                  |
| `mcause`   | Trap 的原因，是异常还是中断                                  |
| `mtval`    | Trap 的附加信息，如出错地址                                  |
| `mscratch` | Trap handler 可自由使用的临时 CSR                         |
| `medeleg`  | 将异常委托给 S-mode                                     |
| `mideleg`  | 将中断委托给 S-mode                                     |
| `satp`     | 控制页表和虚拟地址转换                                       |
| `mhartid`  | 当前 hart 的 ID                                      |

CSR 与普通寄存器的区别：

```text
x0 ~ x31
    ↓
程序计算数据

CSR
    ↓
控制 CPU 自身状态
```

CSR 通过专用指令访问：

```asm
csrr t0, mcause      # 读
csrw mtvec, t0       # 写
csrs mstatus, t0     # 设置某些 bit
csrc mstatus, t0     # 清除某些 bit
```

中断最重要的关系是：

```text
mip
 │
 │ 中断是否 pending
 ▼
mie
 │
 │ 该类中断是否允许
 ▼
mstatus.MIE
 │
 │ 全局中断是否允许
 ▼
CPU 接受中断
 │
 ▼
mtvec
 │
 ▼
trap handler
```

因此可以简单记成：

```text
mip     = 有没有中断
mie     = 要不要这个中断
mstatus = 总开关
mtvec   = 去哪里处理
mcause  = 为什么进来
mepc    = 从哪里进来
mtval   = 出错的附加信息
```

对于 QEMU `riscv64 virt` 的 PLIC 外部中断，流程大致是：

```text
UART / VirtIO
     │
     ▼
    PLIC
     │
     ▼
mip.MEIP = 1
     │
     ▼
mie.MEIE = 1 ?
     │
     ▼
mstatus.MIE = 1 ?
     │
     ▼
   mtvec
     │
     ▼
trap_handler
     │
     ├─ mcause：Machine External Interrupt
     │
     ├─ PLIC claim：获得具体 IRQ
     │
     └─ PLIC complete
     │
     ▼
    mret
```

一句话记忆：

> **CSR 是 RISC-V CPU 的控制面；`mstatus/mie/mip` 管中断开关，`mtvec/mepc/mcause/mtval` 管 Trap，`medeleg/mideleg` 管特权级委托，`satp` 管虚拟内存。**
