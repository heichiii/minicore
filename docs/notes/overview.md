*本项目是一个跑在qemu riscv64 virt的mini os.*

# 外部条件

qemu virt提供了许多MMIO.RAM起始地址是0x8000 0000.

qemu完成了opensbi的加载运行，opensbi把控制流交给0x8020 0000. qemu也完成我们kernel程序的加载。

# 进入OS

## 如何进入？

opensbi执行完后会跳转到0x8020 0000, 如何确保我们的程序被qemu正确加载到这个地址？

qemu会根据elf中段的物理地址将其加载到对应位置，所以我们需要在链接器脚本中设置好地址从而让elf中对应段的物理地址是0x8020 0000.

## 初始内存布局

在执行第一条指令前，我们有必要知道当前的内存布局。

```
celestee@Celestee-debian:~/prj/minicore$ readelf -l build/kernel.elf 

Elf file type is EXEC (Executable file)
Entry point 0x80200000
There are 5 program headers, starting at offset 64

Program Headers:
  Type           Offset             VirtAddr           PhysAddr
                 FileSiz            MemSiz              Flags  Align
  LOAD           0x0000000000001000 0x0000000080200000 0x0000000080200000
                 0x00000000000000b2 0x00000000000000b2  R E    0x1000
  LOAD           0x0000000000000000 0x0000000080201000 0x0000000080201000
                 0x0000000000000000 0x0000000000001000  RW     0x1000
  LOAD           0x0000000000002000 0xffffffc080202000 0x0000000080202000
                 0x00000000000325a7 0x00000000000325a7  R E    0x1000
  LOAD           0x0000000000035000 0xffffffc080235000 0x0000000080235000
                 0x0000000000000020 0x0000000000006000  RW     0x1000
  GNU_STACK      0x0000000000000000 0x0000000000000000 0x0000000000000000
                 0x0000000000000000 0x0000000000000000  RW     0x10

 Section to Segment mapping:
  Segment Sections...
   00     .text.boot 
   01     .bss.boot 
   02     .text .rodata .srodata.cst8 .srodata 
   03     .data .bss 
   04    
```

通过readelf我们可以看到，四个LOAD段会被加载。

1. .text.boot节只包含最初的启动程序。
2. .bss.boot节只包含存储启动临时页表的4KiB字节内存。
3. .text .rodata .srodata.cst8 .srodata 是编译器生成的普通代码使用的只读区域。
4. .data .bss 是编译器生成的普通代码使用的读写区域。其中，链接器脚本还在.bss末尾预留了12K的启动栈空间。

## 执行启动准备

head.S中的`_start`被链接器脚本标记为入口，因此最先执行。`_start`先把opensbi传来的hart ID参数和DTB地址参数从a0 a1寄存器转移到s0 s1寄存器（因为按照 RISC-V 调用约定，`a0-a7` 是**参数/返回值寄存器，也是 caller-saved 寄存器**，后续一旦调用函数，它们很可能被覆盖。而 `s0-s11` 是**callee-saved 寄存器**，被调用函数必须保证返回时恢复原值，所以更适合长期保存启动参数）。

清空临时页表所在的内存。在页表内存直接写页表项（直接使用1GiB的大页），把起始的4GiB内存分别恒等映射到原位置和映射到加0xffffffc000000000后的高地址。写完页表后启用MMU,使用刚写好的临时页表。跳转到.text节的高地址代码（链接器脚本把普通节放在高地址，从readelf也能看到）。

把sp指针设为启动栈顶。

清空bss内存。

然后使用hart ID参数和DTB地址参数调用kernel_main.

# kernel main

进入kernel main就进入了C语言。

## 串口输出

kernel main通过串口输出信息，实际是直接写UART0，MMIO的内存。正常来说，S mode的内核不能直接操作硬件，M mode的sbi程序才可以，但是这里opensbi设置的PMP规则允许了直接访问。

## DTB解析

DTB（Device Tree Blob）是设备树的二进制格式，由DTS编译得到。DTB 本质上是一块连续的二进制内存（大端存储，但riscv是小端读取，所以需要手动大端读取），整体格式是：

```
+---------------------------+
| FDT Header                |
+---------------------------+
| Memory Reservation Block  |
+---------------------------+
| Structure Block           |
+---------------------------+
| Strings Block             |
+---------------------------+
```

### FDT Header

```c
struct fdt_header {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
};
```

头部解析完成之后需要核对各字段是否正确或是否支持。

### Memory Reservation Block

*描述应被保留而不是被OS分配的内存。*

格式非常简单，就是连续的：

```c
struct fdt_reserve_entry {
    uint64_t address;
    uint64_t size;
};		
```

例如：

```
address = 0x80000000
size    = 0x10000

address = 0x81000000
size    = 0x20000

address = 0
size    = 0
```

最后：

```
address = 0
size = 0
```

表示结束。

### Structure Block

格式示意：

```
FDT_BEGIN_NODE
    "/"

    FDT_PROP
        compatible

    FDT_BEGIN_NODE
        "memory@80000000"

        FDT_PROP
            device_type

        FDT_PROP
            reg

    FDT_END_NODE

    FDT_BEGIN_NODE
        "uart@10000000"

        FDT_PROP
            compatible

        FDT_PROP
            reg

    FDT_END_NODE

FDT_END_NODE

FDT_END
```

当前只关心五种节点：

```
 节点类型    识别依据                                   写入 boot_info 的信息
━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 RAM         device_type = "memory"                     memory[] 中的物理地址和大小
──────────  ─────────────────────────────────────────  ─────────────────────────────────
 保留内存    /reserved-memory 的直属子节点，且有 reg    reserved[] 中的地址和大小
──────────  ─────────────────────────────────────────  ─────────────────────────────────
 UART        compatible 匹配支持的 UART                 MMIO 地址、大小、IRQ
──────────  ─────────────────────────────────────────  ─────────────────────────────────
 PLIC        compatible 匹配支持的 PLIC                 MMIO 地址、大小、中断源数量
──────────  ─────────────────────────────────────────  ─────────────────────────────────
 VirtIO      compatible = "virtio,mmio"                 多个设备的 MMIO 地址、大小、IRQ

```

## PLIC init

*PLIC（Platform-Level Interrupt Controller）是 RISC-V 系统里常见的**外部中断控制器**。它负责把来自 UART、VirtIO、网卡等外设的中断，按优先级和目标 hart，转发给 CPU。*

通过解析DTB,我们拿到了PLIC地址等信息，init将信息记录在变量，在PLIC context写入0,允许非零优先级中断通过。（这里只是PLIC允许中断通过，但是产生中断还需要外设本身配置和通过PLIC后CPU开启中断接收。）

## Trap init



```c
    csr_clear_sstatus(SSTATUS_SIE);
    csr_clear_sie(UINT64_MAX);
    csr_write_stvec((uintptr_t)trap_entry);
```

第一句清除 sstatus.SIE，让 CPU 在当前 S-mode 执行期间暂时不响应 S-mode 中断，确保设置入口时不会被中断打断。这不会禁止同步异常，例如非法指令仍然会触发 trap。

第二句清除 sie 中可写的中断使能位，例如：

 位      控制的中断
━━━━━━  ━━━━━━━━━━━━━━━━━━━━━
 SSIE    S-mode 软件中断
──────  ─────────────────────
 STIE    S-mode 定时器中断
──────  ─────────────────────
 SEIE    S-mode 外部设备中断

第三句设置trap入口，中断/异常触发后保存完信息会跳转到制定的地址。

## Page Allocator init

*当前页表仍然使用临时建立的大页页表，为了后续建立4KiB的正常页表，需要知道可分配的空闲内存范围，因此需要读取解析得来的设备树结构，收集可分配内存。*

遍历RAM,排除保留区域，把空闲4KiB串成空闲链表。

## vm build kernel

从空闲页链表申请一个页，清空，作为根页表。对DTB给出的所有RAM,按页进行三级映射，标明保护权限。

vm_activate启用新页表。

## Heap init

*页有了，可以动态申请页，但是页单位是4KiB,需要更小的单位支持更灵活的动态内存申请，于是需要实现堆。*

采用classes slab机制分配内存。支持16,32,64,128,256,512,1024,2048八种大小，通过链表连起来。对于每种大小，动态申请页，对页按块大小切分，使用空闲链表串起来。关中断与锁配合完成内存分配回收。

## log init

环形缓冲区，锁同步读写。

## Scheduler init

完善bootstrap线程，将current设为bootstrap.把bootstrap加入all_threads链表。创建idle线程，加入all_threads链表。开启定时器。
